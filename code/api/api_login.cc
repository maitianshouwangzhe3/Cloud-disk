#include "api_login.h"
#include "redis_keys.h"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

 
#define LOGIN_RET_OK 0   // 成功
#define LOGIN_RET_FAIL 1 // 失败

// 解析登录信息
int decodeLoginJson(const std::string &str_json, string &user_name, string &pwd) {
    rapidjson::Document root;
    root.Parse(str_json.c_str());

    if(root.HasParseError()) {
        return -1;
    }

    if(!root.HasMember("user") && !root["user"].IsString()) {
        return -1;
    }

    user_name = root["user"].GetString();

    if (!root.HasMember("pwd") && !root["pwd"].IsString()) {
        return -1;
    }
    pwd = root["pwd"].GetString();
    return 0;
}

// 封装登录结果的json
int encodeLoginJson(int ret, string &token, string &str_json) {
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    writer.StartObject();
    writer.Key("code");
    writer.Int(ret);
    if(!ret) {
        writer.Key("token");
        writer.String(token.c_str());
    }
    writer.EndObject();
    str_json = buffer.GetString();
    return 0;
}

template <typename... Args>
std::string formatString1(const std::string &format, Args... args) {
    auto size = std::snprintf(nullptr, 0, format.c_str(), args...) +
                1; // Extra space for '\0'
    std::unique_ptr<char[]> buf(new char[size]);
    std::snprintf(buf.get(), size, format.c_str(), args...);
    return std::string(buf.get(),
                       buf.get() + size - 1); // We don't want the '\0' inside
}
/* -------------------------------------------*/
/**
 * @brief  判断用户登陆情况
 *
 * @param username 		用户名
 * @param pwd 		密码
 *
 * @returns
 *      成功: 0
 *      失败：-1
 */
/* -------------------------------------------*/
int verifyUserPassword(string &user_name, string &pwd) {
    int ret = 0;
    CDBManager *db_manager = CDBManager::getInstance();
    CDBConn *db_conn = db_manager->GetDBConn("tuchuang_slave");
    AUTO_REL_DBCONN(db_manager, db_conn);

    // 先查看用户是否存在
    string strSql;

    strSql =
        formatString1("select password from user_info where user_name='%s'",
                      user_name.c_str());
    CResultSet *result_set = db_conn->ExecuteQuery(strSql.c_str());
    uint32_t nowtime = time(NULL);
    if (result_set && result_set->Next()) {
        // 存在在返回
        string password = result_set->GetString("password");
        if (result_set->GetString("password") == pwd)
            ret = 0;
        else
            ret = -1;
    } else { // 如果不存在则注册
        ret = -1;
    }

    delete result_set;

    return ret;
}

/* -------------------------------------------*/
/**
 * @brief  生成token字符串, 保存redis数据库
 *
 * @param username 		用户名
 * @param token     生成的token字符串
 *
 * @returns
 *      成功: 0
 *      失败：-1
 */
/* -------------------------------------------*/
int setToken(string &user_name, string &token) {
    int ret = 0;
    CacheManager *cache_manager = CacheManager::getInstance();
    CacheConn *cache_conn = cache_manager->GetCacheConn("token");
    AUTO_REL_CACHECONN(cache_manager, cache_conn);

    token = RandomString(32); // 随机32个字母

    if (cache_conn) {
        //用户名：token, 86400有效时间为24小时
        cache_conn->SetEx(user_name, 86400, token); // redis做超时
    } else {
        ret = -1;
    }

    return ret;
}

int loadMyfilesCountAndSharepictureCount(string &user_name) {
    int64_t redis_file_count = 0;
    int mysq_file_count = 0;
    CDBManager *db_manager = CDBManager::getInstance();
    CDBConn *db_conn = db_manager->GetDBConn("tuchuang_slave");
    AUTO_REL_DBCONN(db_manager, db_conn);
    CacheManager *cache_manager = CacheManager::getInstance();
    CacheConn *cache_conn = cache_manager->GetCacheConn("token");
    AUTO_REL_CACHECONN(cache_manager, cache_conn);

    // 从mysql加载
    if (DBGetUserFilesCountByUsername(db_conn, user_name, mysq_file_count) < 0) {
        return -1;
    }
    redis_file_count = (int64_t)mysq_file_count;
    if (CacheSetCount(cache_conn, FILE_USER_COUNT + user_name,
                      redis_file_count) < 0) // 失败了那下次继续从mysql加载
    {
        return -1;
    }

    // 从mysql加载 我的分享图片数量
    if (DBGetSharePictureCountByUsername(db_conn, user_name, mysq_file_count) < 0) {
        return -1;
    }

    redis_file_count = (int64_t)mysq_file_count;
    if (CacheSetCount(cache_conn, SHARE_PIC_COUNT + user_name,
                      redis_file_count) < 0) // 失败了那下次继续从mysql加载
    {
        return -1;
    }

    return 0;
}

