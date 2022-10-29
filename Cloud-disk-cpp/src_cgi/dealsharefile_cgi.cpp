#include "dealsharefile_cgi.h"



dealsharefile::dealsharefile(){
    memset(mysql_user, 0, sizeof(mysql_user));
    memset(mysql_pwd, 0, sizeof(mysql_pwd));
    memset(mysql_db, 0, sizeof(mysql_db));
    memset(redis_ip, 0, sizeof(redis_ip));
    memset(redis_port, 0, sizeof(redis_port));

    len = 0;

    conn = nullptr;

    cin_streambuf = cin.rdbuf();
    cout_streambuf = cout.rdbuf();
    cerr_streambuf = cerr.rdbuf();

    FCGX_Init();
    FCGX_InitRequest(&request, 0, 0);

}

dealsharefile::~dealsharefile(){
    cin.rdbuf(cin_streambuf);
    cout.rdbuf(cout_streambuf);
    cerr.rdbuf(cerr_streambuf);
    if(conn != nullptr){
        mysql_close(conn);
    }
}

void dealsharefile::run(){
    read_cfg();
    while(FCGX_Accept_r(&request) == 0){
        Init();
        fcgi_streambuf cin_fcgi_streambuf(request.in);
        fcgi_streambuf cout_fcgi_streambuf(request.out);
        fcgi_streambuf cerr_fcgi_streambuf(request.err);

        cin.rdbuf(&cin_fcgi_streambuf);
        cout.rdbuf(&cout_fcgi_streambuf);
        cerr.rdbuf(&cerr_fcgi_streambuf);

        char * query = FCGX_GetParam("QUERY_STRING", request.envp);

        //解析命令
        util_cgi::query_parse_key_value(query, "cmd", cmd, NULL);
        LOG(DEALSHAREFILE_LOG_MODULE, DEALSHAREFILE_LOG_PROC, "cmd = %s\n", cmd);

        char *contentLength = FCGX_GetParam("CONTENT_LENGTH", request.envp);

        cout << "Content-type: text/html\r\n\r\n";

        if(contentLength != nullptr){
            len = atoi(contentLength); //字符串转整型
        }

        if(len <= 0){
            cout << "No data from standard input.<p>\n";
            LOG(DEALSHAREFILE_LOG_MODULE, DEALSHAREFILE_LOG_PROC, "len = 0, No data from standard input\n");
        }

        //int ret = 0;

        cin.read(buf, len);
        LOG(DEALSHAREFILE_LOG_MODULE, DEALSHAREFILE_LOG_PROC, "buf = %s\n", buf);

        get_json_info(); //解析json信息
        LOG(DEALSHAREFILE_LOG_MODULE, DEALSHAREFILE_LOG_PROC, "user = %s, md5 = %s, filename = %s\n", user, md5, filename);

        if(strcmp(cmd, "pv") == 0) //文件下载标志处理
            {
                pv_file();
            }
            else if(strcmp(cmd, "cancel") == 0) //取消分享文件
            {
                cancel_share_file();
            }
            else if(strcmp(cmd, "save") == 0) //转存文件
            {
                save_file();
            }


    }
}

void dealsharefile::read_cfg(){
    //读取mysql数据库配置信息
    cfg::get_cfg_value(CFG_PATH, "mysql", "user", mysql_user);
    cfg::get_cfg_value(CFG_PATH, "mysql", "password", mysql_pwd);
    cfg::get_cfg_value(CFG_PATH, "mysql", "database", mysql_db);
    LOG(DEALSHAREFILE_LOG_MODULE, DEALSHAREFILE_LOG_PROC, "mysql:[user=%s,pwd=%s,database=%s]", mysql_user, mysql_pwd, mysql_db);

    //读取redis配置信息
    cfg::get_cfg_value(CFG_PATH, "redis", "ip", redis_ip);
    cfg::get_cfg_value(CFG_PATH, "redis", "port", redis_port);
    LOG(DEALSHAREFILE_LOG_MODULE, DEALSHAREFILE_LOG_PROC, "redis:[ip=%s,port=%s]\n", redis_ip, redis_port);
}

void dealsharefile::Init(){
    memset(cmd, 0, sizeof(cmd));
    memset(user, 0, sizeof(user));
    
    memset(filename, 0, sizeof(filename));
    memset(md5, 0, sizeof(md5));
    memset(buf, 0, sizeof(buf));
}

