/**
 * @file reg_cgi.c
 * @brief  注册事件后CGI程序
 * @author Mike
 * @version 2.0
 * @date 2022年10月16日
 */


#include <unistd.h>
extern char ** environ;

#include "fcgio.h"
#include <iostream>


#include "fcgi_config.h"
#include "fcgi_stdio.h"
#include "fcgi_config.h"
#include "fcgi_stdio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/make_log.h" //日志头文件
#include "../include/util_cgi.h"
#include "../include/deal_mysql.h"
#include "../include/cfg.h"
#include "../include/cJSON.h"
#include <sys/time.h>

#define REG_LOG_MODULE       (char*)"cgi"
#define REG_LOG_PROC         (char*)"reg"
// 

using namespace std;
class reg{
private:
    int len;
    char buf[4 * 1024];
    char* out;
    MYSQL* conn;
    char mysql_user[256];
    char mysql_pwd[256];
    char mysql_db[256];

    streambuf * cin_streambuf;
    streambuf * cout_streambuf;
    streambuf * cerr_streambuf;

    FCGX_Request request;
public:
    void run();
    void Init();
    reg();
    ~reg();

    int user_register();
    int get_reg_info(char *reg_buf, char *user, char *nick_name, char *pwd, char *tel, char *email);
};