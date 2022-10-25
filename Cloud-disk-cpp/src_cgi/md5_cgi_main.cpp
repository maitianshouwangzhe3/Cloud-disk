#include "md5_cgi.h"


int main(){
    md5 md;
    md.run();
    return 0;
}
//../common/util_cgi.cpp -I /usr/local/include -L /usr/local/lib
// g++ -o md5 md5_cgi.cpp md5_cgi_main.cpp ../common/util_cgi.cpp ../common/des.cpp ../common/base64.cpp ../common/md5.cpp ../common/cJSON.cpp ../common/redis_op.cpp ../common/deal_mysql.cpp ../common/make_log.c ../common/cfg.cpp  -lmysqlclient -lm -lfcgi -lhiredis  -lfcgi -lstdc++ -lfcgi++