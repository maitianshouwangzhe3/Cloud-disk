#include "myfiles_cgi.h"



myfiles::myfiles(){
    memset(mysql_user, 0, sizeof(mysql_user));
    memset(mysql_pwd, 0, sizeof(mysql_pwd));
    memset(mysql_db, 0, sizeof(mysql_db));
    len = 0;

    conn = nullptr;

    Init();

    cin_streambuf = cin.rdbuf();
    cout_streambuf = cout.rdbuf();
    cerr_streambuf = cerr.rdbuf();

    FCGX_Init();
    FCGX_InitRequest(&request,0,0);
}

myfiles::~myfiles(){
    cin.rdbuf(cin_streambuf);
    cout.rdbuf(cout_streambuf);
    cerr.rdbuf(cerr_streambuf);
    

    if(conn != NULL)
    {
        mysql_close(conn); //断开数据库连接
    }
}

void myfiles::run(){
    read_cfg();
    while(FCGX_Accept_r(&request) == 0){
        fcgi_streambuf cin_fcgi_streambuf(request.in);
        fcgi_streambuf cout_fcgi_streambuf(request.out);
        fcgi_streambuf cerr_fcgi_streambuf(request.err);

        cin.rdbuf(&cin_fcgi_streambuf);
        cout.rdbuf(&cout_fcgi_streambuf);
        cerr.rdbuf(&cerr_fcgi_streambuf);

        Init();

        // 获取URL地址 "?" 后面的内容
        char *query = getenv("QUERY_STRING");

        //解析命令
        util_cgi::query_parse_key_value(query, "cmd", cmd, NULL);
        LOG(MYFILES_LOG_MODULE, MYFILES_LOG_PROC, "cmd = %s\n", cmd);

        char *contentLength = getenv("CONTENT_LENGTH");

        cout << "Content-type: text/html\r\n\r\n";

        if(contentLength != NULL){
            len = atoi(contentLength);
        }

        if(len <= 0){
            cout << "No data from standard input.<p>\n";
            LOG(MYFILES_LOG_MODULE, MYFILES_LOG_PROC, "len = 0, No data from standard input\n");
        }
        else{
            int ret = 0;
            cin.read(buf, len); //从标准输入(web服务器)读取内容
            LOG(MYFILES_LOG_MODULE, MYFILES_LOG_PROC, "buf = %s\n", buf);

            if (strcmp(cmd, "count") == 0) //count 获取用户文件个数
            {
                get_count_json_info(user, token); //通过json包获取用户名, token

                //验证登陆token，成功返回0，失败-1
                ret = util_cgi::verify_token(user, token); //util_cgi.h

                get_user_files_count(user, ret); //获取用户文件个数

            }
            //获取用户文件信息 127.0.0.1:80/myfiles&cmd=normal
            //按下载量升序 127.0.0.1:80/myfiles?cmd=pvasc
            //按下载量降序127.0.0.1:80/myfiles?cmd=pvdesc
            else
            {
                int start; //文件起点
                int count; //文件个数
                get_fileslist_json_info(user, token, &start, &count); //通过json包获取信息
                LOG(MYFILES_LOG_MODULE, MYFILES_LOG_PROC, "user = %s, token = %s, start = %d, count = %d\n", user, token, start, count);

                //验证登陆token，成功返回0，失败-1
                ret = util_cgi::verify_token(user, token); //util_cgi.h
                if(ret == 0)
                {
                     get_user_filelist(cmd, user, start, count); //获取用户文件列表
                }
                else
                {
                    char *out = util_cgi::return_status("111"); //token验证失败错误码
                    if(out != NULL)
                    {
                        cout << out; //给前端反馈错误码
                        free(out);
                    }
                }

            }


        }
    }
}

void myfiles::Init(){
    memset(cmd, 0, sizeof(cmd));
    memset(user, 0, sizeof(user));
    memset(token, 0, sizeof(token));
    memset(buf, 0, sizeof(buf));
}

