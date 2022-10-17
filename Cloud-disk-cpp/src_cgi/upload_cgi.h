/**
 * @file upload_cgi
 * @brief   上传文件后台CGI程序
 * @author  Mike
 * @version 2.0
 * @date 2022年10月17日
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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include "deal_mysql.h"
#include "fcgi_stdio.h"
#include "make_log.h" //日志头文件
#include "cfg.h"
#include "util_cgi.h" //cgi后台通用接口，trim_space(), memstr()

#define UPLOAD_LOG_MODULE "cgi"
#define UPLOAD_LOG_PROC   "upload"

using namespace std;

class upload{
public:
    upload();
    ~upload();
    void run();
    void Init();
    void read_cfg();
    int store_fileinfo_to_mysql();
    int make_file_url();
    int upload_to_dstorage();
    int recv_save_file();
    void tmp(int ret);

private:
    //mysql 数据库配置信息 用户名， 密码， 数据库名称
    char mysql_user[128];
    char mysql_pwd[128];
    char mysql_db[128];

    char filename[FILE_NAME_LEN]; //文件名
    char user[USER_NAME_LEN];   //文件上传者
    char md5[MD5_LEN];    //文件md5码
    long size;  //文件大小
    char fileid[TEMP_BUF_MAX_LEN];    //文件上传到fastDFS后的文件id
    char fdfs_file_url[FILE_URL_LEN]; //文件所存放storage的host_name

    long len;   //接收数据的长度

    MYSQL *conn;

    streambuf * cin_streambuf;
    streambuf * cout_streambuf;
    streambuf * cerr_streambuf;

    FCGX_Request request;
};