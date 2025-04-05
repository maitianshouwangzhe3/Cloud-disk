#include "api_deal_sharefile.h"
#include "log.h"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "api_common.h"
#include "redis_keys.h"
#include <cmath>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

int decodeDealsharefileJson(string &str_json, string &user_name, string &md5, string &filename) {
    rapidjson::Document root;
    root.Parse(str_json.c_str());

    if(root.HasParseError()) {
        return -1;
    }

    if(!root.HasMember("user") && !root["user"].IsString()) {
        return -1;
    }

    user_name = root["user"].GetString();

    if (!root.HasMember("md5") && !root["md5"].IsString()) {
        return -1;
    }
    md5 = root["md5"].GetString();

    if (!root.HasMember("filename") && !root["filename"].IsString()) {
        return -1;
    }
    filename = root["filename"].GetString();
    return 0;
}

int encodeDealsharefileJson(int ret, string &str_json) {
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    writer.StartObject();
    writer.Key("code");
    writer.Int(ret);
    writer.EndObject();
    str_json = buffer.GetString();
    return 0;
}

//文件下载标志处理
int handlePvFile(string &md5, string &filename) {
    int ret = 0;
    char sql_cmd[SQL_MAX_LEN] = {0};
    int ret2 = 0;
    char fileid[1024] = {0};
    int pv = 0;

    CDBManager *db_manager = CDBManager::getInstance();
    CDBConn *db_conn = db_manager->GetDBConn("cloud_disk_slave");
    AUTO_REL_DBCONN(db_manager, db_conn);
    CacheManager *cache_manager = CacheManager::getInstance();
    CacheConn *cache_conn = cache_manager->GetCacheConn("token");
    AUTO_REL_CACHECONN(cache_manager, cache_conn);

    //文件标示，md5+文件名
    sprintf(fileid, "%s%s", md5.c_str(), filename.c_str());

    //===1、mysql的下载量+1(mysql操作)
    // sql语句
    //查看该共享文件的pv字段
    sprintf(
        sql_cmd,
        "select pv from share_file_list where md5 = '%s' and file_name = '%s'",
        md5.c_str(), filename.c_str());
    CResultSet *result_set = db_conn->ExecuteQuery(sql_cmd);
    if (result_set && result_set->Next()) {
        pv = result_set->GetInt("pv");
    } else {
        ret = -1;
        goto END;
    }

    //更新该文件pv字段，+1
    sprintf(sql_cmd,
            "update share_file_list set pv = %d where md5 = '%s' and file_name "
            "= '%s'",
            pv + 1, md5.c_str(), filename.c_str());
    if (!db_conn->ExecuteUpdate(sql_cmd, false)) {
        ret = -1;
        goto END;
    }

    //===2、判断元素是否在集合中(redis操作)
    ret2 = cache_conn->ZsetExit(FILE_PUBLIC_ZSET, fileid);
    if (ret2 == 1) //存在
    {              //===3、如果存在，有序集合score+1
        ret = cache_conn->ZsetIncr(FILE_PUBLIC_ZSET, fileid); // zrange FILE_PUBLIC_ZSET  0 -1 withscores 查看
        if (ret != 0) {
            LOG_ERROR("zsetincr error");
        }
    } else if (ret2 == 0) //不存在
    {                     //===4、如果不存在，从mysql导入数据
        //===5、redis集合中增加一个元素(redis操作)
        ret = cache_conn->ZsetAdd(FILE_PUBLIC_ZSET, pv + 1, fileid);
        if(ret != 0) {
            LOG_ERROR("zsetadd error");
        }

        //===6、redis对应的hash也需要变化 (redis操作)
        //     fileid ------>  filename
        cache_conn->Hset(FILE_NAME_HASH, fileid, filename);
        if(ret!= 0) {
            LOG_ERROR("hset error");
        }
    } else {//出错
        ret = -1;
        LOG_ERROR("ZsetExit error");
        goto END;
    }

END:
    /*
    下载文件pv字段处理
        成功：{"code":0}
        失败：{"code":1}
    */

    if (ret == 0) {
        return HTTP_RESP_OK;
    } else {
        return HTTP_RESP_FAIL;
    }
}

