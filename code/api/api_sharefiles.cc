#include "api_sharefiles.h"
#include "redis_keys.h"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include <memory>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "api_common.h"
#include <sys/time.h>

//获取共享文件数量
int getShareFilesCount(CDBConn *db_conn, CacheConn *cache_conn, int &count) {
    int ret = 0;
    int64_t file_count = 0;

    // 先查看用户是否存在
    string str_sql;

    // 1. 先从redis里面获取，如果数量为0则从mysql查询确定是否为0
    if (CacheGetCount(cache_conn, FILE_PUBLIC_COUNT, file_count) < 0) {
        ret = -1;
    }

    if (file_count == 0) {
        // 从mysql加载
        if (DBGetShareFilesCount(db_conn, count) < 0) {
            return -1;
        }
        file_count = (int64_t)count;
        if (CacheSetCount(cache_conn, FILE_PUBLIC_COUNT, file_count) <
            0) // 失败了那下次继续从mysql加载
        {
            return -1;
        }
        ret = 0;
    }
    count = file_count;

    return ret;
}

//获取共享文件个数
int handleGetSharefilesCount(int &count) {
    CDBManager *db_manager = CDBManager::getInstance();
    CDBConn *db_conn = db_manager->GetDBConn("tuchuang_slave");
    AUTO_REL_DBCONN(db_manager, db_conn);
    CacheManager *cache_manager = CacheManager::getInstance();
    CacheConn *cache_conn = cache_manager->GetCacheConn("token");
    AUTO_REL_CACHECONN(cache_manager, cache_conn);

    int ret = 0;
    ret = getShareFilesCount(db_conn, cache_conn, count);

    return ret;
}

//解析的json包
// 参数
// {
// "count": 2,
// "start": 0,
// "token": "3a58ca22317e637797f8bcad5c047446",
// "user": "qingfu"
// }
int decodeShareFileslistJson(string &str_json, int &start, int &count) {
    bool res;
    rapidjson::Document root;
    root.Parse(str_json.c_str());
    if (!root.IsObject()) {
        return -1;
    }

    if (!root.HasMember("start") && !root["start"].IsInt()) {
        return -1;
    }
    start = root["start"].GetInt();

    if (!root.HasMember("count") && !root["count"].IsInt()) {
        return -1;
    }
    count = root["count"].GetInt();
    return 0;
}

template <typename... Args>
static std::string FormatString(const std::string &format, Args... args) {
    auto size = std::snprintf(nullptr, 0, format.c_str(), args...) +
                1; // Extra space for '\0'
    std::unique_ptr<char[]> buf(new char[size]);
    std::snprintf(buf.get(), size, format.c_str(), args...);
    return std::string(buf.get(),
                       buf.get() + size - 1); // We don't want the '\0' inside
}
//获取共享文件列表
//获取用户文件信息 127.0.0.1:80/api/sharefiles&cmd=normal
void handleGetShareFilelist(int start, int count, string &str_json) {
    int ret = 0;
    string str_sql;
    int total = 0;
    CDBManager *db_manager = CDBManager::getInstance();
    CDBConn *db_conn = db_manager->GetDBConn("tuchuang_slave");
    AUTO_REL_DBCONN(db_manager, db_conn);
    CacheManager *cache_manager = CacheManager::getInstance();
    CacheConn *cache_conn = cache_manager->GetCacheConn("token");
    AUTO_REL_CACHECONN(cache_manager, cache_conn);

    CResultSet *result_set = NULL;
    int file_count = 0;
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    writer.StartObject();

    ret = getShareFilesCount(db_conn, cache_conn, total); // 获取文件总数量
    if (ret < 0) {
        ret = -1;
        goto END;
    } else {
        if (total == 0) {
            ret = 0;
            goto END;
        }
    }

    // sql语句
    str_sql = FormatString(
        "select share_file_list.*, file_info.url, file_info.size, file_info.type from file_info, \
        share_file_list where file_info.md5 = share_file_list.md5 limit %d, %d",
        start, count);
    result_set = db_conn->ExecuteQuery(str_sql.c_str());
    if (result_set) {
        // 遍历所有的内容
        // 获取大小
        writer.Key("files");
        writer.StartArray();
        while (result_set->Next()) {            
            writer.StartObject();
            writer.Key("user");
            writer.String(result_set->GetString("user"));
            writer.Key("md5");
            writer.String(result_set->GetString("md5"));
            writer.Key("file_name");
            writer.String(result_set->GetString("file_name"));
            writer.Key("share_status");
            writer.Int(result_set->GetInt("share_status"));
            writer.Key("pv");
            writer.Int(result_set->GetInt("pv"));
            writer.Key("create_time");
            writer.String(result_set->GetString("create_time"));
            writer.Key("url");
            writer.String(result_set->GetString("url"));
            writer.Key("size");
            writer.Int(result_set->GetInt("size"));
            writer.Key("type");
            writer.String(result_set->GetString("type"));
            writer.EndObject();
            ++file_count;
        }
        writer.EndArray();
        ret = 0;
        delete result_set;
    } else {
        ret = -1;
    }
END:
    if (ret == 0) {
        writer.Key("code");
        writer.Int(0);
        writer.Key("total");
        writer.Int(total);
        writer.Key("count");
        writer.Int(file_count);
    } else {
         writer.Key("code");
        writer.Int(1);
    }
    writer.EndObject();
    str_json = buffer.GetString();
}

