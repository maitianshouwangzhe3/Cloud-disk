#include "sharefiles_cgi.h"

void sharefiles::aftermath(int ret, char* out2, MYSQL_RES* res_set, redisContext* redis_conn, RVALUES value, cJSON* root, char* out){
    if(ret == 0)
    {
        cout << out; //给前端反馈信息
    }
    else
    {   //失败
        out2 = NULL;
        out2 = util_cgi::return_status("015");
    }
    if(out2 != NULL)
    {
        cout << out2; //给前端反馈错误码
        free(out2);
    }

    if(res_set != NULL)
    {
        //完成所有对数据的操作后，调用mysql_free_result来善后处理
        mysql_free_result(res_set);
    }

    if(conn != NULL)
    {
        mysql_close(conn);
    }

    if(root != NULL)
    {
        cJSON_Delete(root);
    }

    if(out != NULL)
    {
        free(out);
    }
}

//善后处理
void sharefiles::aftermath(cJSON *root, char* out, char* out2, MYSQL_RES* res_set, int ret){
    if(ret == 0)
    {
        cout << out; //给前端反馈信息
    }
    else
    {   //失败
        out2 = NULL;
        out2 = util_cgi::return_status("015");
    }
    if(out2 != NULL)
    {
        cout << out2; //给前端反馈错误码
        free(out2);
    }

    if(res_set != NULL)
    {
        //完成所有对数据的操作后，调用mysql_free_result来善后处理
        mysql_free_result(res_set);
    }

    if(conn != NULL)
    {
        mysql_close(conn);
    }

    if(root != NULL)
    {
        cJSON_Delete(root);
    }

    if(out != NULL)
    {
        free(out);
    }
}

sharefiles::sharefiles(){
    memset(mysql_user, 0, sizeof(mysql_user));
    memset(mysql_pwd, 0, sizeof(mysql_pwd));
    memset(mysql_db, 0, sizeof(mysql_db));
    memset(redis_ip, 0, sizeof(redis_ip));
    memset(redis_port, 0, sizeof(redis_port));
    memset(cmd, 0, sizeof(cmd));

    buf = new char[4 * 1024];


    len = 0;
    conn = nullptr;

    cin_streambuf = cin.rdbuf();
    cout_streambuf = cout.rdbuf();
    cerr_streambuf = cerr.rdbuf();

    FCGX_Init();
    FCGX_InitRequest(&request,0,0);
}

sharefiles::~sharefiles(){
    cin.rdbuf(cin_streambuf);
    cout.rdbuf(cout_streambuf);
    cerr.rdbuf(cerr_streambuf);
    delete [] buf;

    if(conn != NULL)
    {
        mysql_close(conn); //断开数据库连接
    }
}

