#include "login_cgi.h"


login::login(){
    len = 0;
    memset(token, 0, sizeof(token));
    memset(buf, 0, sizeof(buf));
    memset(user, 0, sizeof(user));
    memset(pwd, 0, sizeof(pwd));

    cin_streambuf = cin.rdbuf();
    cout_streambuf = cout.rdbuf();
    cerr_streambuf = cerr.rdbuf();

    FCGX_Init();
    FCGX_InitRequest(&request, 0, 0);
}

login::~login(){
    cin.rdbuf(cin_streambuf);
    cout.rdbuf(cout_streambuf);
    cerr.rdbuf(cerr_streambuf);
}

void login::Init(){

}

void login::run(){
    while(FCGX_Accept_r(&request) == 0){
        int len = 0;
        char token[128] = {0};

        fcgi_streambuf cin_fcgi_streambuf(request.in);
        fcgi_streambuf cout_fcgi_streambuf(request.out);
        fcgi_streambuf cerr_fcgi_streambuf(request.err);

        cin.rdbuf(&cin_fcgi_streambuf);
        cout.rdbuf(&cout_fcgi_streambuf);
        cerr.rdbuf(&cerr_fcgi_streambuf);

        char * clenstr = FCGX_GetParam("CONTENT_LENGTH", request.envp);
        cout << "Content-type: text/html\r\n\r\n";

        if(clenstr == NULL){
            len = 0;
        }
        else{
            len = atoi(clenstr);        //字符串转整形
        }

        if(len <= 0){                   //没有用户信息
            cout << "No data from standard input.<p>\n";
            LOG(LOGIN_LOG_MODULE, LOGIN_LOG_PROC, "len = 0, No data from standard input\n");
        }
        else{                           //获取用户信息
            char buf[4 * 1024] = {0};
            cin.read(buf, len);         //从cin(web服务器)读取数据

            LOG(LOGIN_LOG_MODULE, LOGIN_LOG_PROC, "buf = %s\n", buf);

            char user[512] = {0};
            char pwd[512] = {0};
            get_login_info();

            LOG(LOGIN_LOG_MODULE, LOGIN_LOG_PROC, "user = %s, pwd = %s\n", user, pwd);

            //登录判断，成功返回0
            int ret = check_user_pwd();
            if (ret == 0) //登陆成功
            {
                //生成token字符串
                memset(token, 0, sizeof(token));
                ret = set_token();
                LOG(LOGIN_LOG_MODULE, LOGIN_LOG_PROC, "token = %s\n", token);

            }

            if(ret == 0)
            {
                //返回前端登陆情况， 000代表成功
                return_login_status("000");
            }
            else
            {
                //返回前端登陆情况， 001代表失败
                return_login_status("001");
            }
        }
    }
}

int login::get_login_info(){
    int ret = 0;

    //解析json包
    //解析一个json字符串为cJSON对象
    cJSON * root = cJSON_Parse(buf);
    if(NULL == root)
    {
        LOG(LOGIN_LOG_MODULE, LOGIN_LOG_PROC, "cJSON_Parse err\n");
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
        LOG(LOGIN_LOG_MODULE, LOGIN_LOG_PROC, "cJSON_GetObjectItem err\n");
        ret = -1;
        if(root != NULL)
        {
            cJSON_Delete(root);//删除json对象
            root = NULL;
        }
        return ret;
    }

    //LOG(LOGIN_LOG_MODULE, LOGIN_LOG_PROC, "child1->valuestring = %s\n", child1->valuestring);
    strcpy(user, child1->valuestring); //拷贝内容

    //密码
    cJSON *child2 = cJSON_GetObjectItem(root, "pwd");
    if(NULL == child2)
    {
        LOG(LOGIN_LOG_MODULE, LOGIN_LOG_PROC, "cJSON_GetObjectItem err\n");
        ret = -1;
        if(root != NULL)
        {
            cJSON_Delete(root);//删除json对象
            root = NULL;
        }
    }

    //LOG(LOGIN_LOG_MODULE, LOGIN_LOG_PROC, "child1->valuestring = %s\n", child1->valuestring);
    strcpy(pwd, child2->valuestring); //拷贝内容



    return ret;
}

