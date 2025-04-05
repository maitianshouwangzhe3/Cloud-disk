#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "redis_keys.h"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "api_common.h"
#include "db_pool.h"
#include <sys/time.h>
#include <time.h>

enum Md5State {
    Md5Ok = 0,
    Md5Failed = 1,
    Md5TokenFaild = 4,
    Md5FileExit = 5,
};

int decodeMd5Json(string &str_json, string &user_name, string &token, string &md5, string &filename) {
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

    if (!root.HasMember("token") && !root["token"].IsString()) {
        return -1;
    }
    token = root["token"].GetString();

    return 0;
}

int encodeMd5Json(int ret, string &str_json) {
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    writer.StartObject();
    writer.Key("code");
    writer.Int(ret);
    writer.EndObject();
    str_json = buffer.GetString();
    return 0;
}

//秒传处理
void handleDealMd5(const char *user, const char *md5, const char *filename,
                   string &str_json) {
    Md5State md5_state = Md5Failed;
    int ret = 0;
    int file_ref_count = 0;
    char sql_cmd[SQL_MAX_LEN] = {0};

    CDBManager *db_manager = CDBManager::getInstance();
    CDBConn *db_conn = db_manager->GetDBConn("cloud_disk_slave");
    AUTO_REL_DBCONN(db_manager, db_conn);
    CacheManager *cache_manager = CacheManager::getInstance();
    CacheConn *cache_conn = cache_manager->GetCacheConn("token");
    AUTO_REL_CACHECONN(cache_manager, cache_conn);

    // sql 语句，获取此md5值文件的文件计数器 count
    sprintf(sql_cmd, "select count from file_info where md5 = '%s'", md5);
    //返回值： 0成功并保存记录集，1没有记录集，2有记录集但是没有保存，-1失败
    file_ref_count = 0;
    ret = GetResultOneCount(db_conn, sql_cmd, file_ref_count); //执行sql语句
    if (ret == 0) //有结果, 并且返回 file_info被引用的计数 file_ref_count
    {
        //查看此用户是否已经有此文件，如果存在说明此文件已上传，无需再上传
        sprintf(sql_cmd,
                "select * from user_file_list where user = '%s' and md5 = '%s' "
                "and file_name = '%s'",
                user, md5, filename);
        //返回值： 1: 表示已经存储了，有这个文件记录
        ret = CheckwhetherHaveRecord(db_conn, sql_cmd); // 检测个人是否有记录
        if (ret == 1) //如果有结果，说明此用户已经保存此文件
        {
            md5_state = Md5FileExit; // 此用户已经有该文件了，不能重复上传
            goto END;
        }

        // 修改file_info中的count字段，+1 （count
        // 文件引用计数），多了一个用户拥有该文件
        sprintf(sql_cmd, "update file_info set count = %d where md5 = '%s'",
                file_ref_count + 1, md5);
        if (!db_conn->ExecutePassQuery(sql_cmd)) {
            md5_state =
                Md5Failed; // 更新文件引用计数失败这里也认为秒传失败，宁愿他再次上传文件
            goto END;
        }

        // 2、user_file_list, 用户文件列表插入一条数据
        //当前时间戳
        struct timeval tv;
        struct tm *ptm;
        char time_str[128];

        //使用函数gettimeofday()函数来得到时间。它的精度可以达到微妙
        gettimeofday(&tv, NULL);
        ptm = localtime(
            &tv.tv_sec); //把从1970-1-1零点零分到当前时间系统所偏移的秒数时间转换为本地时间
        // strftime()
        // 函数根据区域设置格式化本地时间/日期，函数的功能将时间格式化，或者说格式化一个时间字符串
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", ptm);

        // 用户列表增加一个文件记录
        sprintf(sql_cmd,
                "insert into user_file_list(user, md5, create_time, file_name, "
                "shared_status, pv) values ('%s', '%s', '%s', '%s', %d, %d)",
                user, md5, time_str, filename, 0, 0);

        if (!db_conn->ExecuteCreate(sql_cmd)) {

            md5_state = Md5Failed;
            // 恢复引用计数
            sprintf(sql_cmd, "update file_info set count = %d where md5 = '%s'",
                    file_ref_count, md5);
            if (!db_conn->ExecutePassQuery(sql_cmd)) {

            }
            goto END;
        }

        //查询用户文件数量, 用户数量+1
        if (CacheIncrCount(cache_conn, FILE_USER_COUNT + string(user)) < 0) {

        }

        md5_state = Md5Ok;
    } else //没有结果，秒传失败
    {
        md5_state = Md5Failed;
        goto END;
    }

END:
    /*
    秒传文件：
        秒传成功：  {"code": 0}
        秒传失败：  {"code":1}
        文件已存在：{"code": 5}
    */
    int code = (int)md5_state;
    encodeMd5Json(code, str_json);
}

int ApiMd5(string &url, string &post_data, string &str_json) {
    UNUSED(url);
    //解析json中信息
    /*
        * {
        user:xxxx,
        token: xxxx,
        md5:xxx,
        fileName: xxx
        }
        */
    string user;
    string md5;
    string token;
    string filename;
    int ret = 0;
    ret = decodeMd5Json(post_data, user, token, md5, filename); //解析json中信息
    if (ret < 0) {
        encodeMd5Json((int)Md5Failed, str_json);
        return 0;
    }

    //验证登陆token，成功返回0，失败-1
    ret = VerifyToken(user, token); //
    if (ret == 0) {
        handleDealMd5(user.c_str(), md5.c_str(), filename.c_str(),
                      str_json); //秒传处理
        return 0;
        // encodeMd5Json(HTTP_RESP_FAIL, str_json);   // 暂时先取消MD5校验
        // return 0;
    } else {
        encodeMd5Json(HTTP_RESP_TOKEN_ERR, str_json); // token验证失败错误码
        return 0;
    }
}