void sharefiles::run(){
    read_cfg();
    fcgi_streambuf cin_fcgi_streambuf(request.in);
    fcgi_streambuf cout_fcgi_streambuf(request.out);
    fcgi_streambuf cerr_fcgi_streambuf(request.err);

    cin.rdbuf(&cin_fcgi_streambuf);
    cout.rdbuf(&cout_fcgi_streambuf);
    cerr.rdbuf(&cerr_fcgi_streambuf);
    while(FCGX_Accept_r(&request) == 0){
        
        // 获取URL地址 "?" 后面的内容
        char* query = getenv("QUERY_STRING");

        //解析命令
        util_cgi::query_parse_key_value(query, "cmd", cmd, NULL);
        LOG(SHAREFILES_LOG_MODULE, SHAREFILES_LOG_PROC, "cmd = %s\n", cmd);

        cout << "Content-type: text/html\r\n\r\n";

        if (strcmp(cmd, "count") == 0) //count 获取用户文件个数
        {
            get_share_files_count(); //获取共享文件个数
        }
        else{
            char * contentLength = FCGX_GetParam("CONTENT_LENGTH", request.envp);
            
            if(contentLength != nullptr){
                len = atoi(contentLength);
            }

            if(len <= 0){
                cout << "No data from standard input.<p>\n";
                LOG(SHAREFILES_LOG_MODULE, SHAREFILES_LOG_PROC, "len = 0, No data from standard input\n");
            }
            else{
                int ret = 0;
                cin.read(buf, len);

                LOG(SHAREFILES_LOG_MODULE, SHAREFILES_LOG_PROC, "buf = %s\n", buf);

                //获取共享文件信息 127.0.0.1:80/sharefiles&cmd=normal
                //按下载量升序 127.0.0.1:80/sharefiles?cmd=pvasc
                //按下载量降序127.0.0.1:80/sharefiles?cmd=pvdesc

                int start; //文件起点
                int count; //文件个数
                get_fileslist_json_info(&start, &count); //通过json包获取信息
                LOG(SHAREFILES_LOG_MODULE, SHAREFILES_LOG_PROC, "start = %d, count = %d\n", start, count);
                if (strcmp(cmd, "normal") == 0)
                {
                    get_share_filelist(start, count); //获取共享文件列表
                }
                else if(strcmp(cmd, "pvdesc") == 0)
                {
                    get_ranking_filelist(start, count);//获取共享文件排行版
                }
            }
        }
    }
}

void sharefiles::Init(){
    memset(cmd, 0, sizeof(cmd));
}

void sharefiles::read_cfg(){
    //读取mysql数据库配置信息
    cfg::get_cfg_value(CFG_PATH, "mysql", "user", mysql_user);
    cfg::get_cfg_value(CFG_PATH, "mysql", "password", mysql_pwd);
    cfg::get_cfg_value(CFG_PATH, "mysql", "database", mysql_db);
    LOG(SHAREFILES_LOG_MODULE, SHAREFILES_LOG_PROC, "mysql:[user=%s,pwd=%s,database=%s]", mysql_user, mysql_pwd, mysql_db);

    //读取redis配置信息
    cfg::get_cfg_value(CFG_PATH, "redis", "ip", redis_ip);
    cfg::get_cfg_value(CFG_PATH, "redis", "port", redis_port);
    LOG(SHAREFILES_LOG_MODULE, SHAREFILES_LOG_PROC, "redis:[ip=%s,port=%s]\n", redis_ip, redis_port);
}