//取消分享文件
int handleCancelShareFile(string &user_name, string &md5, string &filename) {
    int ret = 0;
    char sql_cmd[SQL_MAX_LEN] = {0};
    char fileid[1024] = {0};
    int ret2;

    CDBManager *db_manager = CDBManager::getInstance();
    CDBConn *db_conn = db_manager->GetDBConn("cloud_disk_slave");
    AUTO_REL_DBCONN(db_manager, db_conn);
    CacheManager *cache_manager = CacheManager::getInstance();
    CacheConn *cache_conn = cache_manager->GetCacheConn("token");
    AUTO_REL_CACHECONN(cache_manager, cache_conn);

    //文件标示，md5+文件名
    sprintf(fileid, "%s%s", md5.c_str(), filename.c_str());

    // 共享标志设置为0
    sprintf(sql_cmd,
            "update user_file_list set shared_status = 0 where user = '%s' and "
            "md5 = '%s' and file_name = '%s'",
            user_name.c_str(), md5.c_str(), filename.c_str());

    if (!db_conn->ExecuteUpdate(sql_cmd, false)) {
        ret = -1;
        goto END;
    }

    // 共享文件数量-1
    ret2 = CacheDecrCount(cache_conn, FILE_PUBLIC_COUNT);
    if (ret2 < 0) {
        ret = -1;
        goto END;
    }

    //删除在共享列表的数据
    sprintf(sql_cmd,
            "delete from share_file_list where user = '%s' and md5 = '%s' and "
            "file_name = '%s'",
            user_name.c_str(), md5.c_str(), filename.c_str());
    
    if (!db_conn->ExecuteDrop(sql_cmd)) {
        ret = -1;
        goto END;
    }

    //===2、redis记录操作
    //有序集合删除指定成员
    ret = cache_conn->ZsetZrem(FILE_PUBLIC_ZSET, fileid);
    if (ret != 0) {
        goto END;
    }

    //从hash移除相应记录
    ret = cache_conn->Hdel(FILE_NAME_HASH, fileid);
    if (ret < 0) {
        goto END;
    }

END:
    /*
    取消分享：
        成功：{"code": 0}
        失败：{"code": 1}
    */
    if (ret == 0) {
        return (HTTP_RESP_OK);
    } else {
        return (HTTP_RESP_FAIL);
    }
}