void myfiles::read_cfg(){
    //读取mysql数据库配置信息
    cfg::get_cfg_value(CFG_PATH, "mysql", "user", mysql_user);
    cfg::get_cfg_value(CFG_PATH, "mysql", "password", mysql_pwd);
    cfg::get_cfg_value(CFG_PATH, "mysql", "database", mysql_db);
    LOG(MYFILES_LOG_MODULE, MYFILES_LOG_PROC, "mysql:[user=%s,pwd=%s,database=%s]", mysql_user, mysql_pwd, mysql_db);

}

int myfiles::get_count_json_info(char *user, char *token){
    int ret = 0;

    /*json数据如下
    {
        "token": "9e894efc0b2a898a82765d0a7f2c94cb",
        user:xxxx
    }
    */

    //解析json包
    //解析一个json字符串为cJSON对象
    cJSON * root = cJSON_Parse(buf);
    if(NULL == root)
    {
        LOG(MYFILES_LOG_MODULE, MYFILES_LOG_PROC, "cJSON_Parse err\n");
        ret = -1;
        //善后
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
        LOG(MYFILES_LOG_MODULE, MYFILES_LOG_PROC, "cJSON_GetObjectItem err\n");
        ret = -1;
        //善后
        if(root != NULL)
        {
            cJSON_Delete(root);//删除json对象
            root = NULL;
        }

        return ret;
    }

    LOG(MYFILES_LOG_MODULE, MYFILES_LOG_PROC, "child1->valuestring = %s\n", child1->valuestring);
    strcpy(user, child1->valuestring); //拷贝内容

    //登陆token
    cJSON *child2 = cJSON_GetObjectItem(root, "token");
    if(NULL == child2)
    {
        LOG(MYFILES_LOG_MODULE, MYFILES_LOG_PROC, "cJSON_GetObjectItem err\n");
        ret = -1;
        //善后
        if(root != NULL)
        {
            cJSON_Delete(root);//删除json对象
            root = NULL;
        }

        return ret;
    }

    LOG(MYFILES_LOG_MODULE, MYFILES_LOG_PROC, "child2->valuestring = %s\n", child2->valuestring);
    strcpy(token, child2->valuestring); //拷贝内容

    if(root != NULL)
    {
        cJSON_Delete(root);//删除json对象
        root = NULL;
    }

    return ret;
}

void myfiles::return_login_status(long num, int token_flag){
    char *out = NULL;
    char *token;
    char num_buf[128] = {0};

    if(token_flag == 0)
    {
        token = "110"; //成功
    }
    else
    {
        token = "111"; //失败
    }

    //数字
    sprintf(num_buf, "%ld", num);

    cJSON *root = cJSON_CreateObject();  //创建json项目
    cJSON_AddStringToObject(root, "num", num_buf);// {"num":"1111"}
    cJSON_AddStringToObject(root, "code", token);// {"code":"110"}
    out = cJSON_Print(root);//cJSON to string(char *)

    cJSON_Delete(root);

    if(out != NULL)
    {
        cout << out; //给前端反馈信息
        free(out); //记得释放
    }
}

void myfiles::get_user_files_count(char *user, int ret){

    char sql_cmd[SQL_MAX_LEN] = {0};
    long line = 0;

    //connect the database
    conn = deal_mysql::msql_conn(mysql_user, mysql_pwd, mysql_db);
    if (conn == NULL)
    {
        LOG(MYFILES_LOG_MODULE, MYFILES_LOG_PROC, "msql_conn err\n");
        //善后
        if(conn != NULL)
        {
            mysql_close(conn);
        }

        LOG(MYFILES_LOG_MODULE, MYFILES_LOG_PROC, "line = %ld\n", line);

        //给前端反馈的信息
        return_login_status(line, ret);
        return;
    }

    //设置数据库编码，主要处理中文编码问题
    mysql_query(conn, "set names utf8");

    sprintf(sql_cmd, "select count from user_file_count where user=\"%s\"", user);
    char tmp[512] = {0};
    //返回值： 0成功并保存记录集，1没有记录集，2有记录集但是没有保存，-1失败
    int ret2 = deal_mysql::process_result_one(conn, sql_cmd, tmp); //指向sql语句
    LOG(MYFILES_LOG_MODULE, MYFILES_LOG_PROC, "ret2=%d\n", ret2);
    if(ret2 != 0)
    {
        LOG(MYFILES_LOG_MODULE, MYFILES_LOG_PROC, "%s 操作失败\n", sql_cmd);
        //善后
        if(conn != NULL)
        {
            mysql_close(conn);
        }

        LOG(MYFILES_LOG_MODULE, MYFILES_LOG_PROC, "line = %ld\n", line);

        //给前端反馈的信息
        return_login_status(line, ret);
        return;
    }

    line = atol(tmp); //字符串转长整形

    if(conn != NULL)
    {
        mysql_close(conn);
    }

    LOG(MYFILES_LOG_MODULE, MYFILES_LOG_PROC, "line = %ld\n", line);

    //给前端反馈的信息
    return_login_status(line, ret);

}