int sharefiles::get_ranking_filelist(int start, int count){

     /*
    a) mysql共享文件数量和redis共享文件数量对比，判断是否相等
    b) 如果不相等，清空redis数据，从mysql中导入数据到redis (mysql和redis交互)
    c) 从redis读取数据，给前端反馈相应信息
    */
    int ret = 0;
    char sql_cmd[SQL_MAX_LEN] = {0};
    cJSON *root = NULL;
    RVALUES value = NULL;
    cJSON *array =NULL;
    char *out = NULL;
    char *out2 = NULL;
    char tmp[512] = {0};
    int ret2 = 0;
    MYSQL_RES *res_set = NULL;
    redisContext * redis_conn = NULL;
    redis_op rp;

    //连接redis数据库
    redis_conn = rp.rop_connectdb_nopwd(redis_ip, redis_port);
    if (redis_conn == NULL)
    {
        LOG(SHAREFILES_LOG_MODULE, SHAREFILES_LOG_PROC, "redis connected error");
        ret = -1;
        //善后
        aftermath(ret, out2, res_set, redis_conn, value, root, out);
        return ret;
    }

    //connect the database
    conn = deal_mysql::msql_conn(mysql_user, mysql_pwd, mysql_db);
    if (conn == NULL)
    {
        LOG(SHAREFILES_LOG_MODULE, SHAREFILES_LOG_PROC, "msql_conn err\n");
        ret = -1;
        //善后
        aftermath(ret, out2, res_set, redis_conn, value, root, out);
        return ret;
    }

    //设置数据库编码，主要处理中文编码问题
    mysql_query(conn, "set names utf8");

    //===1、mysql共享文件数量
    //sprintf(sql_cmd, "select count from user_file_count where user=\"%s\"", "xxx_share_xxx_file_xxx_list_xxx_count_xxx");
    sprintf(sql_cmd, "select count from user_file_count where user=\"%s\"", "lwx");
    ret2 = deal_mysql::process_result_one(conn, sql_cmd, tmp); //指向sql语句
    if(ret2 != 0)
    {
        LOG(SHAREFILES_LOG_MODULE, SHAREFILES_LOG_PROC, "%s 操作失败\n", sql_cmd);
        ret = -1;
        //善后
        aftermath(ret, out2, res_set, redis_conn, value, root, out);
        return ret;
    }

    int sql_num = atoi(tmp); //字符串转长整形

    //===2、redis共享文件数量
    int redis_num = rp.rop_zset_zcard(redis_conn, FILE_PUBLIC_ZSET);
    if(redis_num == -1)
    {
        LOG(SHAREFILES_LOG_MODULE, SHAREFILES_LOG_PROC, "rop_zset_zcard 操作失败\n");
        ret = -1;
        //善后
        aftermath(ret, out2, res_set, redis_conn, value, root, out);
        return ret;
    }

    LOG(SHAREFILES_LOG_MODULE, SHAREFILES_LOG_PROC, "sql_num = %d, redis_num = %d\n", sql_num, redis_num);

    //===3、mysql共享文件数量和redis共享文件数量对比，判断是否相等
    if(redis_num != sql_num)
    {//===4、如果不相等，清空redis数据，重新从mysql中导入数据到redis (mysql和redis交互)

        //a) 清空redis有序数据
        rp.rop_del_key(redis_conn, FILE_PUBLIC_ZSET);
        rp.rop_del_key(redis_conn, FILE_NAME_HASH);

        //b) 从mysql中导入数据到redis
        //sql语句
        strcpy(sql_cmd, "select md5, filename, pv from share_file_list order by pv desc");

        LOG(SHAREFILES_LOG_MODULE, SHAREFILES_LOG_PROC, "%s 在操作\n", sql_cmd);

        if (mysql_query(conn, sql_cmd) != 0)
        {
            LOG(SHAREFILES_LOG_MODULE, SHAREFILES_LOG_PROC, "%s 操作失败: %s\n", sql_cmd, mysql_error(conn));
            ret = -1;
            //善后
            aftermath(ret, out2, res_set, redis_conn, value, root, out);
            return ret;
        }

        res_set = mysql_store_result(conn);/*生成结果集*/
        if (res_set == NULL)
        {
            LOG(SHAREFILES_LOG_MODULE, SHAREFILES_LOG_PROC, "smysql_store_result error!\n");
            ret = -1;
            //善后
            aftermath(ret, out2, res_set, redis_conn, value, root, out);
            return ret;
        }

        long line = 0;
        //mysql_num_rows接受由mysql_store_result返回的结果结构集，并返回结构集中的行数
        line = mysql_num_rows(res_set);
        if (line == 0)
        {
            LOG(SHAREFILES_LOG_MODULE, SHAREFILES_LOG_PROC, "mysql_num_rows(res_set) failed\n");
            ret = -1;
            //善后
            aftermath(ret, out2, res_set, redis_conn, value, root, out);
            return ret;
        }

         MYSQL_ROW row;
        // mysql_fetch_row从使用mysql_store_result得到的结果结构中提取一行，并把它放到一个行结构中。
        // 当数据用完或发生错误时返回NULL.
        while ((row = mysql_fetch_row(res_set)) != NULL)
        {
            //md5, filename, pv
            if(row[0] == NULL || row[1] == NULL || row[2] == NULL)
            {
                LOG(SHAREFILES_LOG_MODULE, SHAREFILES_LOG_PROC, "mysql_fetch_row(res_set)) failed\n");
                ret = -1;
                //善后
                aftermath(ret, out2, res_set, redis_conn, value, root, out);
                return ret;
            }

            char fileid[1024] = {0};
            sprintf(fileid, "%s%s", row[0], row[1]); //文件标示，md5+文件名

            //增加有序集合成员
            rp.rop_zset_add(redis_conn, FILE_PUBLIC_ZSET, atoi(row[2]), fileid);

            //增加hash记录
            rp.rop_hash_set(redis_conn, FILE_NAME_HASH, fileid, row[1]);
        }
    }

    //===5、从redis读取数据，给前端反馈相应信息
    //char value[count][1024];
    value  = (RVALUES)calloc(count, VALUES_ID_SIZE); //堆区请求空间
    if(value == NULL)
    {
        ret = -1;
        //善后
        aftermath(ret, out2, res_set, redis_conn, value, root, out);
        return ret;
    }

    int n = 0;
    int end = start + count - 1;//加载资源的结束位置
    //降序获取有序集合的元素
    ret = rp.rop_zset_zrevrange(redis_conn, FILE_PUBLIC_ZSET, start, end, value, &n);
    if(ret != 0)
    {
        LOG(SHAREFILES_LOG_MODULE, SHAREFILES_LOG_PROC, "rop_zset_zrevrange 操作失败\n");
        //善后
        aftermath(ret, out2, res_set, redis_conn, value, root, out);
        return ret;
    }

    root = cJSON_CreateObject();
    array = cJSON_CreateArray();
    //遍历元素个数
    for(int i = 0; i < n; ++i)
    {
        //array[i]:
        cJSON* item = cJSON_CreateObject();

        /*
        {
            "filename": "test.mp4",
            "pv": 0
        }
        */

        //-- filename 文件名字
        char filename[1024] = {0};
        ret = rp.rop_hash_get(redis_conn, FILE_NAME_HASH, value[i], filename);
        if(ret != 0)
        {
            LOG(SHAREFILES_LOG_MODULE, SHAREFILES_LOG_PROC, "rop_hash_get 操作失败\n");
            ret = -1;
            //善后
            aftermath(ret, out2, res_set, redis_conn, value, root, out);
            return ret;
        }
        cJSON_AddStringToObject(item, "filename", filename);


        //-- pv 文件下载量
        int score = rp.rop_zset_get_score(redis_conn, FILE_PUBLIC_ZSET, value[i]);
        if(score == -1)
        {
            LOG(SHAREFILES_LOG_MODULE, SHAREFILES_LOG_PROC, "rop_zset_get_score 操作失败\n");
            ret = -1;
            //善后
            aftermath(ret, out2, res_set, redis_conn, value, root, out);
            return ret;
        }
        cJSON_AddNumberToObject(item, "pv", score);


        cJSON_AddItemToArray(array, item);

    }

    cJSON_AddItemToObject(root, "files", array);

    out = cJSON_Print(root);

    LOG(SHAREFILES_LOG_MODULE, SHAREFILES_LOG_PROC, "%s\n", out);

    if(ret == 0)
    {
        cout << out; //给前端反馈信息
    }
    else
    {   //失败
        out2 = NULL;
        out2 = util_cgi::return_status("015");
    }
    if(out2 != NULL)
    {
        cout << out2; //给前端反馈错误码
        free(out2);
    }

    if(res_set != NULL)
    {
        //完成所有对数据的操作后，调用mysql_free_result来善后处理
        mysql_free_result(res_set);
    }

    if(conn != NULL)
    {
        mysql_close(conn);
    }

    if(root != NULL)
    {
        cJSON_Delete(root);
    }

    if(out != NULL)
    {
        free(out);
    }
    return ret;
}

