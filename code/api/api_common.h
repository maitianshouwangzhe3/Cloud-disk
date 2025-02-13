#ifndef _API_COMMON_H_
#define _API_COMMON_H_
#include "cache_pool.h"
#include "db_pool.h"
#include "tc_common.h"
#include <string>

#define HTTP_RESPONSE_JSON_MAX 4096
#define HTTP_RESPONSE_JSON                                                     \
    "HTTP/1.1 200 OK\r\n"                                                      \
    "Connection:close\r\n"                                                     \
    "Content-Length:%d\r\n"                                                    \
    "Content-Type:application/json;charset=utf-8\r\n\r\n%s"

#define HTTP_RESPONSE_HTML                                                    \
    "HTTP/1.1 200 OK\r\n"                                                      \
    "Connection:close\r\n"                                                     \
    "Content-Length:%d\r\n"                                                    \
    "Content-Type:text/html;charset=utf-8\r\n\r\n%s"


#define HTTP_RESPONSE_BAD_REQ                                                     \
    "HTTP/1.1 400 Bad\r\n"                                                      \
    "Connection:close\r\n"                                                     \
    "Content-Length:%d\r\n"                                                    \
    "Content-Type:application/json;charset=utf-8\r\n\r\n%s"

// 开启多线程
#define API_MYFILES_MUTIL_THREAD 0
#define API_LOGIN_MUTIL_THREAD  0

extern string s_dfs_path_client;
extern string s_storage_web_server_ip;
extern string s_storage_web_server_port;
extern string s_shorturl_server_address;
extern string s_shorturl_server_access_token;
using std::string;
int ApiInit();
//获取用户文件个数
int CacheSetCount(CacheConn *cache_conn, string key, int64_t count);
int CacheGetCount(CacheConn *cache_conn, string key, int64_t &count);
int CacheIncrCount(CacheConn *cache_conn, string key);
int CacheDecrCount(CacheConn *cache_conn, string key);
int CacheHmset(CacheConn *cache_conn, string key, unordered_map<string, string> &hash);
int DBGetUserFilesCountByUsername(CDBConn *db_conn, string user_name, int &count);
int DBGetShareFilesCount(CDBConn *db_conn, int &count);
int DBGetSharePictureCountByUsername(CDBConn *db_conn, string user_name, int &count);
int RemoveFileFromFastDfs(const char *fileid);
#endif