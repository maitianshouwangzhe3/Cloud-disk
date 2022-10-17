#include "upload_cgi.h"

int main(){
    upload up;
    up.run();
}


// g++ -o upload upload_cgi.cpp upload_cgi_main.cpp ../common/cJSON.cpp ../common/redis_op.cpp ../common/deal_mysql.cpp ../common/make_log.cpp ../common/cfg.cpp ../common/util_cgi.cpp -lmysqlclient -lm -lfcgi -lhiredis -I /usr/local/include -L /usr/local/lib -lfcgi -lstdc++ -lfcgi++