#include "upload_cgi.h"


upload::upload(){
    Init();
    conn = nullptr;
    cin_streambuf = cin.rdbuf();
    cout_streambuf = cout.rdbuf();
    cerr_streambuf = cerr.rdbuf();

    FCGX_Init();
    FCGX_InitRequest(&request,0,0);
}

upload::~upload(){
    cin.rdbuf(cin_streambuf);
    cout.rdbuf(cout_streambuf);
    cerr.rdbuf(cerr_streambuf);

    if(conn != NULL)
    {
        mysql_close(conn); //断开数据库连接
    }
}

void upload::Init(){
    memset(mysql_user, 0, sizeof(mysql_user));
    memset(mysql_pwd, 0, sizeof(mysql_pwd));
    memset(mysql_db, 0, sizeof(mysql_db));
    memset(filename, 0, sizeof(filename));
    memset(user, 0, sizeof(user));
    memset(md5, 0, sizeof(md5));
    memset(fileid, 0, sizeof(fileid));
    memset(fdfs_file_url, 0, sizeof(fdfs_file_url));

    size = 0;
    len = 0;
}

void upload::run(){
    fcgi_streambuf cin_fcgi_streambuf(request.in);
    fcgi_streambuf cout_fcgi_streambuf(request.out);
    fcgi_streambuf cerr_fcgi_streambuf(request.err);

    cin.rdbuf(&cin_fcgi_streambuf);
    cout.rdbuf(&cout_fcgi_streambuf);
    cerr.rdbuf(&cerr_fcgi_streambuf);
    while(FCGX_Accept_r(&request) == 0){
        int ret = 0;
        

        char * contentLength = FCGX_GetParam("CONTENT_LENGTH", request.envp);

        cout << "Content-type: text/html\r\n\r\n";

        if (contentLength != NULL)
        {
            len = strtol(contentLength, NULL, 10); //字符串转long， 或者atol
        }

        if(len <= 0){
            cout << "No data from standard input\n";
            LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC, "len = 0, No data from standard input\n");
            ret = -1;
        }
        else {
            //===============> 得到上传文件  <============
            if (recv_save_file() < 0)
            {
                ret = -1;
                //这里记得做善后
                tmp(ret);
            }
            LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC, "%s成功上传[%s, 大小：%ld, md5码:%s]到本地\n", user, filename, size, md5);

            //===============> 将该文件存入fastDFS中,并得到文件的file_id <============
            if (upload_to_dstorage() < 0)
            {
                ret = -1;
                //这里记得做善后
                tmp(ret);
            }

            //================> 删除本地临时存放的上传文件 <===============
            unlink(filename);

            //================> 得到文件所存放storage的host_name <=================
            if (make_file_url() < 0)
            {
                ret = -1;
                //这里记得做善后
                tmp(ret);
            }

            //===============> 将该文件的FastDFS相关信息存入mysql中 <======
            if (store_fileinfo_to_mysql() < 0)
            {
                ret = -1;
                //这里记得做善后
                tmp(ret);
            }

            if(ret != -1){
                tmp(ret);
            }
        }
    }
}

void upload::tmp(int ret){
    memset(filename, 0, FILE_NAME_LEN);
    memset(user, 0, USER_NAME_LEN);
    memset(md5, 0, MD5_LEN);
    memset(fileid, 0, TEMP_BUF_MAX_LEN);
    memset(fdfs_file_url, 0, FILE_URL_LEN);

    char *out = NULL;
    //给前端返回，上传情况
    /*
   上传文件：
   成功：{"code":"008"}
   失败：{"code":"009"}
    */
   if(ret == 0) //成功上传
    {
        out = util_cgi::return_status("008");//common.h
    }
    else//上传失败
    {
        out = util_cgi::return_status("009");//common.h
    }

    if(out != NULL)
    {
        printf(out); //给前端反馈信息
        free(out);   //记得释放
    }
}

/* -------------------------------------------*/
/**
 * @brief  解析上传的post数据 保存到本地临时路径
 *         同时得到文件上传者、文件名称、文件大小
 *
 * @param len       (in)    post数据的长度
 * @param user      (out)   文件上传者
 * @param file_name (out)   文件的文件名
 * @param md5       (out)   文件的MD5码
 * @param p_size    (out)   文件大小
 *
 * @returns
 *          0 succ, -1 fail
 */