int dealsharefile::save_file(){
    int ret = 0;
    char sql_cmd[SQL_MAX_LEN] = {0};
    
    //char *out = NULL;
    int ret2 = 0;
    char tmp[512] = {0};
    
    //connect the database
    conn = deal_mysql::msql_conn(mysql_user, mysql_pwd, mysql_db);
    if (conn == NULL)
    {
        LOG(DEALSHAREFILE_LOG_MODULE, DEALSHAREFILE_LOG_PROC, "msql_conn err\n");
        ret = -1;
        //善后
        aftermath(ret);
        return ret;
    }

    //设置数据库编码，主要处理中文编码问题
    mysql_query(conn, "set names utf8");

    //查看此用户，文件名和md5是否存在，如果存在说明此文件存在
    sprintf(sql_cmd, "select * from user_file_list where user = '%s' and md5 = '%s' and filename = '%s'", user, md5, filename);
    //返回值： 0成功并保存记录集，1没有记录集，2有记录集但是没有保存，-1失败
    ret2 = deal_mysql::process_result_one(conn, sql_cmd, NULL); //执行sql语句, 最后一个参数为NULL
    if(ret2 == 2) //如果有结果，说明此用户已有此文件
    {
        LOG(DEALSHAREFILE_LOG_MODULE, DEALSHAREFILE_LOG_PROC, "%s[filename:%s, md5:%s]已存在\n", user, filename, md5);
        ret = -2; //返回-2错误码
        //善后
        aftermath(ret);
        return ret;
    }

    //文件信息表，查找该文件的计数器
    sprintf(sql_cmd, "select count from file_info where md5 = '%s'", md5);
    ret2 = deal_mysql::process_result_one(conn, sql_cmd, tmp); //执行sql语句
    if(ret2 != 0)
    {
        LOG(DEALSHAREFILE_LOG_MODULE, DEALSHAREFILE_LOG_PROC, "%s 操作失败\n", sql_cmd);
        ret = -1;
        //善后
        aftermath(ret);
        return ret;
    }

    int count = atoi(tmp); //字符串转整型，文件计数器
    //LOG(DEALSHAREFILE_LOG_MODULE, DEALSHAREFILE_LOG_PROC, "count = %s\n", tmp);

    //1、修改file_info中的count字段，+1 （count 文件引用计数）
    sprintf(sql_cmd, "update file_info set count = %d where md5 = '%s'", count+1, md5);
    if(mysql_query(conn, sql_cmd) != 0)
    {
        LOG(DEALSHAREFILE_LOG_MODULE, DEALSHAREFILE_LOG_PROC, "%s 操作失败\n", sql_cmd);
        ret = -1;
        //善后
        aftermath(ret);
        return ret;
    }

     //2、user_file_list插入一条数据
    //当前时间戳
    struct timeval tv;
    struct tm* ptm;
    char time_str[128];

    //使用函数gettimeofday()函数来得到时间。它的精度可以达到微妙
    gettimeofday(&tv, NULL);
    ptm = localtime(&tv.tv_sec);//把从1970-1-1零点零分到当前时间系统所偏移的秒数时间转换为本地时间
    //strftime() 函数根据区域设置格式化本地时间/日期，函数的功能将时间格式化，或者说格式化一个时间字符串
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", ptm);

    //sql语句
    /*
    -- =============================================== 用户文件列表
    -- user	文件所属用户
    -- md5 文件md5
    -- createtime 文件创建时间
    -- filename 文件名字
    -- shared_status 共享状态, 0为没有共享， 1为共享
    -- pv 文件下载量，默认值为0，下载一次加1
    */
    sprintf(sql_cmd, "insert into user_file_list(user, md5, createtime, filename, shared_status, pv) values ('%s', '%s', '%s', '%s', %d, %d)", user, md5, time_str, filename, 0, 0);
    if(mysql_query(conn, sql_cmd) != 0)
    {
        LOG(DEALSHAREFILE_LOG_MODULE, DEALSHAREFILE_LOG_PROC, "%s 操作失败\n", sql_cmd);
        ret = -1;
        //善后
        aftermath(ret);
        return ret;
    }

     //3、查询用户文件数量，更新该字段
    sprintf(sql_cmd, "select count from user_file_count where user = '%s'", user);
    count = 0;
    //返回值： 0成功并保存记录集，1没有记录集，2有记录集但是没有保存，-1失败
    ret2 = deal_mysql::process_result_one(conn, sql_cmd, tmp); //指向sql语句
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
        LOG(DEALSHAREFILE_LOG_MODULE, DEALSHAREFILE_LOG_PROC, "%s 操作失败\n", sql_cmd);
        ret = -1;
        //善后
        aftermath(ret);
        return ret;
    }


    aftermath(ret);
    return ret;
}

