#include "dealsharefile_cgi.h"


int main(){
    dealsharefile dsf;
    dsf.run();
    return 0;
}

// g++ -o dealsharefile dealsharefile_cgi.cpp dealsharefile_cgi_main.cpp ../common/util_cgi.cpp ../common/des.cpp ../common/base64.cpp ../common/md5.cpp ../common/cJSON.cpp ../common/redis_op.cpp ../common/deal_mysql.cpp ../common/make_log.c ../common/cfg.cpp  -lmysqlclient -lm -lfcgi -lhiredis  -lfcgi -lstdc++ -lfcgi++