/* -------------------------------------------*/
int upload::recv_save_file(){
    int ret = 0;
    char *file_buf = NULL;
    char *begin = NULL;
    char *p, *q, *k;

    char content_text[TEMP_BUF_MAX_LEN] = {0}; //文件头部信息
    char boundary[TEMP_BUF_MAX_LEN] = {0};     //分界线信息

    //==========> 开辟存放文件的 内存 <===========
    file_buf = (char *)malloc(len);
    if (file_buf == NULL)
    {
        LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC, "malloc error! file size is to big!!!!\n");
        return -1;
    }

    int ret2 = fread(file_buf, 1, len, stdin); //从标准输入(web服务器)读取内容
    if(ret2 == 0)
    {
        LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC, "fread(file_buf, 1, len, stdin) err\n");
        ret = -1;
        //善后
        free(file_buf);
        return ret;
    }

    //===========> 开始处理前端发送过来的post数据格式 <============
    begin = file_buf;    //内存起点
    p = begin;

    //get boundary 得到分界线, ------WebKitFormBoundary88asdgewtgewx
    p = strstr(begin, "\r\n");
    if (p == NULL)
    {
        LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC,"wrong no boundary!\n");
        ret = -1;
        //善后
        free(file_buf);
        return ret;
    }

    //拷贝分界线
    strncpy(boundary, begin, p-begin);
    boundary[p-begin] = '\0';   //字符串结束符
    //LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC,"boundary: [%s]\n", boundary);

    p += 2;//\r\n
    //已经处理了p-begin的长度
    len -= (p-begin);

    //get content text head
    begin = p;

    //Content-Disposition: form-data; user="mike"; filename="xxx.jpg"; md5="xxxx"; size=10240\r\n
    p = strstr(begin, "\r\n");
    if(p == NULL)
    {
        LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC,"ERROR: get context text error, no filename?\n");
        ret = -1;
        //善后
        free(file_buf);
        return ret;
    }

    strncpy(content_text, begin, p-begin);
    content_text[p-begin] = '\0';
    //LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC,"content_text: [%s]\n", content_text);

    p += 2;//\r\n
    len -= (p-begin);

    //========================================获取文件上传者
    //Content-Disposition: form-data; user="mike"; filename="xxx.jpg"; md5="xxxx"; size=10240\r\n
    //                                ↑
    q = begin;
    q = strstr(begin, "user=");

    //Content-Disposition: form-data; user="mike"; filename="xxx.jpg"; md5="xxxx"; size=10240\r\n
    //                                      ↑
    q += strlen("user=");
    q++;    //跳过第一个"

    //Content-Disposition: form-data; user="mike"; filename="xxx.jpg"; md5="xxxx"; size=10240\r\n
    //                                          ↑
    k = strchr(q, '"');
    strncpy(user, q, k-q);  //拷贝用户名
    user[k-q] = '\0';

    //去掉一个字符串两边的空白字符
    util_cgi::trim_space(user);   //util_cgi.h

    //========================================获取文件名字
    //"; filename="xxx.jpg"; md5="xxxx"; size=10240\r\n
    //   ↑
    begin = k;
    q = begin;
    q = strstr(begin, "filename=");

    //"; filename="xxx.jpg"; md5="xxxx"; size=10240\r\n
    //             ↑
    q += strlen("filename=");
    q++;    //跳过第一个"

    //"; filename="xxx.jpg"; md5="xxxx"; size=10240\r\n
    //                    ↑
    k = strchr(q, '"');
    strncpy(filename, q, k-q);  //拷贝文件名
    filename[k-q] = '\0';

    util_cgi::trim_space(filename);   //util_cgi.h

    //========================================获取文件MD5码
    //"; md5="xxxx"; size=10240\r\n
    //   ↑
    begin = k;
    q = begin;
    q = strstr(begin, "md5=");

    //"; md5="xxxx"; size=10240\r\n
    //        ↑
    q += strlen("md5=");
    q++;    //跳过第一个"

    //"; md5="xxxx"; size=10240\r\n
    //            ↑
    k = strchr(q, '"');
    strncpy(md5, q, k-q);   //拷贝文件名
    md5[k-q] = '\0';

    util_cgi::trim_space(md5);    //util_cgi.h

    //========================================获取文件大小
    //"; size=10240\r\n
    //   ↑
    begin = k;
    q = begin;
    q = strstr(begin, "size=");

    //"; size=10240\r\n
    //        ↑
    q += strlen("size=");

    //"; size=10240\r\n
    //             ↑
    k = strstr(q, "\r\n");
    char tmp[256] = {0};
    strncpy(tmp, q, k-q);   //内容
    tmp[k-q] = '\0';

    size = strtol(tmp, NULL, 10); //字符串转long

    begin = p;
    p = strstr(begin, "\r\n");
    p += 4;//\r\n\r\n
    len -= (p-begin);


    //下面才是文件的真正内容
    begin = p;
    //find file's end
    p = util_cgi::memstr(begin, len, boundary);//util_cgi.h， 找文件结尾
    if (p == NULL)
    {
        LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC, "memstr(begin, len, boundary) error\n");
        ret = -1;
        //善后
        free(file_buf);
        return ret;
    }
    else
    {
        p = p - 2;//\r\n
    }

    //begin---> file_len = (p-begin)

    //=====> 此时begin-->p两个指针的区间就是post的文件二进制数据
    //======>将数据写入文件中,其中文件名也是从post数据解析得来  <===========

    int fd = 0;
    fd = open(filename, O_CREAT|O_WRONLY, 0644);
    if (fd < 0)
    {
        LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC,"open %s error\n", filename);
        ret = -1;
        //善后
        free(file_buf);
        return ret;
    }

    //ftruncate会将参数fd指定的文件大小改为参数length指定的大小
    ftruncate(fd, (p-begin));
    write(fd, begin, (p-begin));
    close(fd);

    free(file_buf);
    return ret;
}

