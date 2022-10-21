/**
 * @file myfiles_cgi.c
 * @brief  用户列表展示CGI程序
 * @author Mike
 * @version 2.0
 * @date 2017年10月20日
 */

#include <unistd.h>
extern char ** environ;

#include "fcgio.h"
#include <iostream>


#include "fcgi_config.h"
#include "fcgi_stdio.h"
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

#define MYFILES_LOG_MODULE       "cgi"
#define MYFILES_LOG_PROC         "myfiles"

using namespace std;

class myfiles{
public:
    myfiles();
    ~myfiles();
    void run();
private:
    void Init();
    void aftermath(int ret, char* out2, MYSQL_RES* res_set, cJSON* root, char* out);
    //读取配置信息
    void read_cfg();
    //解析的json包, 登陆token
    int get_count_json_info(char *user, char *token);
    //返回前端情况
    void return_login_status(long num, int token_flag);
    //获取用户文件个数
    void get_user_files_count(char *user, int ret);
    //解析的json包
    int get_fileslist_json_info(char *user, char *token, int *p_start, int *p_count);
    //获取用户文件列表
    //获取用户文件信息 127.0.0.1:80/myfiles&cmd=normal
    //按下载量升序 127.0.0.1:80/myfiles?cmd=pvasc
    //按下载量降序127.0.0.1:80/myfiles?cmd=pvdesc
    int get_user_filelist(char *cmd, char *user, int start, int count);

    //mysql 数据库配置信息 用户名， 密码， 数据库名称
    char mysql_user[128];
    char mysql_pwd[128];
    char mysql_db[128];

    //count 获取用户文件个数
    //display 获取用户文件信息，展示到前端
    char cmd[20];
    char user[USER_NAME_LEN];
    char token[TOKEN_LEN];

    MYSQL *conn;

    char buf[4 * 1024];

    int len;

    streambuf * cin_streambuf;
    streambuf * cout_streambuf;
    streambuf * cerr_streambuf;

    FCGX_Request request;

};