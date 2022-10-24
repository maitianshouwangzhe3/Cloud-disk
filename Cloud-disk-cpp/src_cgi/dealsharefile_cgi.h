/**
 * @file dealsharefile_cgi.c
 * @brief  共享文件pv字段处理、取消分享、转存文件cgi程序
 * @author Mike
 * @version 2.0
 * @date 2022年10月24日
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
#include "../include/redis_keys.h"
#include "../include/redis_op.h"
#include <sys/time.h>

#define DEALSHAREFILE_LOG_MODULE       "cgi"
#define DEALSHAREFILE_LOG_PROC         "dealsharefile"

using namespace std;


class dealsharefile{
public:
    dealsharefile();
    ~dealsharefile();
    void run();

private:
    void read_cfg();
    void Init();
    //转存文件
    //返回值：0成功，-1转存失败，-2文件已存在
    int save_file();
    //取消分享文件
    int cancel_share_file();
    //文件下载标志处理
    int pv_file();
    //解析的json包
    int get_json_info();
    void aftermath(int ret, redisContext* redis_conn, redis_op& rp);
    void aftermath(int ret);
    void aftermath2(int ret, redisContext* redis_conn, redis_op& rp);

    //mysql 数据库配置信息 用户名， 密码， 数据库名称
    char mysql_user[128];
    char mysql_pwd[128];
    char mysql_db[128];

    //redis 服务器ip、端口
    char redis_ip[30];
    char redis_port[10];

    char cmd[20];
    char user[USER_NAME_LEN];   //用户名
    char md5[MD5_LEN];          //文件md5码
    char filename[FILE_NAME_LEN]; //文件名字

    char buf[4 * 1024];

    int len;

    MYSQL* conn;

    streambuf * cin_streambuf;
    streambuf * cout_streambuf;
    streambuf * cerr_streambuf;

    FCGX_Request request;



};