/* -------------------------------------------*/
/**
 * @brief  将一个本地文件上传到 后台分布式文件系统中
 *
 * @param filename  (in) 本地文件的路径
 * @param fileid    (out)得到上传之后的文件ID路径
 *
 * @returns
 *      0 succ, -1 fail
 */
/* -------------------------------------------*/
int upload::upload_to_dstorage(){
    int ret = 0;

    pid_t pid;
    int fd[2];

    //无名管道的创建
    if (pipe(fd) < 0)
    {
        LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC,"pip error\n");
        ret = -1;
        //善后
        return ret;
    }

    //创建进程
    pid = fork();
    if (pid < 0)//进程创建失败
    {
        LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC,"fork error\n");
        ret = -1;
        //善后
        return ret;
    }

    if(pid == 0) //子进程
    {
        //关闭读端
        close(fd[0]);

        //将标准输出 重定向 写管道
        dup2(fd[1], STDOUT_FILENO); //dup2(fd[1], 1);


        //读取fdfs client 配置文件的路径
        char fdfs_cli_conf_path[256] = {0};
        cfg::get_cfg_value(CFG_PATH, "dfs_path", "client", fdfs_cli_conf_path);

        int aaa = 0;
        //通过execlp执行fdfs_upload_file
        aaa = execlp("fdfs_upload_file", "fdfs_upload_file", fdfs_cli_conf_path, filename, NULL);
        if(aaa == 0){
            LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC, "execlp fdfs_upload_file error  aaa = %d\n", aaa);
        }

        //执行失败
        //LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC, "execlp fdfs_upload_file error\n");

        close(fd[1]);
    }
    else //父进程
    {
        //关闭写端
        close(fd[1]);

        //从管道中去读数据
        read(fd[0], fileid, TEMP_BUF_MAX_LEN);

        //去掉一个字符串两边的空白字符
        util_cgi::trim_space(fileid);

        if (strlen(fileid) == 0)
        {
            LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC,"[upload FAILED!]\n");
            ret = -1;
            //善后
            return ret;
        }

        LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC, "get [%s] succ!\n", fileid);

        wait(NULL); //等待子进程结束，回收其资源
        close(fd[0]);
    }
}

/* -------------------------------------------*/
/**
 * @brief  封装文件存储在分布式系统中的 完整 url
 *
 * @param fileid        (in)    文件分布式id路径
 * @param fdfs_file_url (out)   文件的完整url地址
 *
 * @returns
 *      0 succ, -1 fail
 */
/* -------------------------------------------*/
int upload::make_file_url(){
    int ret = 0;

    char *p = NULL;
    char *q = NULL;
    char *k = NULL;

    char fdfs_file_stat_buf[TEMP_BUF_MAX_LEN] = {0};
    char fdfs_file_host_name[HOST_NAME_LEN] = {0};  //storage所在服务器ip地址

    pid_t pid;
    int fd[2];

     //无名管道的创建
    if (pipe(fd) < 0)
    {
        LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC, "pip error\n");
        ret = -1;
        //善后
        return ret;
    }

    //创建进程
    pid = fork();
    if (pid < 0)//进程创建失败
    {
        LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC,"fork error\n");
        ret = -1;
        //善后
        return ret;
    }

    if(pid == 0) //子进程
    {
        //关闭读端
        close(fd[0]);

        //将标准输出 重定向 写管道
        dup2(fd[1], STDOUT_FILENO); //dup2(fd[1], 1);

        //读取fdfs client 配置文件的路径
        char fdfs_cli_conf_path[256] = {0};
        cfg::get_cfg_value(CFG_PATH, "dfs_path", "client", fdfs_cli_conf_path);

        execlp("fdfs_file_info", "fdfs_file_info", fdfs_cli_conf_path, fileid, NULL);

        //执行失败
        //LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC, "execlp fdfs_file_info error\n");

        close(fd[1]);
    }
    else //父进程
    {
        //关闭写端
        close(fd[1]);

        //从管道中去读数据
        read(fd[0], fdfs_file_stat_buf, TEMP_BUF_MAX_LEN);
        //LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC, "get file_ip [%s] succ\n", fdfs_file_stat_buf);

        wait(NULL); //等待子进程结束，回收其资源
        close(fd[0]);

        //拼接上传文件的完整url地址
        p = strstr(fdfs_file_stat_buf, "source ip address: ");

        q = p + strlen("source ip address: ");
        k = strstr(q, "\n");

        strncpy(fdfs_file_host_name, q, k-q);
        fdfs_file_host_name[k-q] = '\0';

        

        //读取storage_web_server服务器的端口
        char storage_web_server_port[20] = {0};
        cfg::get_cfg_value(CFG_PATH, "storage_web_server", "port", storage_web_server_port);
        strcat(fdfs_file_url, "http://");

        /*
            nm,这里不知道为什么从管道里读出来的是云服务器的内网地址，我配置文件写的是公网地址，
            但他偏偏就是成了内网地址，弄得下载功能一直用不了，我直接暴力修改了，暂时不管那么多，
            让项目先跑起来再说吧，等有空再回来改
        */
        //strcat(fdfs_file_url, fdfs_file_host_name);
        strcat(fdfs_file_url, "101.42.137.149");
        strcat(fdfs_file_url, ":");
        strcat(fdfs_file_url, storage_web_server_port);
        strcat(fdfs_file_url, "/");
        strcat(fdfs_file_url, fileid);

        //printf("[%s]\n", fdfs_file_url);
        LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC, "file url is: %s\n", fdfs_file_url);
    }
}