int sharefiles::get_share_filelist(int start, int count){

    int ret = 0;
    char sql_cmd[SQL_MAX_LEN] = {0};
   
    cJSON *root = NULL;
    cJSON *array =NULL;
    char *out = NULL;
    char *out2 = NULL;
    MYSQL_RES *res_set = NULL;

     //connect the database
    conn = deal_mysql::msql_conn(mysql_user, mysql_pwd, mysql_db);
    if (conn == NULL)
    {
        LOG(SHAREFILES_LOG_MODULE, SHAREFILES_LOG_PROC, "msql_conn err\n");
        ret = -1;
        //善后
        aftermath(root, out, out2, res_set, ret);
        return ret;
    }

    //设置数据库编码，主要处理中文编码问题
    mysql_query(conn, "set names utf8");


    //sql语句
    sprintf(sql_cmd, "select share_file_list.*, file_info.url, file_info.size, file_info.type from file_info, share_file_list where file_info.md5 = share_file_list.md5 limit %d, %d", start, count);

    LOG(SHAREFILES_LOG_MODULE, SHAREFILES_LOG_PROC, "%s 在操作\n", sql_cmd);

    if (mysql_query(conn, sql_cmd) != 0)
    {
        LOG(SHAREFILES_LOG_MODULE, SHAREFILES_LOG_PROC, "%s 操作失败: %s\n", sql_cmd, mysql_error(conn));
        ret = -1;
        //善后
        aftermath(root, out, out2, res_set, ret);
        return ret;
    }

    res_set = mysql_store_result(conn);/*生成结果集*/
    if (res_set == NULL)
    {
        LOG(SHAREFILES_LOG_MODULE, SHAREFILES_LOG_PROC, "smysql_store_result error!\n");
        ret = -1;
        //善后
        aftermath(root, out, out2, res_set, ret);
        return ret;
    }

    long line = 0;
    //mysql_num_rows接受由mysql_store_result返回的结果结构集，并返回结构集中的行数
    line = mysql_num_rows(res_set);
    if (line == 0)
    {
        LOG(SHAREFILES_LOG_MODULE, SHAREFILES_LOG_PROC, "mysql_num_rows(res_set) failed\n");
        ret = -1;
        //善后
        aftermath(root, out, out2, res_set, ret);
        return ret;
    }

    MYSQL_ROW row;

    root = cJSON_CreateObject();
    array = cJSON_CreateArray();

    // mysql_fetch_row从使用mysql_store_result得到的结果结构中提取一行，并把它放到一个行结构中。
    // 当数据用完或发生错误时返回NULL.
    while ((row = mysql_fetch_row(res_set)) != NULL)
    {
        //array[i]:
        cJSON* item = cJSON_CreateObject();
        //-- user	文件所属用户
        if(row[0] != NULL)
        {
            cJSON_AddStringToObject(item, "user", row[0]);
        }

        //-- md5 文件md5
        if(row[1] != NULL)
        {
            cJSON_AddStringToObject(item, "md5", row[1]);
        }

        //-- createtime 文件创建时间
        if(row[2] != NULL)
        {
            cJSON_AddStringToObject(item, "time", row[2]);
        }

        //-- filename 文件名字
        if(row[3] != NULL)
        {
            cJSON_AddStringToObject(item, "filename", row[3]);
        }

        //-- shared_status 共享状态, 0为没有共享， 1为共享
        cJSON_AddNumberToObject(item, "share_status", 1);


        //-- pv 文件下载量，默认值为0，下载一次加1
        if(row[4] != NULL)
        {
            cJSON_AddNumberToObject(item, "pv", atol( row[4] ));
        }

        //-- url 文件url
        if(row[5] != NULL)
        {
            cJSON_AddStringToObject(item, "url", row[5]);
        }

        //-- size 文件大小, 以字节为单位
        if(row[6] != NULL)
        {
            cJSON_AddNumberToObject(item, "size", atol( row[6] ));
        }

        //-- type 文件类型： png, zip, mp4……
        if(row[7] != NULL)
        {
            cJSON_AddStringToObject(item, "type", row[7]);
        }

        cJSON_AddItemToArray(array, item);
    }

    cJSON_AddItemToObject(root, "files", array);

    out = cJSON_Print(root);

    LOG(SHAREFILES_LOG_MODULE, SHAREFILES_LOG_PROC, "%s\n", out);

    aftermath(root, out, out2, res_set, ret);

    return ret;

}

