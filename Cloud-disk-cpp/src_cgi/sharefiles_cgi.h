/**
 * @file sharefiles_cgi
 * @brief  共享文件列表展示CGI程序
 * @author Mike
 * @version 2.0
 * @date 2022年10月18日
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
#include "../include/redis_keys.h"
#include "../include/redis_op.h"
#include "../include/cfg.h"
#include "../include/cJSON.h"
#include <sys/time.h>

#define SHAREFILES_LOG_MODULE       (char*)"cgi"
#define SHAREFILES_LOG_PROC         (char*)"sharefiles"


using namespace std;

class sharefiles{
public:
    sharefiles();
    ~sharefiles();
    void run();



private:

    void read_cfg();
    void Init();
    int get_ranking_filelist(int start, int count);
    int get_share_filelist(int start, int count);
    int get_fileslist_json_info(int *p_start, int *p_count);
    void get_share_files_count();
    void aftermath(cJSON *root, char* out, char* out2, MYSQL_RES* res_set, int ret);
    void aftermath(int ret, char* out2, MYSQL_RES* res_set, redisContext* redis_conn, RVALUES value, cJSON* root, char* out);

    //mysql 数据库配置信息 用户名， 密码， 数据库名称
    char mysql_user[128];
    char mysql_pwd[128];
    char mysql_db[128];

    //redis 服务器ip、端口
    char redis_ip[30];
    char redis_port[10];

    char cmd[20];

    char* buf;

    int len;

    MYSQL* conn;

    streambuf * cin_streambuf;
    streambuf * cout_streambuf;
    streambuf * cerr_streambuf;

    FCGX_Request request;

};