int login::check_user_pwd(){
    char sql_cmd[SQL_MAX_LEN] = {0};
    int ret = 0;

    //获取数据库用户名、用户密码、数据库标示等信息
    char mysql_user[256] = {0};
    char mysql_pwd[256] = {0};
    char mysql_db[256] = {0};
    cfg::get_mysql_info(mysql_user, mysql_pwd,  mysql_db);
    LOG(LOGIN_LOG_MODULE, LOGIN_LOG_PROC, "mysql_user = %s, mysql_pwd = %s, mysql_db = %s\n", mysql_user, mysql_pwd, mysql_db);

    //connect the database
    MYSQL *conn = deal_mysql::msql_conn(mysql_user, mysql_pwd, mysql_db);
    if (conn == NULL)
    {
        LOG(LOGIN_LOG_MODULE, LOGIN_LOG_PROC, "msql_conn err\n");
        return -1;
    }

    //设置数据库编码，主要处理中文编码问题
    mysql_query(conn, "set names utf8");

    //sql语句，查找某个用户对应的密码
    sprintf(sql_cmd, "select password from user where name=\"%s\"", user);

    //deal result
    char tmp[PWD_LEN] = {0};

    //返回值： 0成功并保存记录集，1没有记录集，2有记录集但是没有保存，-1失败
    deal_mysql::process_result_one(conn, sql_cmd, tmp); //执行sql语句，结果集保存在tmp
    if(strcmp(tmp, pwd) == 0)
    {
        ret = 0;
    }
    else
    {
        ret = -1;
    }

    mysql_close(conn);


    return ret;
}

int login::set_token(){
    int ret = 0;
    redisContext * redis_conn = NULL;

    //redis 服务器ip、端口
    char redis_ip[30] = {0};
    char redis_port[10] = {0};

    //读取redis配置信息
    cfg::get_cfg_value(CFG_PATH, "redis", "ip", redis_ip);
    cfg::get_cfg_value(CFG_PATH, "redis", "port", redis_port);
    LOG(LOGIN_LOG_MODULE, LOGIN_LOG_PROC, "redis:[ip=%s,port=%s]\n", redis_ip, redis_port);

    //连接redis数据库
    redis_op rp;
    redis_conn = rp.rop_connectdb_nopwd(redis_ip, redis_port);
    if (redis_conn == NULL)
    {
        LOG(LOGIN_LOG_MODULE, LOGIN_LOG_PROC, "redis connected error\n");
        ret = -1;
        if(redis_conn != NULL)
        {
            rp.rop_disconnect(redis_conn);
        }

        return ret;
    }

    //产生4个1000以内的随机数
    int rand_num[4] = {0};
    int i = 0;

    //设置随机种子
    srand((unsigned int)time(NULL));
    for(i = 0; i < 4; ++i)
    {
        rand_num[i] = rand()%1000;//随机数
    }

    char tmp[1024] = {0};
    sprintf(tmp, "%s%d%d%d%d", user, rand_num[0], rand_num[1], rand_num[2], rand_num[3]);
    LOG(LOGIN_LOG_MODULE, LOGIN_LOG_PROC, "tmp = %s\n", tmp);

    //加密
    char enc_tmp[1024*2] = {0};
    int enc_len = 0;
    ret = DesEnc((unsigned char *)tmp, strlen(tmp), (unsigned char *)enc_tmp, &enc_len);
    if(ret != 0)
    {
        LOG(LOGIN_LOG_MODULE, LOGIN_LOG_PROC, "DesEnc error\n");
        ret = -1;
        if(redis_conn != NULL)
        {
            rp.rop_disconnect(redis_conn);
        }

        return ret;
    }

    //to base64
    char base64[1024*3] = {0};
    base64::base64_encode((const unsigned char *)enc_tmp, enc_len, base64); //base64编码
    LOG(LOGIN_LOG_MODULE, LOGIN_LOG_PROC, "base64 = %s\n", base64);

    //to md5
    MD5_CTX md5;
    md5::MD5Init(&md5);
    unsigned char decrypt[16];
    md5::MD5Update(&md5, (unsigned char *)base64, strlen(base64) );
    md5::MD5Final(&md5, decrypt);


    char str[100] = { 0 };
    for (i = 0; i < 16; i++)
    {
        sprintf(str, "%02x", decrypt[i]);
        strcat(token, str);
    }

    // redis保存此字符串，用户名：token, 有效时间为24小时
    ret = rp.rop_setex_string(redis_conn, user, 86400, token);
    //ret = rop_setex_string(redis_conn, user, 30, token); //30秒


END:
    if(redis_conn != NULL)
    {
        rp.rop_disconnect(redis_conn);
    }

    return ret;

}

void login::return_login_status(char* status_num){
    char *out = NULL;
    cJSON *root = cJSON_CreateObject();  //创建json项目
    cJSON_AddStringToObject(root, "code", status_num);// {"code":"000"}
    cJSON_AddStringToObject(root, "token", token);// {"token":"token"}
    out = cJSON_Print(root);//cJSON to string(char *)

    cJSON_Delete(root);

    if(out != NULL)
    {
        //printf(out); //给前端反馈信息
        cout << out << '\n';
        free(out); //记得释放
    }
}