int sharefiles::get_fileslist_json_info(int *p_start, int *p_count){
     int ret = 0;

    /*json数据如下
    {
        "start": 0
        "count": 10
    }
    */

    //解析json包
    //解析一个json字符串为cJSON对象
    cJSON * root = cJSON_Parse(buf);

    if(NULL == root)
    {
        LOG(SHAREFILES_LOG_MODULE, SHAREFILES_LOG_PROC, "cJSON_Parse err\n");
        ret = -1;
        //善后
        if(root != NULL)
        {
            cJSON_Delete(root);//删除json对象
            root = NULL;
        }

        return ret;
    }

    //文件起点
    cJSON *child2 = cJSON_GetObjectItem(root, "start");
    if(NULL == child2)
    {
        LOG(SHAREFILES_LOG_MODULE, SHAREFILES_LOG_PROC, "cJSON_GetObjectItem err\n");
        ret = -1;
        //善后
        if(root != NULL)
        {
            cJSON_Delete(root);//删除json对象
            root = NULL;
        }

        return ret;
    }

    *p_start = child2->valueint;

    //文件请求个数
    cJSON *child3 = cJSON_GetObjectItem(root, "count");
    if(NULL == child3)
    {
        LOG(SHAREFILES_LOG_MODULE, SHAREFILES_LOG_PROC, "cJSON_GetObjectItem err\n");
        ret = -1;
        //善后
        if(root != NULL)
        {
            cJSON_Delete(root);//删除json对象
            root = NULL;
        }

        return ret;
    }

    *p_count = child3->valueint;
    if(root != NULL)
    {
        cJSON_Delete(root);//删除json对象
        root = NULL;
    }

    return ret;
}

