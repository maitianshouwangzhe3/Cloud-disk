/**
 * @file login_cgi.c
 * @brief   登陆后台CGI程序
 * @author Mike
 * @version 2.0
 * @date
 */

#include "login_cgi.h"
//  g++ -o login login_cgi.cpp login_cgi_main.cpp ../common/redis_op.cpp ../common/des.cpp ../common/base64.cpp ../common/md5.cpp ../common/make_log.cpp ../common/util_cgi.cpp ../common/deal_mysql.cpp ../common/cfg.cpp ../common/cJSON.cpp -lmysqlclient -lm -lfcgi -lhiredis  -I /usr/local/include -L /usr/local/lib -lfcgi -lstdc++ -lfcgi++

int main(){
    login ln;
    ln.run();
}