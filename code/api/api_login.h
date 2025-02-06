#ifndef _API_LOGIN_H_
#define _API_LOGIN_H_
#include "api_common.h"

#if API_LOGIN_MUTIL_THREAD  // 该把这个宏定义放到cmakelists.txt才对
int ApiUserLogin(u_int32_t conn_uuid, std::string &url, std::string &post_data);
#else
int ApiUserLogin(string &url, string &post_data, string &str_json);
#endif
// 这里只是用来测试的，不用理会
int ApiUserLoginTest(string &url, string &post_data, string &str_json);
#endif // ! _API_LOGIN_H_