void sharefiles::get_share_files_count(){
    char sql_cmd[SQL_MAX_LEN] = {0};
   
    long line = 0;

    //connect the database
    conn = deal_mysql::msql_conn(mysql_user, mysql_pwd, mysql_db);
    if (conn == NULL)
    {
        LOG(SHAREFILES_LOG_MODULE, SHAREFILES_LOG_PROC, "msql_conn err\n");
        if(conn != NULL)
        {
            mysql_close(conn);
        }

        LOG(SHAREFILES_LOG_MODULE, SHAREFILES_LOG_PROC, "line = %ld\n", line);
        cout << line; //给前端反馈的信息
        return;
    }

    //设置数据库编码，主要处理中文编码问题
    mysql_query(conn, "set names utf8");

    //sprintf(sql_cmd, "select count from user_file_count where user=\"%s\"", "xxx_share_xxx_file_xxx_list_xxx_count_xxx");
    sprintf(sql_cmd, "select count from user_file_count where user=\"%s\"", "lwx");
    char tmp[512] = {0};
    int ret2 = deal_mysql::process_result_one(conn, sql_cmd, tmp); //指向sql语句
    if(ret2 != 0)
    {
        LOG(SHAREFILES_LOG_MODULE, SHAREFILES_LOG_PROC, "%s 操作失败  ret2 = %d\n", sql_cmd, ret2);
        if(conn != NULL)
        {
            mysql_close(conn);
        }

        LOG(SHAREFILES_LOG_MODULE, SHAREFILES_LOG_PROC, "line = %ld\n", line);
        cout << line; //给前端反馈的信息
        return;
    }

    line = atol(tmp); //字符串转长整形


    if(conn != NULL)
    {
        mysql_close(conn);
    }

    LOG(SHAREFILES_LOG_MODULE, SHAREFILES_LOG_PROC, "line = %ld\n", line);
    cout << line; //给前端反馈的信息
}