void dealsharefile::aftermath(int ret){
    /*
    返回值：0成功，-1转存失败，-2文件已存在
    转存文件：
        成功：{"code":"020"}
        文件已存在：{"code":"021"}
        失败：{"code":"022"}
    */
    char* out = NULL;
    if(ret == 0)
    {
        out = util_cgi::return_status("020");
    }
    else if(ret == -1)
    {
        out = util_cgi::return_status("022");
    }
    else if(ret == -2)
    {
        out = util_cgi::return_status("021");
    }

    if(out != NULL)
    {
        cout << out;//给前端反馈信息
        free(out);
    }


    if(conn != NULL)
    {
        mysql_close(conn);
    }
}

int dealsharefile::cancel_share_file(){
    int ret = 0;
    char sql_cmd[SQL_MAX_LEN] = {0};
    
    redisContext * redis_conn = NULL;
    //char *out = NULL;
    char fileid[1024] = {0};
    redis_op rp;

    //连接redis数据库
    redis_conn = rp.rop_connectdb_nopwd(redis_ip, redis_port);
    if (redis_conn == NULL)
    {
        LOG(DEALSHAREFILE_LOG_MODULE, DEALSHAREFILE_LOG_PROC, "redis connected error");
        ret = -1;
        //善后
        aftermath2(ret, redis_conn, rp);
        return ret;
    }

    //connect the database
    conn = deal_mysql::msql_conn(mysql_user, mysql_pwd, mysql_db);
    if (conn == NULL)
    {
        LOG(DEALSHAREFILE_LOG_MODULE, DEALSHAREFILE_LOG_PROC, "msql_conn err\n");
        ret = -1;
        //善后
        aftermath2(ret, redis_conn, rp);
        return ret;
    }

    //设置数据库编码，主要处理中文编码问题
    mysql_query(conn, "set names utf8");

    //文件标示，md5+文件名
    sprintf(fileid, "%s%s", md5, filename);

    //===1、mysql记录操作
    //sql语句
    sprintf(sql_cmd, "update user_file_list set shared_status = 0 where user = '%s' and md5 = '%s' and filename = '%s'", user, md5, filename);

    if (mysql_query(conn, sql_cmd) != 0)
    {
        LOG(DEALSHAREFILE_LOG_MODULE, DEALSHAREFILE_LOG_PROC, "%s 操作失败\n", sql_cmd);
        ret = -1;
        //善后
        aftermath2(ret, redis_conn, rp);
        return ret;
    }

    //查询共享文件数量
    sprintf(sql_cmd, "select count from user_file_count where user = '%s'", "xxx_share_xxx_file_xxx_list_xxx_count_xxx");
    int count = 0;
    char tmp[512] = {0};
    int ret2 = deal_mysql::process_result_one(conn, sql_cmd, tmp); //执行sql语句
    if(ret2 != 0)
    {
        LOG(DEALSHAREFILE_LOG_MODULE, DEALSHAREFILE_LOG_PROC, "%s 操作失败\n", sql_cmd);
        ret = -1;
        //善后
        aftermath2(ret, redis_conn, rp);
        return ret;
    }

    //更新用户文件数量count字段
    count = atoi(tmp);
    if(count == 1)
    {
        //删除数据
        sprintf(sql_cmd, "delete from user_file_count where user = '%s'", "xxx_share_xxx_file_xxx_list_xxx_count_xxx");
    }
    else
    {
        sprintf(sql_cmd, "update user_file_count set count = %d where user = '%s'", count-1, "xxx_share_xxx_file_xxx_list_xxx_count_xxx");
    }

    if(mysql_query(conn, sql_cmd) != 0)
    {
        LOG(DEALSHAREFILE_LOG_MODULE, DEALSHAREFILE_LOG_PROC, "%s 操作失败\n", sql_cmd);
        ret = -1;
        //善后
        aftermath2(ret, redis_conn, rp);
        return ret;
    }

    //删除在共享列表的数据
    sprintf(sql_cmd, "delete from share_file_list where user = '%s' and md5 = '%s' and filename = '%s'", user, md5, filename);
    if(mysql_query(conn, sql_cmd) != 0)
    {
        LOG(DEALSHAREFILE_LOG_MODULE, DEALSHAREFILE_LOG_PROC, "%s 操作失败\n", sql_cmd);
        ret = -1;
        //善后
        aftermath2(ret, redis_conn, rp);
        return ret;
    }

    //===2、redis记录操作
    //有序集合删除指定成员
    ret = rp.rop_zset_zrem(redis_conn, FILE_PUBLIC_ZSET, fileid);
    if(ret != 0)
    {
        LOG(DEALSHAREFILE_LOG_MODULE, DEALSHAREFILE_LOG_PROC, "rop_zset_zrem 操作失败\n");
        //善后
        aftermath2(ret, redis_conn, rp);
        return ret;
    }

    //从hash移除相应记录
    ret = rp.rop_hash_del(redis_conn, FILE_NAME_HASH, fileid);
    if(ret != 0)
    {
        LOG(DEALSHAREFILE_LOG_MODULE, DEALSHAREFILE_LOG_PROC, "rop_hash_del 操作失败\n");
        //善后
        aftermath2(ret, redis_conn, rp);
        return ret;
    }

    aftermath2(ret, redis_conn, rp);
    return ret;

}

