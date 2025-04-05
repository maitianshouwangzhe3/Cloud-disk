#include "api_register.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "api_common.h"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
 #include <sys/time.h>
#include <time.h>

//解析用户注册信息的json包
/*json数据如下
    {
        userName:xxxx,
        nickName:xxx,
        firstPwd:xxx,
        phone:xxx,
        email:xxx
    }
    */
int decodeRegisterJson(const std::string &str_json, string &user_name, string &nick_name, string &pwd, string &phone, string &email) {
    rapidjson::Document root;
    root.Parse(str_json.c_str());

    if(root.HasParseError()) {
        return -1;
    }

    if(!root.HasMember("userName") && !root["userName"].IsString()) {
        return -1;
    }

    user_name = root["userName"].GetString();

    if (!root.HasMember("nickName") && !root["nickName"].IsString()) {
        return -1;
    }
    nick_name = root["nickName"].GetString();

    if (!root.HasMember("firstPwd") && !root["firstPwd"].IsString()) {
        return -1;
    }
    pwd = root["firstPwd"].GetString();

    if (!root.HasMember("phone") && !root["phone"].IsString()) {
        //return -1;
    } else {
        phone = root["phone"].GetString();
    }
    
    if (!root.HasMember("email") && !root["email"].IsString()) {
        //return -1;
    } else {
        email = root["email"].GetString();
    }
    

    return 0;
}

// 封装注册用户的json
int encodeRegisterJson(int ret, string &str_json) {
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    writer.StartObject();
    writer.Key("code");
    writer.Int(ret);
    writer.EndObject();
    str_json = buffer.GetString();
    return 0;
}

template <typename... Args>
std::string formatString2(const std::string &format, Args... args) {
    auto size = std::snprintf(nullptr, 0, format.c_str(), args...) +
                1; // Extra space for '\0'
    std::unique_ptr<char[]> buf(new char[size]);
    std::snprintf(buf.get(), size, format.c_str(), args...);
    return std::string(buf.get(),
                       buf.get() + size - 1); // We don't want the '\0' inside
}
int registerUser(string &user_name, string &nick_name, string &pwd,
                 string &phone, string &email) {
    int ret = 0;
    uint32_t user_id;
    CDBManager *db_manager = CDBManager::getInstance();
    CDBConn *db_conn = db_manager->GetDBConn("cloud_disk_slave");
    AUTO_REL_DBCONN(db_manager, db_conn);
    // 先查看用户是否存在
    string str_sql;
    // str_sql =  "select * from user_info where user_name=" +  \' + user_name +
    // "/"";
    str_sql = formatString2("select id, user_name from user_info where user_name='%s'",
                           user_name.c_str());
    CResultSet *result_set = db_conn->ExecuteQuery(str_sql.c_str());
    if (result_set && result_set->Next()) { // 检测是否存在用户记录
        // 存在在返回
        delete result_set;
        ret = 2;
    } else { // 如果不存在则注册
        time_t now;
        char create_time[TIME_STRING_LEN];
        //获取当前时间
        now = time(NULL);
        strftime(create_time, TIME_STRING_LEN - 1, "%Y-%m-%d %H:%M:%S",
                 localtime(&now));
        str_sql = "insert into user_info "
                 "(`user_name`,`nick_name`,`password`,`phone`,`email`,`create_"
                 "time`) values(?,?,?,?,?,?)";
        // 必须在释放连接前delete
        // CPrepareStatement对象，否则有可能多个线程操作mysql对象，会crash
        CPrepareStatement *stmt = new CPrepareStatement();
        if (stmt->Init(db_conn->GetMysql(), str_sql)) {
            uint32_t index = 0;
            string c_time = create_time;
            stmt->SetParam(index++, user_name);
            stmt->SetParam(index++, nick_name);
            stmt->SetParam(index++, pwd);
            stmt->SetParam(index++, phone);
            stmt->SetParam(index++, email);
            stmt->SetParam(index++, c_time);
            bool bRet = stmt->ExecuteUpdate();
            if (bRet) {
                ret = 0;
                user_id = db_conn->GetInsertId();
            } else {
                ret = 1;
            }
        }
        delete stmt;
    }

    return ret;
}


#define HTTP_RESPONSE_JSON                                                     \
    "HTTP/1.1 200 OK\r\n"                                                      \
    "Connection:close\r\n"                                                     \
    "Content-Length:%d\r\n"                                                    \
    "Content-Type:application/json;charset=utf-8\r\n\r\n%s"
int ApiRegisterUser(string &url, string &post_data, string &resp_json) {

    // 封装注册结果
    // encodeRegisterJson(0, resp_json);
    // return 0;
    int ret = 0;
    string user_name;
    string nick_name;
    string pwd;
    string phone;
    string email;

    // 判断数据是否为空
    if (post_data.empty()) {
        // 封装注册结果
        encodeRegisterJson(1, resp_json);
        return -1;
    }
    // 解析json
    if (decodeRegisterJson(post_data, user_name, nick_name, pwd, phone, email) < 0) {
        // 封装注册结果
        encodeRegisterJson(1, resp_json);
        return -1;
    }

    // 注册账号
    ret = registerUser(user_name, nick_name, pwd, phone, email); //先不操作数据库看看性能
 
    // 封装注册结果
    ret = encodeRegisterJson(ret, resp_json);


    return ret;
}