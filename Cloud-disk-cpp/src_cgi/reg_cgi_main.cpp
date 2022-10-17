#include "reg_cgi.h"

// g++ -o reg reg_cgi_main.cpp reg_cgi.cpp ../common/redis_op.cpp ../common/make_log.cpp ../common/util_cgi.cpp ../common/deal_mysql.cpp ../common/cfg.cpp ../common/cJSON.cpp -lmysqlclient -lm -lfcgi -lhiredis -I /usr/local/include -L /usr/local/lib -lfcgi -lstdc++ -lfcgi++

int main(){
    reg r;
    r.run();
}