
#include "api_share.h"
#include "redis_keys.h"
#include "api_common.h"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

template <typename... Args>
static std::string formatString(const std::string &format, Args... args) {
    auto size = std::snprintf(nullptr, 0, format.c_str(), args...) +
                1; // Extra space for '\0'
    std::unique_ptr<char[]> buf(new char[size]);
    std::snprintf(buf.get(), size, format.c_str(), args...);
    return std::string(buf.get(), buf.get() + size - 1); // We don't want the '\0' inside
}

int encodeSharePictureJson(int ret, int pv, string& url, string& user, string& time, string &str_json) {
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    writer.StartObject();
    writer.Key("code");
    writer.Int(ret);
    if (HTTP_RESP_OK == ret) {
        writer.Key("url");
        writer.String(url.c_str());
        writer.Key("pv");
        writer.Int(pv);
        writer.Key("user");
        writer.String(user.c_str());
        writer.Key("time");
        writer.String(time.c_str());
    }
    writer.EndObject();
    str_json = buffer.GetString();
    return 0;
}

int ApiSharep(std::string& urlMd5, std::string& strJson) {
    // 获取数据库连接
    CDBManager *db_manager = CDBManager::getInstance();
    CDBConn *db_conn = db_manager->GetDBConn("tuchuang_slave");
    AUTO_REL_DBCONN(db_manager, db_conn);
    CacheManager *cache_manager = CacheManager::getInstance();
    CacheConn *cache_conn = cache_manager->GetCacheConn("token");
    AUTO_REL_CACHECONN(cache_manager, cache_conn);
    string Url = "";
    int pv = 0;
    string user = "";
    string time = "";

    std::unordered_map<std::string, std::string> ResultMap;
    if(cache_conn->HgetAll(urlMd5, ResultMap) && !ResultMap.empty()) {
        Url = ResultMap["url"];
        pv = atoi(ResultMap["pv"].c_str());
        user = ResultMap["user"];
        time = ResultMap["create_time"];
        if(Url.empty()) {
            encodeSharePictureJson(HTTP_RESP_FAIL, pv, Url, user, time, strJson);
        } else {
            encodeSharePictureJson(HTTP_RESP_OK, pv, Url, user, time, strJson);
        }
    } else {
        std::string strSql = formatString("select a.url, b.`user`, b.pv, b.create_time from file_info as a join share_picture_list  as b on b.filemd5 = a.md5 where b.urlmd5 = '%s';", urlMd5.c_str());
        CResultSet *result_set = db_conn->ExecuteQuery(strSql.c_str());
        if (result_set && result_set->Next()) {
            // 存在在返回
            Url = result_set->GetString("url");
            pv = result_set->GetInt("pv");
            user = result_set->GetString("user");
            time = result_set->GetString("create_time");
            if(Url.empty()) {
                encodeSharePictureJson(HTTP_RESP_FAIL, pv, Url, user, time, strJson);
            } else {
                ResultMap["url"] = Url;
                ResultMap["pv"] = std::to_string(pv);
                ResultMap["user"] = user;
                ResultMap["create_time"] = time;
                cache_conn->Hmset(urlMd5, ResultMap);
                encodeSharePictureJson(HTTP_RESP_OK, pv, Url, user, time, strJson);
            }
        } else {
            encodeSharePictureJson(HTTP_RESP_FAIL, pv, Url, user, time, strJson);
        }
    }

    return 0;
}