//转存文件
//返回值：0成功，-1转存失败，-2文件已存在
int handleSaveFile(string &user_name, string &md5, string &filename) {
    int ret = 0;
    char sql_cmd[SQL_MAX_LEN] = {0};
    int ret2 = 0;
    //当前时间戳
    struct timeval tv;
    struct tm *ptm;
    char time_str[128];
    int count;
    CDBManager *db_manager = CDBManager::getInstance();
    CDBConn *db_conn = db_manager->GetDBConn("cloud_disk_slave");
    AUTO_REL_DBCONN(db_manager, db_conn);
    CacheManager *cache_manager = CacheManager::getInstance();
    CacheConn *cache_conn = cache_manager->GetCacheConn("token");
    AUTO_REL_CACHECONN(cache_manager, cache_conn);

    //查看此用户，文件名和md5是否存在，如果存在说明此文件存在
    sprintf(sql_cmd,
            "select * from user_file_list where user = '%s' and md5 = '%s' and "
            "file_name = '%s'",
            user_name.c_str(), md5.c_str(), filename.c_str());
    //返回值： 0成功并保存记录集，1没有记录集，2有记录集但是没有保存，-1失败

    // 有记录返回1，错误返回-1，无记录返回0
    ret2 = CheckwhetherHaveRecord(db_conn, sql_cmd);
    if (ret2 == 1) { //如果有结果，说明此用户已有此文件
        ret = -2; //返回-2错误码
        goto END;
    }
    if (ret2 < 0) {
        ret = -1; //返回-1错误码
        goto END;
    }

    //文件信息表，查找该文件的计数器
    sprintf(sql_cmd, "select count from file_info where md5 = '%s'", md5.c_str());
    count = 0;
    ret2 = GetResultOneCount(db_conn, sql_cmd, count); //执行sql语句
    if (ret2 != 0) {
        ret = -1;
        goto END;
    }
    // 1、修改file_info中的count字段，+1 （count 文件引用计数）
    sprintf(sql_cmd, "update file_info set count = %d where md5 = '%s'",
            count + 1, md5.c_str());
    if (!db_conn->ExecuteUpdate(sql_cmd)) {
        ret = -1;
        goto END;
    }

    // 2、user_file_list插入一条数据

    //使用函数gettimeofday()函数来得到时间。它的精度可以达到微妙
    gettimeofday(&tv, NULL);
    ptm = localtime(
        &tv.tv_sec); //把从1970-1-1零点零分到当前时间系统所偏移的秒数时间转换为本地时间
    // strftime()
    // 函数根据区域设置格式化本地时间/日期，函数的功能将时间格式化，或者说格式化一个时间字符串
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", ptm);

    // sql语句
    /*
    -- =============================================== 用户文件列表
    -- user	文件所属用户
    -- md5 文件md5
    -- create_time 文件创建时间
    -- file_name 文件名字
    -- shared_status 共享状态, 0为没有共享， 1为共享
    -- pv 文件下载量，默认值为0，下载一次加1
    */
    sprintf(sql_cmd,
            "insert into user_file_list(user, md5, create_time, file_name, "
            "shared_status, pv) values ('%s', '%s', '%s', '%s', %d, %d)",
            user_name.c_str(), md5.c_str(), time_str, filename.c_str(), 0, 0);
    if (!db_conn->ExecuteCreate(sql_cmd)) {
        ret = -1;
        goto END;
    }

    // 3用户文件数量+1

    // 3、查询用户文件数量，更新该字段
    if (CacheIncrCount(cache_conn, FILE_USER_COUNT + user_name) < 0) {
        ret = -1;
        goto END;
    }
    ret = 0;
END:
    /*
    返回值：0成功，-1转存失败，-2文件已存在
    转存文件：
        成功：{"code":0}
        文件已存在：{"code":5}
        失败：{"code":1}
    */
    if (ret == 0) {
        return (HTTP_RESP_OK);
    } else if (ret == -1) {
        return (HTTP_RESP_FAIL);
    } else if (ret == -2) {
        return (HTTP_RESP_FILE_EXIST);
    }
    return 0;
}

int ApiDealsharefile(string &url, string &post_data, string &str_json) {
    char cmd[20];
    string user_name;
    string token;
    string md5;      //文件md5码
    string filename; //文件名字
    int ret = 0;

    //解析命令
    QueryParseKeyValue(url.c_str(), "cmd", cmd, NULL);

    ret = decodeDealsharefileJson(post_data, user_name, md5, filename);
    if (ret != 0) {
        encodeDealsharefileJson(HTTP_RESP_FAIL, str_json);
        return 0;
    }
    ret = 0;
    if (strcmp(cmd, "cancel") == 0) //取消分享文件
    {
        ret = handleCancelShareFile(user_name, md5, filename);
    } else if (strcmp(cmd, "save") == 0) //转存文件
    {
        ret = handleSaveFile(user_name, md5, filename);
    } else if (strcmp(cmd, "pv") == 0) //文件下载标志处理
    {
        ret = handlePvFile(md5, filename);
    }

    if (ret < 0)
        encodeDealsharefileJson(HTTP_RESP_FAIL, str_json);
    else
        encodeDealsharefileJson(HTTP_RESP_OK, str_json);

    return 0;
}
