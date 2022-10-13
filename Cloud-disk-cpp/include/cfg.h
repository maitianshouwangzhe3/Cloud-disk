#ifndef _CFG_H_
#define _CFG_H_

//配置文件路径
#define CFG_PATH "/home/ubuntu/mycloud/cloud/conf/cfg.json" 
///home/ubuntu/mycloud/cloud/conf
#define CFG_LOG_MODULE "cgi"
#define CFG_LOG_PROC   "cfg"

//#define LOGIN_LOG_PROC   "login"

class cfg{
public:
    static int get_cfg_value(const char *profile, char *tile, char *key, char *value);

    //获取数据库用户名、用户密码、数据库标示等信息
    static int get_mysql_info(char *mysql_user, char *mysql_pwd, char *mysql_db);

    //static int get_login_info(char *login_buf, char *user, char *pwd);
};


#endif