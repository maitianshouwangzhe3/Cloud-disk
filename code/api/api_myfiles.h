#ifndef _API_MYFILES_H_
#define _API_MYFILES_H_
#include "api_common.h"
#if API_MYFILES_MUTIL_THREAD
int ApiMyfiles(uint32_t conn_uuid, std::string &url, std::string &post_data);
#else
int ApiMyfiles(string &url, string &post_data, string &str_json);
#endif
#endif