int upload::store_fileinfo_to_mysql(){
    int ret = 0;

    time_t now;;
    char create_time[TIME_STRING_LEN];
    char suffix[SUFFIX_LEN];
    char sql_cmd[SQL_MAX_LEN] = {0};

    //连接 mysql 数据库
    conn = deal_mysql::msql_conn(mysql_user, mysql_pwd, mysql_db);
    if (conn == NULL)
    {
        LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC, "msql_conn connect err\n");
        ret = -1;
        //善后
        if (conn != NULL)
        {
            mysql_close(conn); //断开数据库连接
        }

        return ret;

    }

    //设置数据库编码
    mysql_query(conn, "set names utf8");

    //得到文件后缀字符串 如果非法文件后缀,返回"null"
    util_cgi::get_file_suffix(filename, suffix); //mp4, jpg, png

    //sql 语句
    sprintf(sql_cmd, "insert into file_info (md5, file_id, url, size, type, count) values ('%s', '%s', '%s', '%ld', '%s', %d)",
            md5, fileid, fdfs_file_url, size, suffix, 1);

    if (mysql_query(conn, sql_cmd) != 0) //执行sql语句
    {
        //print_error(conn, "插入失败");
        LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC, "%s 插入失败: %s\n", sql_cmd, mysql_error(conn));
        ret = -1;
        //善后
        if (conn != NULL)
        {
            mysql_close(conn); //断开数据库连接
        }

        return ret;

    }

    LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC, "%s 文件信息插入成功\n", sql_cmd);

    //获取当前时间
    now = time(NULL);
    strftime(create_time, TIME_STRING_LEN-1, "%Y-%m-%d %H:%M:%S", localtime(&now));

    //sql语句
    sprintf(sql_cmd, "insert into user_file_list(user, md5, createtime, filename, shared_status, pv) values ('%s', '%s', '%s', '%s', %d, %d)", user, md5, create_time, filename, 0, 0);
    if(mysql_query(conn, sql_cmd) != 0)
    {
        LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC, "%s 操作失败: %s\n", sql_cmd, mysql_error(conn));
        ret = -1;
        //善后
        if (conn != NULL)
        {
            mysql_close(conn); //断开数据库连接
        }

        return ret;

    }

     //查询用户文件数量
    sprintf(sql_cmd, "select count from user_file_count where user = '%s'", user);
    int ret2 = 0;
    char tmp[512] = {0};
    int count = 0;
    //返回值： 0成功并保存记录集，1没有记录集，2有记录集但是没有保存，-1失败
    ret2 = deal_mysql::process_result_one(conn, sql_cmd, tmp); //执行sql语句
    if(ret2 == 1) //没有记录
    {
        //插入记录
        sprintf(sql_cmd, " insert into user_file_count (user, count) values('%s', %d)", user, 1);
    }
    else if(ret2 == 0)
    {
        //更新用户文件数量count字段
        count = atoi(tmp);
        sprintf(sql_cmd, "update user_file_count set count = %d where user = '%s'", count+1, user);
    }

    if(mysql_query(conn, sql_cmd) != 0)
    {
        LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC, "%s 操作失败: %s\n", sql_cmd, mysql_error(conn));
        ret = -1;
        //善后
        if (conn != NULL)
        {
            mysql_close(conn); //断开数据库连接
        }

        return ret;

    }

    if (conn != NULL)
    {
        mysql_close(conn); //断开数据库连接
    }

    return ret;
}