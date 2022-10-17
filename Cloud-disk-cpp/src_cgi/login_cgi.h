#include <unistd.h>
extern char ** environ;

#include "fcgio.h"
#include <iostream>

#include <vector>
#include "fcgi_config.h"
#include "fcgi_stdio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/make_log.h" //日志头文件
#include "../include/util_cgi.h"
#include "../include/deal_mysql.h"
#include "../include/redis_op.h"
#include "../include/cfg.h"
#include "../include/cJSON.h"
#include "../include/des.h"    //加密
#include "../include/base64.h" //base64
#include "../include/md5.h"    //md5
#include <time.h>
//  g++ -o login login_cgi.cpp ../common/redis_op.cpp ../common/des.cpp ../common/base64.cpp ../common/md5.cpp ../common/make_log.cpp ../common/util_cgi.cpp ../common/deal_mysql.cpp ../common/cfg.cpp ../common/cJSON.cpp -lmysqlclient -lm -lfcgi -lhiredis  -I /usr/local/include -L /usr/local/lib -lfcgi -lstdc++ -lfcgi++
#define LOGIN_LOG_MODULE "cgi"
#define LOGIN_LOG_PROC   "(char*)login"

using namespace std;


class login{
private:
    int len;

    char token[128];
    char buf[1024 * 4];
    char user[512];
    char pwd[512];

    streambuf * cin_streambuf;
    streambuf * cout_streambuf;
    streambuf * cerr_streambuf;

    FCGX_Request request;
public:
    login();
    ~login();
    void Init();
    void run();

    //解析用户登陆信息的json包login_buf
    //用户名保存在user，密码保存在pwd
    int get_login_info();
    int check_user_pwd();
    int set_token();
    void return_login_status(char *status_num);

};