//获 取共享文件排行版
//按下载量降序127.0.0.1:80/api/sharefiles?cmd=pvdesc
void handleGetRankingFilelist(int start, int count, string &str_json) {
    /*
    a) mysql共享文件数量和redis共享文件数量对比，判断是否相等
    b) 如果不相等，清空redis数据，从mysql中导入数据到redis (mysql和redis交互)
    c) 从redis读取数据，给前端反馈相应信息
    */

    int ret = 0;
    char sql_cmd[SQL_MAX_LEN] = {0};
    int total = 0;
    char filename[1024] = {0};
    int sql_num;
    int redis_num;
    int score;
    int end;
    RVALUES value = NULL;
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    writer.StartObject();
    int file_count = 0;

    CDBManager *db_manager = CDBManager::getInstance();
    CDBConn *db_conn = db_manager->GetDBConn("tuchuang_slave");
    AUTO_REL_DBCONN(db_manager, db_conn);
    CResultSet *pCResultSet = NULL;

    CacheManager *cache_manager = CacheManager::getInstance();
    CacheConn *cache_conn = cache_manager->GetCacheConn("token");
    AUTO_REL_CACHECONN(cache_manager, cache_conn);

    // 获取共享文件的总数量
    ret = getShareFilesCount(db_conn, cache_conn, total);
    if (ret != 0) {
        //LOG_ERROR << sql_cmd << " 操作失败";
        ret = -1;
        goto END;
    }
    //===1、mysql共享文件数量
    sql_num = total;

    //===2、redis共享文件数量
    redis_num = cache_conn->ZsetZcard(
        FILE_PUBLIC_ZSET); // Zcard 命令用于计算集合中元素的数量。
    if (redis_num == -1) {
        //LOG_ERROR << "ZsetZcard  操作失败";
        ret = -1;
        goto END;
    }

    //===3、mysql共享文件数量和redis共享文件数量对比，判断是否相等
    if (redis_num != sql_num) // 如果数量太多会导致阻塞， redis mysql数据不一致怎么处理？
    { //===4、如果不相等，清空redis数据，重新从mysql中导入数据到redis
      //(mysql和redis交互)

        // a) 清空redis有序数据
        cache_conn->Del(FILE_PUBLIC_ZSET); // 删除集合
        cache_conn->Del(FILE_NAME_HASH); // 删除hash， 理解 这里hash和集合的关系

        // b) 从mysql中导入数据到redis
        // sql语句
        strcpy( sql_cmd, "select md5, file_name, pv from share_file_list order by pv desc");

        pCResultSet = db_conn->ExecuteQuery(sql_cmd);
        if (!pCResultSet) {
            //LOG_ERROR << sql_cmd << " 操作失败";
            ret = -1;
            goto END;
        }

        // mysql_fetch_row从使用mysql_store_result得到的结果结构中提取一行，并把它放到一个行结构中。
        // 当数据用完或发生错误时返回NULL.
        while (
            pCResultSet
                ->Next()) // 这里如果文件数量特别多，导致耗时严重，
                          // 可以这么去改进当
                          // mysql的记录和redis不一致的时候，开启一个后台线程去做同步
        {
            char field[1024] = {0};
            string md5 = pCResultSet->GetString("md5"); // 文件的MD5
            string file_name = pCResultSet->GetString("file_name"); // 文件名
            int pv = pCResultSet->GetInt("pv");
            sprintf(field, "%s%s", md5.c_str(),
                    file_name.c_str()); //文件标示，md5+文件名

            //增加有序集合成员
            cache_conn->ZsetAdd(FILE_PUBLIC_ZSET, pv, field);

            //增加hash记录
            cache_conn->Hset(FILE_NAME_HASH, field, file_name);
        }
    }

    //===5、从redis读取数据，给前端反馈相应信息
    // char value[count][1024];
    value = (RVALUES)calloc(count, VALUES_ID_SIZE); //堆区请求空间
    if (value == NULL) {
        ret = -1;
        goto END;
    }

    file_count = 0;
    end = start + count - 1; //加载资源的结束位置
    //降序获取有序集合的元素   file_count获取实际返回的个数
    ret = cache_conn->ZsetZrevrange(FILE_PUBLIC_ZSET, start, end, value,
                                    file_count);
    if (ret != 0) {
        //LOG_ERROR << "ZsetZrevrange 操作失败";
        ret = -1;
        goto END;
    }

    writer.Key("files");
    writer.StartArray();
    //遍历元素个数
    for (int i = 0; i < file_count; ++i) {
        /*
        {
            "filename": "test.mp4",
            "pv": 0
        }
        */
        writer.StartObject();
        writer.Key("filename");
        writer.String(filename);
        writer.Key("pv");
        writer.Int(score);
        writer.EndObject();
    }
    writer.EndArray();
END:
    if (ret == 0) {
        writer.Key("code");
        writer.Int(0);
        writer.Key("total");
        writer.Int(total);
        writer.Key("count");
        writer.Int(file_count);
    } else {
        writer.Key("code");
        writer.Int(1);
    }

    writer.EndObject();
    str_json = buffer.GetString();
}

