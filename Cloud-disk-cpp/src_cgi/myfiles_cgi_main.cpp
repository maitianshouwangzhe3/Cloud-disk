#include "myfiles_cgi.h"

int main(){
    myfiles mf;
    mf.run();
}


// g++ -o myfiles myfiles_cgi.cpp myfiles_cgi_main.cpp ../common/cJSON.cpp ../common/redis_op.cpp ../common/deal_mysql.cpp ../common/make_log.c ../common/cfg.cpp ../common/util_cgi.cpp -lmysqlclient -lm -lfcgi -lhiredis -I /usr/local/include -L /usr/local/lib -lfcgi -lstdc++ -lfcgi++