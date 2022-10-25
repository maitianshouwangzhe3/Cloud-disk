#include "dealfile_cgi.h"


int main(){
    dealfile df;
    df.run();
    return 0;
}

//g++ -o dealfile dealfile_cgi.cpp dealfile_cgi_main.cpp ../common/util_cgi.cpp ../common/des.cpp ../common/base64.cpp ../common/md5.cpp ../common/cJSON.cpp ../common/redis_op.cpp ../common/deal_mysql.cpp ../common/make_log.c ../common/cfg.cpp  -lmysqlclient -lm -lfcgi -lhiredis  -lfcgi -lstdc++ -lfcgi++