int encodeSharefilesJson(int ret, int total, string &str_json) {
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    writer.StartObject();
    writer.Key("code");
    writer.Int(ret);
    if(!ret) {
        // 正常返回的时候才写入token
        writer.Key("total");
        writer.Int(total);
    }
    writer.EndObject();
    str_json = buffer.GetString();
    return 0;
}
int ApiSharefiles(string &url, string &post_data, string &str_json) {
    // 解析url有没有命令

    // count 获取用户文件个数
    // display 获取用户文件信息，展示到前端
    char cmd[20];
    string user_name;
    string token;
    int start = 0; //文件起点
    int count = 0; //文件个数

    //解析命令 解析url获取自定义参数
    QueryParseKeyValue(url.c_str(), "cmd", cmd, NULL);
    
    if (strcmp(cmd, "count") == 0) // count 获取用户文件个数
    {
        // 解析json
        if (handleGetSharefilesCount(count) < 0) //获取共享文件个数
        {
            encodeSharefilesJson(1, 0, str_json);
        } else {
            encodeSharefilesJson(0, count, str_json);
        }
        return 0;
    } else {
        //获取共享文件信息 127.0.0.1:80/sharefiles&cmd=normal
        //按下载量升序 127.0.0.1:80/sharefiles?cmd=pvasc
        //按下载量降序127.0.0.1:80/sharefiles?cmd=pvdesc
        if (decodeShareFileslistJson(post_data, start, count) < 0){ //通过json包获取信息
            encodeSharefilesJson(1, 0, str_json);
            return 0;
        }
        if (strcmp(cmd, "normal") == 0) {
            handleGetShareFilelist(start, count, str_json); // 获取共享文件
        } else if (strcmp(cmd, "pvdesc") == 0) {
            handleGetRankingFilelist(start, count, str_json); ////获取共享文件排行版
        } else {
            encodeSharefilesJson(1, 0, str_json);
        }
    }
    return 0;
}