#if API_LOGIN_MUTIL_THREAD
int ApiUserLogin(u_int32_t conn_uuid, std::string &url, std::string &post_data)
{
    UNUSED(url);
    string user_name;
    string pwd;
    string token;
    string str_json;
    // 判断数据是否为空
    if (post_data.empty()) {
        encodeLoginJson(1, token, str_json);
        goto END;
    }
    // 解析json
    if (decodeLoginJson(post_data, user_name, pwd) < 0) {
        LOG_ERROR << "decodeRegisterJson failed";
        encodeLoginJson(1, token, str_json);
        goto END;
    }

    // 验证账号和密码是否匹配
    if (verifyUserPassword(user_name, pwd) < 0) {
        LOG_ERROR << "verifyUserPassword failed";
        encodeLoginJson(1, token, str_json);
        goto END;
    }

    // 生成token
    if (setToken(user_name, token) < 0) {
        LOG_ERROR << "setToken failed";
        encodeLoginJson(1, token, str_json);
        goto END;
    }

    // 加载 我的文件数量  我的分享图片数量

    if (loadMyfilesCountAndSharepictureCount(user_name) < 0) {
        LOG_ERROR << "loadMyfilesCountAndSharepictureCount failed";
        encodeLoginJson(1, token, str_json);
        goto END;
    }
    // 注册账号
    // ret = registerUser(user_name, nick_name, pwd, phone, email);
    encodeLoginJson(0, token, str_json);
END:
    char *str_content = new char[HTTP_RESPONSE_JSON_MAX];
    uint32_t ulen = str_json.length();
    snprintf(str_content, HTTP_RESPONSE_JSON_MAX, HTTP_RESPONSE_JSON, ulen,
             str_json.c_str());
    str_json = str_content;
    CHttpConn::AddResponseData(conn_uuid, str_json);
    delete str_content;
    
    return 0;
}
#else

int ApiUserLogin(string &url, string &post_data, string &str_json) {
    UNUSED(url);
    string user_name;
    string pwd;
    string token;

    // 判断数据是否为空
    if (post_data.empty()) {
        return -1;
    }
    // 解析json
    if (decodeLoginJson(post_data, user_name, pwd) < 0) {
        encodeLoginJson(1, token, str_json);
        return -1;
    }

    // 验证账号和密码是否匹配
    if (verifyUserPassword(user_name, pwd) < 0) {
        encodeLoginJson(1, token, str_json);
        return -1;
    }

    // 生成token
    if (setToken(user_name, token) < 0) {
        encodeLoginJson(1, token, str_json);
        return -1;
    }

    // 加载 我的文件数量  我的分享图片数量

    if (loadMyfilesCountAndSharepictureCount(user_name) < 0) {
        encodeLoginJson(1, token, str_json);
        return -1;
    }
    // 注册账号
    // ret = registerUser(user_name, nick_name, pwd, phone, email);
    encodeLoginJson(0, token, str_json);
    return 0;
}
#endif


// 这里只是用来测试的，不用理会
int ApiUserLoginTest(string &url, string &post_data, string &str_json) {
    UNUSED(url);
    string user_name;
    string pwd;
    string token;

    // 判断数据是否为空
    if (post_data.empty()) {
        return -1;
    }
    // 解析json
    if (decodeLoginJson(post_data, user_name, pwd) < 0) {
        encodeLoginJson(1, token, str_json);
        return -1;
    }

    // 验证账号和密码是否匹配
    if (verifyUserPassword(user_name, pwd) < 0) {
        encodeLoginJson(1, token, str_json);
        return -1;
    }

    // 生成token
    if (setToken(user_name, token) < 0) {
        encodeLoginJson(1, token, str_json);
        return -1;
    }

    // 加载 我的文件数量  我的分享图片数量

    if (loadMyfilesCountAndSharepictureCount(user_name) < 0) {
        encodeLoginJson(1, token, str_json);
        return -1;
    }
    // 注册账号
    // ret = registerUser(user_name, nick_name, pwd, phone, email);
    encodeLoginJson(0, token, str_json);
    return 0;
}