void dealsharefile::aftermath2(int ret, redisContext* redis_conn, redis_op& rp){
    /*
    取消分享：
        成功：{"code":"018"}
        失败：{"code":"019"}
    */
    char* out = NULL;
    if(ret == 0)
    {
        out = util_cgi::return_status("018");
    }
    else
    {
        out = util_cgi::return_status("019");
    }

    if(out != NULL)
    {
        cout << out;//给前端反馈信息
        free(out);
    }

    if(redis_conn != NULL)
    {
        rp.rop_disconnect(redis_conn);
    }


    if(conn != NULL)
    {
        mysql_close(conn);
    }

}

int dealsharefile::pv_file(){
    int ret = 0;
    char sql_cmd[SQL_MAX_LEN] = {0};
    
    //char *out = NULL;
    char tmp[512] = {0};
    int ret2 = 0;
    redisContext * redis_conn = NULL;
    char fileid[1024] = {0};

    redis_op rp;

    //连接redis数据库
    redis_conn = rp.rop_connectdb_nopwd(redis_ip, redis_port);
    if (redis_conn == NULL)
    {
        LOG(DEALSHAREFILE_LOG_MODULE, DEALSHAREFILE_LOG_PROC, "redis connected error");
        ret = -1;
        //善后
        aftermath(ret, redis_conn, rp);
        return ret;
    }

    //文件标示，md5+文件名
    sprintf(fileid, "%s%s", md5, filename);

    //connect the database
    conn = deal_mysql::msql_conn(mysql_user, mysql_pwd, mysql_db);
    if (conn == NULL)
    {
        LOG(DEALSHAREFILE_LOG_MODULE, DEALSHAREFILE_LOG_PROC, "msql_conn err\n");
        ret = -1;
        //善后
        aftermath(ret, redis_conn, rp);
        return ret;
    }

    //设置数据库编码，主要处理中文编码问题
    mysql_query(conn, "set names utf8");

    //===1、mysql的下载量+1(mysql操作)
    //sql语句
    //查看该共享文件的pv字段
    sprintf(sql_cmd, "select pv from share_file_list where md5 = '%s' and filename = '%s'", md5, filename);

    ret2 = deal_mysql::process_result_one(conn, sql_cmd, tmp); //执行sql语句
    int pv = 0;
    if(ret2 == 0)
    {
        pv = atoi(tmp); //pv字段
    }
    else
    {
        LOG(DEALSHAREFILE_LOG_MODULE, DEALSHAREFILE_LOG_PROC, "%s 操作失败\n", sql_cmd);
        ret = -1;
        //善后
        aftermath(ret, redis_conn, rp);
        return ret;
    }

    //更新该文件pv字段，+1
    sprintf(sql_cmd, "update share_file_list set pv = %d where md5 = '%s' and filename = '%s'", pv+1, md5, filename);

    if (mysql_query(conn, sql_cmd) != 0)
    {
        LOG(DEALSHAREFILE_LOG_MODULE, DEALSHAREFILE_LOG_PROC, "%s 操作失败\n", sql_cmd);
        ret = -1;
        //善后
        aftermath(ret, redis_conn, rp);
        return ret;
    }

    //===2、判断元素是否在集合中(redis操作)
    ret2 = rp.rop_zset_exit(redis_conn, FILE_PUBLIC_ZSET, fileid);
    if(ret2 == 1) //存在
    {//===3、如果存在，有序集合score+1
        ret = rp.rop_zset_increment(redis_conn, FILE_PUBLIC_ZSET, fileid);
        if(ret != 0)
        {
            LOG(DEALSHAREFILE_LOG_MODULE, DEALSHAREFILE_LOG_PROC, "rop_zset_increment 操作失败\n");
        }
    }
    else if(ret2 == 0) //不存在
    {//===4、如果不存在，从mysql导入数据
        //===5、redis集合中增加一个元素(redis操作)
        rp.rop_zset_add(redis_conn, FILE_PUBLIC_ZSET, pv+1, fileid);

        //===6、redis对应的hash也需要变化 (redis操作)
        //     fileid ------>  filename
        rp.rop_hash_set(redis_conn, FILE_NAME_HASH, fileid, filename);

    }
    else//出错
    {
        ret = -1;
        //善后
        aftermath(ret, redis_conn, rp);
        return ret;
    }

    aftermath(ret, redis_conn, rp);
    return ret;
}