int myfiles::get_fileslist_json_info(char *user, char *token, int *p_start, int *p_count){

    int ret = 0;

    /*json数据如下
    {
        "user": "yoyo"
        "token": xxxx
        "start": 0
        "count": 10
    }
    */

    //解析json包
    //解析一个json字符串为cJSON对象
    cJSON * root = cJSON_Parse(buf);
    if(NULL == root)
    {
        LOG(MYFILES_LOG_MODULE, MYFILES_LOG_PROC, "cJSON_Parse err\n");
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
        LOG(MYFILES_LOG_MODULE, MYFILES_LOG_PROC, "cJSON_GetObjectItem err\n");
        ret = -1;
        if(root != NULL)
        {
            cJSON_Delete(root);//删除json对象
            root = NULL;
        }

        return ret;
    }

    //LOG(MYFILES_LOG_MODULE, MYFILES_LOG_PROC, "child1->valuestring = %s\n", child1->valuestring);
    strcpy(user, child1->valuestring); //拷贝内容

    //token
    cJSON *child2 = cJSON_GetObjectItem(root, "token");
    if(NULL == child2)
    {
        LOG(MYFILES_LOG_MODULE, MYFILES_LOG_PROC, "cJSON_GetObjectItem err\n");
        ret = -1;
        if(root != NULL)
        {
            cJSON_Delete(root);//删除json对象
            root = NULL;
        }

        return ret;
    }

    strcpy(token, child2->valuestring); //拷贝内容

    //文件起点
    cJSON *child3 = cJSON_GetObjectItem(root, "start");
    if(NULL == child3)
    {
        LOG(MYFILES_LOG_MODULE, MYFILES_LOG_PROC, "cJSON_GetObjectItem err\n");
        ret = -1;
        if(root != NULL)
        {
            cJSON_Delete(root);//删除json对象
            root = NULL;
        }

        return ret;
    }

    *p_start = child3->valueint;

    //文件请求个数
    cJSON *child4 = cJSON_GetObjectItem(root, "count");
    if(NULL == child4)
    {
        LOG(MYFILES_LOG_MODULE, MYFILES_LOG_PROC, "cJSON_GetObjectItem err\n");
        ret = -1;
        if(root != NULL)
        {
            cJSON_Delete(root);//删除json对象
            root = NULL;
        }

        return ret;
    }

    *p_count = child4->valueint;


    if(root != NULL)
    {
        cJSON_Delete(root);//删除json对象
        root = NULL;
    }

    return ret;
}

