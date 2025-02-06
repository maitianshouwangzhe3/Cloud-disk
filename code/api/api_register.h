#ifndef _API_REGISTER_H_
#define _API_REGISTER_H_
#include "api_common.h"
// using std::string;
int ApiRegisterUser(string &url, string &post_data, string &str_json);
// int ApiRegisterUser(string &url, string &post_data);
// int ApiRegisterUser(uint32_t conn_uuid, std::string &url, std::string &post_data);
#endif