void dealsharefile::aftermath(int ret, redisContext* redis_conn, redis_op& rp){
    /*
    下载文件pv字段处理
        成功：{"code":"016"}
        失败：{"code":"017"}
    */
    char* out = nullptr;
    if(ret == 0)
    {
        out = util_cgi::return_status("016");
    }
    else
    {
        out = util_cgi::return_status("017");
    }

    if(out != NULL)
    {
        cout << out;//给前端反馈信息
        free(out);
    }

    if(redis_conn != NULL)
    {
        rp.rop_disconnect(redis_conn);
    }


    if(conn != NULL)
    {
        mysql_close(conn);
    }
}

int dealsharefile::get_json_info(){
    int ret = 0;

    /*json数据如下
    {
    "user": "yoyo",
    "md5": "xxx",
    "filename": "xxx"
    }
    */

    //解析json包
    //解析一个json字符串为cJSON对象
    cJSON * root = cJSON_Parse(buf);
    if(NULL == root)
    {
        LOG(DEALSHAREFILE_LOG_MODULE, DEALSHAREFILE_LOG_PROC, "cJSON_Parse err\n");
        ret = -1;
        if(root != NULL)
        {
            cJSON_Delete(root);//删除json对象
            root = NULL;
        }

        return ret;
    }

    //返回指定字符串对应的json对象
    //用户
    cJSON *child1 = cJSON_GetObjectItem(root, "user");
    if(NULL == child1)
    {
        LOG(DEALSHAREFILE_LOG_MODULE, DEALSHAREFILE_LOG_PROC, "cJSON_GetObjectItem err\n");
        ret = -1;
        if(root != NULL)
        {
            cJSON_Delete(root);//删除json对象
            root = NULL;
        }

        return ret;
    }

    //LOG(DEALSHAREFILE_LOG_MODULE, DEALSHAREFILE_LOG_PROC, "child1->valuestring = %s\n", child1->valuestring);
    strcpy(user, child1->valuestring); //拷贝内容

    //文件md5码
    cJSON *child2 = cJSON_GetObjectItem(root, "md5");
    if(NULL == child2)
    {
        LOG(DEALSHAREFILE_LOG_MODULE, DEALSHAREFILE_LOG_PROC, "cJSON_GetObjectItem err\n");
        ret = -1;
        if(root != NULL)
        {
            cJSON_Delete(root);//删除json对象
            root = NULL;
        }

        return ret;
    }

    strcpy(md5, child2->valuestring); //拷贝内容

    //文件名字
    cJSON *child3 = cJSON_GetObjectItem(root, "filename");
    if(NULL == child3)
    {
        LOG(DEALSHAREFILE_LOG_MODULE, DEALSHAREFILE_LOG_PROC, "cJSON_GetObjectItem err\n");
        ret = -1;
        if(root != NULL)
        {
            cJSON_Delete(root);//删除json对象
            root = NULL;
        }

        return ret;
    }

    strcpy(filename, child3->valuestring); //拷贝内容



    if(root != NULL)
    {
        cJSON_Delete(root);//删除json对象
        root = NULL;
    }

    return ret;
}