int myfiles::get_user_filelist(char *cmd, char *user, int start, int count){
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
        LOG(MYFILES_LOG_MODULE, MYFILES_LOG_PROC, "msql_conn err\n");
        ret = -1;
        //善后
        aftermath(ret, out2, res_set, root, out);

        return ret;
    }

    //设置数据库编码，主要处理中文编码问题
    mysql_query(conn, "set names utf8");

    //多表指定行范围查询
    if(strcmp(cmd, "normal") == 0) //获取用户文件信息
    {
        //sql语句
        sprintf(sql_cmd, "select user_file_list.*, file_info.url, file_info.size, file_info.type from file_info, user_file_list where user = '%s' and file_info.md5 = user_file_list.md5 limit %d, %d", user, start, count);
    }
    else if(strcmp(cmd, "pvasc") == 0) //按下载量升序
    {
        //sql语句
        sprintf(sql_cmd, "select user_file_list.*, file_info.url, file_info.size, file_info.type from file_info, user_file_list where user = '%s' and file_info.md5 = user_file_list.md5  order by pv asc limit %d, %d", user, start, count);
    }
    else if(strcmp(cmd, "pvdesc") == 0) //按下载量降序
    {
        //sql语句
        sprintf(sql_cmd, "select user_file_list.*, file_info.url, file_info.size, file_info.type from file_info, user_file_list where user = '%s' and file_info.md5 = user_file_list.md5 order by pv desc limit %d, %d", user, start, count);
    }

    LOG(MYFILES_LOG_MODULE, MYFILES_LOG_PROC, "%s 在操作\n", sql_cmd);

    if (mysql_query(conn, sql_cmd) != 0)
    {
        LOG(MYFILES_LOG_MODULE, MYFILES_LOG_PROC, "%s 操作失败：%s\n", sql_cmd, mysql_error(conn));
        ret = -1;
        //善后
        aftermath(ret, out2, res_set, root, out);

        return ret;
    }

    res_set = mysql_store_result(conn);/*生成结果集*/
    if (res_set == NULL)
    {
        LOG(MYFILES_LOG_MODULE, MYFILES_LOG_PROC, "smysql_store_result error: %s!\n", mysql_error(conn));
        ret = -1;
        //善后
        aftermath(ret, out2, res_set, root, out);

        return ret;
    }

    long line = 0;
    //mysql_num_rows接受由mysql_store_result返回的结果结构集，并返回结构集中的行数
    line = mysql_num_rows(res_set);
    if (line == 0)//没有结果
    {
        LOG(MYFILES_LOG_MODULE, MYFILES_LOG_PROC, "mysql_num_rows(res_set) failed：%s\n", mysql_error(conn));
        ret = -1;
        //善后
        aftermath(ret, out2, res_set, root, out);

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

        //mysql_num_fields获取结果中列的个数
        /*for(i = 0; i < mysql_num_fields(res_set); i++)
        {
            if(row[i] != NULL)
            {

            }
        }*/

        /*
        {
        "user": "yoyo",
        "md5": "e8ea6031b779ac26c319ddf949ad9d8d",
        "time": "2017-02-26 21:35:25",
        "filename": "test.mp4",
        "share_status": 0,
        "pv": 0,
        "url": "http://192.168.31.109:80/group1/M00/00/00/wKgfbViy2Z2AJ-FTAaM3As-g3Z0782.mp4",
        "size": 27473666,
         "type": "mp4"
        }

        */
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
        if(row[4] != NULL)
        {
            cJSON_AddNumberToObject(item, "share_status", atoi( row[4] ));
        }

        //-- pv 文件下载量，默认值为0，下载一次加1
        if(row[5] != NULL)
        {
            cJSON_AddNumberToObject(item, "pv", atol( row[5] ));
        }

        //-- url 文件url
        if(row[6] != NULL)
        {
            cJSON_AddStringToObject(item, "url", row[6]);
        }

        //-- size 文件大小, 以字节为单位
        if(row[7] != NULL)
        {
            cJSON_AddNumberToObject(item, "size", atol( row[7] ));
        }

        //-- type 文件类型： png, zip, mp4……
        if(row[8] != NULL)
        {
            cJSON_AddStringToObject(item, "type", row[8]);
        }

        cJSON_AddItemToArray(array, item);
    }

    cJSON_AddItemToObject(root, "files", array);

    out = cJSON_Print(root);

    LOG(MYFILES_LOG_MODULE, MYFILES_LOG_PROC, "%s\n", out);

    aftermath(ret, out2, res_set, root, out);

    return ret;
}

void myfiles::aftermath(int ret, char* out2, MYSQL_RES* res_set, cJSON* root, char* out){
    if(ret == 0)
    {
        cout << out; //给前端反馈信息
    }
    else
    {   //失败
        /*
        获取用户文件列表：
            成功：文件列表json
            失败：{"code": "015"}
        */
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