/**
 * @file md5_cgi.c
 * @brief  秒传功能的cgi
 * @author Mike
 * @version 2.0
 * @date 2022年10月21日
 */

#include <unistd.h>
extern char ** environ;

#include "fcgio.h"
#include <iostream>


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

#define MD5_LOG_MODULE       "cgi"
#define MD5_LOG_PROC         "md5"


using namespace std;


class md5{
public:
    md5();
    ~md5();
    void run();

private:
    void Init();
    void read_cfg();
    void afretmath(int ret);
    //秒传处理
    //返回值：0秒传成功，-1出错，-2此用户已拥有此文件， -3秒传失败
    int deal_md5();
    //解析秒传信息的json包
    int get_md5_info();

    //mysql 数据库配置信息 用户名， 密码， 数据库名称
    char mysql_user[128];
    char mysql_pwd[128];
    char mysql_db[128];

    char buf[4 * 1024];

    char user[128];
    char Md5[256];
    char token[256];
    char filename[128];

    int len;

    MYSQL* conn;

    streambuf * cin_streambuf;
    streambuf * cout_streambuf;
    streambuf * cerr_streambuf;

    FCGX_Request request;
};