#include "cache_pool.h"

#include "util.h"
#include <stdlib.h>
#include <string.h>
#define log_error printf
#define log_info printf
#define log_warn printf
// #define log printf
#define MIN_CACHE_CONN_CNT 2
#define MAX_CACHE_CONN_FAIL_NUM 10

#include "muduo/base/Logging.h"

#include "config_file_reader.h"


CacheManager *CacheManager::s_cache_manager = NULL;
std::string CacheManager::conf_path_ = "tc_http_server.conf"; // 默认
CacheConn::CacheConn(const char *server_ip, int server_port, int db_index,
                     const char *password, const char *pool_name) {
    server_ip_ = server_ip;
    server_port_ = server_port;

    db_index_ = db_index;
    password_ = password;
    pool_name_ = pool_name;
    context_ = NULL;
    last_connect_time_ = 0;
}

CacheConn::CacheConn(CachePool *pCachePool) {
    cache_pool_ = pCachePool;
    if (pCachePool) {
        server_ip_ = pCachePool->GetServerIP();
        server_port_ = pCachePool->GetServerPort();
        db_index_ = pCachePool->GetDBIndex();
        password_ = pCachePool->GetPassword();
        pool_name_ = pCachePool->GetPoolName();
    } else {
        log_error("pCachePool is NULL\n");
    }

    context_ = NULL;
    last_connect_time_ = 0;
}

CacheConn::~CacheConn() {
    if (context_) {
        redisFree(context_);
        context_ = NULL;
    }
}

/*
 * redis初始化连接和重连操作，类似mysql_ping()
 */
int CacheConn::Init() {
    if (context_) // 非空，连接是正常的
    {
        return 0;
    }

    // 1s 尝试重连一次
    uint64_t cur_time = (uint64_t)time(NULL);
    if (cur_time < last_connect_time_ + 1) // 重连尝试 间隔1秒
    {
        printf("cur_time:%lu, m_last_connect_time:%lu\n", cur_time,
               last_connect_time_);
        return 1;
    }
    // printf("m_last_connect_time = cur_time\n");
    last_connect_time_ = cur_time;

    // 1000ms超时
    struct timeval timeout = {0, 1000000};
    // 建立连接后使用 redisContext 来保存连接状态。
    // redisContext 在每次操作后会修改其中的 err 和  errstr
    // 字段来表示发生的错误码（大于0）和对应的描述。
    context_ =
        redisConnectWithTimeout(server_ip_.c_str(), server_port_, timeout);

    if (!context_ || context_->err) {
        if (context_) {
            log_error("redisConnect failed: %s\n", context_->errstr);
            redisFree(context_);
            context_ = NULL;
        } else {
            log_error("redisConnect failed\n");
        }

        return 1;
    }

    redisReply *reply;
    // 验证
    if (!password_.empty()) {
        reply =
            (redisReply *)redisCommand(context_, "AUTH %s", password_.c_str());

        if (!reply || reply->type == REDIS_REPLY_ERROR) {
            log_error("Authentication failure:%p\n", reply);
            if (reply)
                freeReplyObject(reply);
            return -1;
        } else {
            // log_info("Authentication success\n");
        }

        freeReplyObject(reply);
    }

    reply = (redisReply *)redisCommand(context_, "SELECT %d", 0);

    if (reply && (reply->type == REDIS_REPLY_STATUS) &&
        (strncmp(reply->str, "OK", 2) == 0)) {
        freeReplyObject(reply);
        return 0;
    } else {
        if (reply)
            log_error("select cache db failed:%s\n", reply->str);
        return 2;
    }
}

void CacheConn::DeInit() {
    if (context_) {
        redisFree(context_);
        context_ = NULL;
    }
}

const char *CacheConn::GetPoolName() { return pool_name_.c_str(); }

string CacheConn::Get(string key) {
    string value;

    if (Init()) {
        return value;
    }

    redisReply *reply =
        (redisReply *)redisCommand(context_, "GET %s", key.c_str());
    if (!reply) {
        log_error("redisCommand failed:%s\n", context_->errstr);
        redisFree(context_);
        context_ = NULL;
        return value;
    }

    if (reply->type == REDIS_REPLY_STRING) {
        value.append(reply->str, reply->len);
    }

    freeReplyObject(reply);
    return value;
}

string CacheConn::Set(string key, string value) {
    string ret_value;

    if (Init()) {
        return ret_value;
    }
    // 返回的结果存放在redisReply
    redisReply *reply = (redisReply *)redisCommand(context_, "SET %s %s",
                                                   key.c_str(), value.c_str());
    if (!reply) {
        log_error("redisCommand failed:%s\n", context_->errstr);
        redisFree(context_);
        context_ = NULL;
        return ret_value;
    }

    ret_value.append(reply->str, reply->len);
    freeReplyObject(reply); // 释放资源
    return ret_value;
}

string CacheConn::SetEx(string key, int timeout, string value) {
    string ret_value;

    if (Init()) {
        return ret_value;
    }

    redisReply *reply = (redisReply *)redisCommand(
        context_, "SETEX %s %d %s", key.c_str(), timeout, value.c_str());
    if (!reply) {
        log_error("redisCommand failed:%s\n", context_->errstr);
        redisFree(context_);
        context_ = NULL;
        return ret_value;
    }

    ret_value.append(reply->str, reply->len);
    freeReplyObject(reply);
    return ret_value;
}

bool CacheConn::MGet(const vector<string> &keys,
                     map<string, string> &ret_value) {
    if (Init()) {
        return false;
    }
    if (keys.empty()) {
        return false;
    }

    string strKey;
    bool bFirst = true;
    for (vector<string>::const_iterator it = keys.begin(); it != keys.end();
         ++it) {
        if (bFirst) {
            bFirst = false;
            strKey = *it;
        } else {
            strKey += " " + *it;
        }
    }

    if (strKey.empty()) {
        return false;
    }
    strKey = "MGET " + strKey;
    redisReply *reply = (redisReply *)redisCommand(context_, strKey.c_str());
    if (!reply) {
        log_info("redisCommand failed:%s\n", context_->errstr);
        redisFree(context_);
        context_ = NULL;
        return false;
    }
    if (reply->type == REDIS_REPLY_ARRAY) {
        for (size_t i = 0; i < reply->elements; ++i) {
            redisReply *child_reply = reply->element[i];
            if (child_reply->type == REDIS_REPLY_STRING) {
                ret_value[keys[i]] = child_reply->str;
            }
        }
    }
    freeReplyObject(reply);
    return true;
}

bool CacheConn::IsExists(string &key) {
    if (Init()) {
        return false;
    }

    redisReply *reply =
        (redisReply *)redisCommand(context_, "EXISTS %s", key.c_str());
    if (!reply) {
        log_error("redisCommand failed:%s\n", context_->errstr);
        redisFree(context_);
        context_ = NULL;
        return false;
    }
    long ret_value = reply->integer;
    freeReplyObject(reply);
    if (0 == ret_value) {
        return false;
    } else {
        return true;
    }
}

long CacheConn::Del(string key) {
    if (Init()) {
        return 0;
    }

    redisReply *reply =
        (redisReply *)redisCommand(context_, "DEL %s", key.c_str());
    if (!reply) {
        log_error("redisCommand failed:%s\n", context_->errstr);
        redisFree(context_);
        context_ = NULL;
        return 0;
    }

    long ret_value = reply->integer;
    freeReplyObject(reply);
    return ret_value;
}

long CacheConn::Hdel(string key, string field) {
    if (Init()) {
        return -1;
    }
    redisReply *reply = (redisReply *)redisCommand(context_, "HDEL %s %s",
                                                   key.c_str(), field.c_str());
    if (!reply) {
        log_error("redisCommand failed:%s\n", context_->errstr);
        redisFree(context_);
        context_ = NULL;
        return -1;
    }

    long ret_value = reply->integer;
    freeReplyObject(reply);
    return ret_value;
}

string CacheConn::Hget(string key, string field) {
    string ret_value;
    if (Init()) {
        return ret_value;
    }

    redisReply *reply = (redisReply *)redisCommand(context_, "HGET %s %s",
                                                   key.c_str(), field.c_str());
    if (!reply) {
        log_error("redisCommand failed:%s\n", context_->errstr);
        redisFree(context_);
        context_ = NULL;
        return ret_value;
    }

    if (reply->type == REDIS_REPLY_STRING) {
        ret_value.append(reply->str, reply->len);
    }

    freeReplyObject(reply);
    return ret_value;
}
int CacheConn::Hget(string key, char *field, char *value) {
    int retn = 0;
    int len = 0;

    if (Init()) {
        return -1;
    }

    redisReply *reply =
        (redisReply *)redisCommand(context_, "hget %s %s", key.c_str(), field);
    if (reply == NULL || reply->type != REDIS_REPLY_STRING) {
        printf("hget %s %s  error %s\n", key.c_str(), field, context_->errstr);
        retn = -1;
        goto END;
    }

    len = reply->len > VALUES_ID_SIZE ? VALUES_ID_SIZE : reply->len;
    strncpy(value, reply->str, len);

    value[len] = '\0';

END:
    freeReplyObject(reply);

    return retn;
}
bool CacheConn::HgetAll(string key, map<string, string> &ret_value) {
    if (Init()) {
        return false;
    }

    redisReply *reply =
        (redisReply *)redisCommand(context_, "HGETALL %s", key.c_str());
    if (!reply) {
        log_error("redisCommand failed:%s\n", context_->errstr);
        redisFree(context_);
        context_ = NULL;
        return false;
    }

    if ((reply->type == REDIS_REPLY_ARRAY) && (reply->elements % 2 == 0)) {
        for (size_t i = 0; i < reply->elements; i += 2) {
            redisReply *field_reply = reply->element[i];
            redisReply *value_reply = reply->element[i + 1];

            string field(field_reply->str, field_reply->len);
            string value(value_reply->str, value_reply->len);
            ret_value.insert(make_pair(field, value));
        }
    }

    freeReplyObject(reply);
    return true;
}

long CacheConn::Hset(string key, string field, string value) {
    if (Init()) {
        return -1;
    }

    redisReply *reply = (redisReply *)redisCommand(
        context_, "HSET %s %s %s", key.c_str(), field.c_str(), value.c_str());
    if (!reply) {
        log_error("redisCommand failed:%s\n", context_->errstr);
        redisFree(context_);
        context_ = NULL;
        return -1;
    }

    long ret_value = reply->integer;
    freeReplyObject(reply);
    return ret_value;
}

long CacheConn::HincrBy(string key, string field, long value) {
    if (Init()) {
        return -1;
    }

    redisReply *reply = (redisReply *)redisCommand(
        context_, "HINCRBY %s %s %ld", key.c_str(), field.c_str(), value);
    if (!reply) {
        log_error("redisCommand failed:%s\n", context_->errstr);
        redisFree(context_);
        context_ = NULL;
        return -1;
    }

    long ret_value = reply->integer;
    freeReplyObject(reply);
    return ret_value;
}

long CacheConn::IncrBy(string key, long value) {
    if (Init()) {
        return -1;
    }

    redisReply *reply = (redisReply *)redisCommand(context_, "INCRBY %s %ld",
                                                   key.c_str(), value);
    if (!reply) {
        log_error("redis Command failed:%s\n", context_->errstr);
        redisFree(context_);
        context_ = NULL;
        return -1;
    }
    long ret_value = reply->integer;
    freeReplyObject(reply);
    return ret_value;
}

string CacheConn::Hmset(string key, map<string, string> &hash) {
    string ret_value;

    if (Init()) {
        return ret_value;
    }

    int argc = hash.size() * 2 + 2;
    const char **argv = new const char *[argc];
    if (!argv) {
        return ret_value;
    }

    argv[0] = "HMSET";
    argv[1] = key.c_str();
    int i = 2;
    for (map<string, string>::iterator it = hash.begin(); it != hash.end();
         it++) {
        argv[i++] = it->first.c_str();
        argv[i++] = it->second.c_str();
    }

    redisReply *reply =
        (redisReply *)redisCommandArgv(context_, argc, argv, NULL);
    if (!reply) {
        log_error("redisCommand failed:%s\n", context_->errstr);
        delete[] argv;

        redisFree(context_);
        context_ = NULL;
        return ret_value;
    }

    ret_value.append(reply->str, reply->len);

    delete[] argv;
    freeReplyObject(reply);
    return ret_value;
}

bool CacheConn::Hmget(string key, list<string> &fields,
                      list<string> &ret_value) {
    if (Init()) {
        return false;
    }

    int argc = fields.size() + 2;
    const char **argv = new const char *[argc];
    if (!argv) {
        return false;
    }

    argv[0] = "HMGET";
    argv[1] = key.c_str();
    int i = 2;
    for (list<string>::iterator it = fields.begin(); it != fields.end(); it++) {
        argv[i++] = it->c_str();
    }

    redisReply *reply = (redisReply *)redisCommandArgv(
        context_, argc, (const char **)argv, NULL);
    if (!reply) {
        log_error("redisCommand failed:%s\n", context_->errstr);
        delete[] argv;

        redisFree(context_);
        context_ = NULL;

        return false;
    }

    if (reply->type == REDIS_REPLY_ARRAY) {
        for (size_t i = 0; i < reply->elements; i++) {
            redisReply *value_reply = reply->element[i];
            string value(value_reply->str, value_reply->len);
            ret_value.push_back(value);
        }
    }

    delete[] argv;
    freeReplyObject(reply);
    return true;
}

int CacheConn::Incr(string key, int64_t &value) {
    value = 0;
    if (Init()) {
        return -1;
    }

    redisReply *reply =
        (redisReply *)redisCommand(context_, "INCR %s", key.c_str());
    if (!reply) {
        log_error("redis Command failed:%s\n", context_->errstr);
        redisFree(context_);
        context_ = NULL;
        return -1;
    }
    value = reply->integer;
    freeReplyObject(reply);
    return 0;
}

int CacheConn::Decr(string key, int64_t &value) {
    if (Init()) {
        return -1;
    }

    redisReply *reply =
        (redisReply *)redisCommand(context_, "DECR %s", key.c_str());
    if (!reply) {
        log_error("redis Command failed:%s\n", context_->errstr);
        redisFree(context_);
        context_ = NULL;
        return -1;
    }
    value = reply->integer;
    freeReplyObject(reply);
    return 0;
}

long CacheConn::Lpush(string key, string value) {
    if (Init()) {
        return -1;
    }

    redisReply *reply = (redisReply *)redisCommand(context_, "LPUSH %s %s",
                                                   key.c_str(), value.c_str());
    if (!reply) {
        log_error("redisCommand failed:%s\n", context_->errstr);
        redisFree(context_);
        context_ = NULL;
        return -1;
    }

    long ret_value = reply->integer;
    freeReplyObject(reply);
    return ret_value;
}

long CacheConn::Rpush(string key, string value) {
    if (Init()) {
        return -1;
    }

    redisReply *reply = (redisReply *)redisCommand(context_, "RPUSH %s %s",
                                                   key.c_str(), value.c_str());
    if (!reply) {
        log_error("redisCommand failed:%s\n", context_->errstr);
        redisFree(context_);
        context_ = NULL;
        return -1;
    }

    long ret_value = reply->integer;
    freeReplyObject(reply);
    return ret_value;
}

long CacheConn::Llen(string key) {
    if (Init()) {
        return -1;
    }

    redisReply *reply =
        (redisReply *)redisCommand(context_, "LLEN %s", key.c_str());
    if (!reply) {
        log_error("redisCommand failed:%s\n", context_->errstr);
        redisFree(context_);
        context_ = NULL;
        return -1;
    }

    long ret_value = reply->integer;
    freeReplyObject(reply);
    return ret_value;
}

bool CacheConn::Lrange(string key, long start, long end,
                       list<string> &ret_value) {
    if (Init()) {
        return false;
    }

    redisReply *reply = (redisReply *)redisCommand(context_, "LRANGE %s %d %d",
                                                   key.c_str(), start, end);
    if (!reply) {
        log_error("redisCommand failed:%s\n", context_->errstr);
        redisFree(context_);
        context_ = NULL;
        return false;
    }

    if (reply->type == REDIS_REPLY_ARRAY) {
        for (size_t i = 0; i < reply->elements; i++) {
            redisReply *value_reply = reply->element[i];
            string value(value_reply->str, value_reply->len);
            ret_value.push_back(value);
        }
    }

    freeReplyObject(reply);
    return true;
}

int CacheConn::ZsetExit(string key, string member) {
    int retn = 0;
    redisReply *reply = NULL;
    if (Init()) {
        return -1;
    }

    //执行命令
    reply =
        (redisReply *)redisCommand(context_, "zlexcount %s [%s [%s",
                                   key.c_str(), member.c_str(), member.c_str());

    if (reply->type != REDIS_REPLY_INTEGER) {
        log_error("zlexcount: %s,member: %s Error:%s,%s\n", key.c_str(),
                  member.c_str(), reply->str, context_->errstr);
        retn = -1;
        goto END;
    }

    retn = reply->integer;

END:

    freeReplyObject(reply);
    return retn;
}

int CacheConn::ZsetAdd(string key, long score, string member) {
    int retn = 0;
    redisReply *reply = NULL;
    if (Init()) {
        LOG_ERROR << "Init() -> failed";
        return -1;
    }

    //执行命令, reply->integer成功返回1，reply->integer失败返回0
    reply = (redisReply *)redisCommand(context_, "ZADD %s %ld %s", key.c_str(),
                                       score, member.c_str());
    // rop_test_reply_type(reply);

    if (reply->type != REDIS_REPLY_INTEGER) {
        printf("ZADD: %s,member: %s Error:%s,%s, reply->integer:%lld, %d\n",
               key.c_str(), member.c_str(), reply->str, context_->errstr,
               reply->integer, reply->type);
        retn = -1;
        goto END;
    }

END:

    freeReplyObject(reply);
    return retn;
}

int CacheConn::ZsetZrem(string key, string member) {
    int retn = 0;
    redisReply *reply = NULL;
    if (Init()) {
        LOG_ERROR << "Init() -> failed";
        return -1;
    }

    //执行命令, reply->integer成功返回1，reply->integer失败返回0
    reply = (redisReply *)redisCommand(context_, "ZREM %s %s", key.c_str(),
                                       member.c_str());
    if (reply->type != REDIS_REPLY_INTEGER) {
        printf("ZREM: %s,member: %s Error:%s,%s\n", key.c_str(), member.c_str(),
               reply->str, context_->errstr);
        retn = -1;
        goto END;
    }
END:

    freeReplyObject(reply);
    return retn;
}
int CacheConn::ZsetIncr(string key, string member) {
    int retn = 0;
    redisReply *reply = NULL;
    if (Init()) {
        return false;
    }

    reply = (redisReply *)redisCommand(context_, "ZINCRBY %s 1 %s", key.c_str(),
                                       member.c_str());
    // rop_test_reply_type(reply);
    if (strcmp(reply->str, "OK") != 0) {
        printf("Add or increment table: %s,member: %s Error:%s,%s\n",
               key.c_str(), member.c_str(), reply->str, context_->errstr);

        retn = -1;
        goto END;
    }

END:
    freeReplyObject(reply);
    return retn;
}

int CacheConn::ZsetZcard(string key) {
    redisReply *reply = NULL;
    if (Init()) {
        return -1;
    }

    int cnt = 0;

    reply = (redisReply *)redisCommand(context_, "ZCARD %s", key.c_str());
    if (reply->type != REDIS_REPLY_INTEGER) {
        printf("ZCARD %s error %s\n", key.c_str(), context_->errstr);
        cnt = -1;
        goto END;
    }

    cnt = reply->integer;

END:
    freeReplyObject(reply);
    return cnt;
}
int CacheConn::ZsetZrevrange(string key, int from_pos, int end_pos,
                             RVALUES values, int &get_num) {
    int retn = 0;
    redisReply *reply = NULL;
    if (Init()) {
        return -1;
    }
    int i = 0;
    int max_count = 0;

    int count = end_pos - from_pos + 1; //请求元素个数

    //降序获取有序集合的元素
    reply = (redisReply *)redisCommand(context_, "ZREVRANGE %s %d %d",
                                       key.c_str(), from_pos, end_pos);
    if (reply->type != REDIS_REPLY_ARRAY) //如果返回不是数组
    {
        printf("ZREVRANGE %s  error!%s\n", key.c_str(), context_->errstr);
        retn = -1;
        goto END;
    }

    //返回一个数组，查看elements的值(数组个数)
    //通过element[index] 的方式访问数组元素
    //每个数组元素是一个redisReply对象的指针

    max_count = (reply->elements > count) ? count : reply->elements;
    get_num = max_count; //得到结果value的个数

    for (i = 0; i < max_count; ++i) {
        strncpy(values[i], reply->element[i]->str, VALUES_ID_SIZE - 1);
        values[i][VALUES_ID_SIZE - 1] = 0; //结束符
    }

END:
    if (reply != NULL) {
        freeReplyObject(reply);
    }

    return retn;
}

int CacheConn::ZsetGetScore(string key, string member) {
    if (Init()) {
        return -1;
    }

    int score = 0;

    redisReply *reply = NULL;

    reply = (redisReply *)redisCommand(context_, "ZSCORE %s %s", key.c_str(),
                                       member.c_str());

    if (reply->type != REDIS_REPLY_STRING) {
        printf("[-][GMS_REDIS]ZSCORE %s %s error %s\n", key.c_str(),
               member.c_str(), context_->errstr);
        score = -1;
        goto END;
    }
    score = atoi(reply->str);

END:
    freeReplyObject(reply);

    return score;
}

bool CacheConn::FlushDb() {
    bool ret = false;
    if (Init()) {
        return false;
    }

    redisReply *reply = (redisReply *)redisCommand(context_, "FLUSHDB");
    if (!reply) {
        log_error("redisCommand failed:%s\n", context_->errstr);
        redisFree(context_);
        context_ = NULL;
        return false;
    }

    if (reply->type == REDIS_REPLY_STRING &&
        strncmp(reply->str, "OK", 2) == 0) {
        ret = true;
    }

    freeReplyObject(reply);

    return ret;
}
///////////////
CachePool::CachePool(const char *pool_name, const char *server_ip,
                     int server_port, int db_index, const char *password,
                     int max_conn_cnt) {
    pool_name_ = pool_name;
    server_ip_ = server_ip;
    m_server_port = server_port;
    db_index_ = db_index;
    password_ = password;
    max_conn_cnt_ = max_conn_cnt;
    cur_conn_cnt_ = MIN_CACHE_CONN_CNT;
}

CachePool::~CachePool() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        abort_request_ = true;
        cond_var_.notify_all(); // 通知所有在等待的
    }
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (list<CacheConn *>::iterator it = free_list_.begin();
             it != free_list_.end(); it++) {
            CacheConn *pConn = *it;
            delete pConn;
        }
    }

    free_list_.clear();
    cur_conn_cnt_ = 0;
}

int CachePool::Init() {
    for (int i = 0; i < cur_conn_cnt_; i++) {
        CacheConn *pConn =
            new CacheConn(server_ip_.c_str(), m_server_port, db_index_,
                          password_.c_str(), pool_name_.c_str());
        if (pConn->Init()) {
            delete pConn;
            return 1;
        }

        free_list_.push_back(pConn);
    }

    log_info("cache pool: %s, list size: %lu\n", pool_name_.c_str(),
             free_list_.size());
    return 0;
}

CacheConn *CachePool::GetCacheConn(const int timeout_ms) {
    std::unique_lock<std::mutex> lock(m_mutex);
    if (abort_request_) {
        log_info("have aboort\n");
        return NULL;
    }

    if (free_list_.empty()) // 2 当没有连接可以用时
    {
        // 第一步先检测 当前连接数量是否达到最大的连接数量
        if (cur_conn_cnt_ >= max_conn_cnt_) // 等待的逻辑
        {
            // 如果已经到达了，看看是否需要超时等待
            if (timeout_ms <= 0) // 死等，直到有连接可以用 或者 连接池要退出
            {
                log_info("wait ms:%d\n", timeout_ms);
                cond_var_.wait(lock, [this] {
                    // 当前连接数量小于最大连接数量 或者请求释放连接池时退出
                    return (!free_list_.empty()) | abort_request_;
                });
            } else {
                // return如果返回 false，继续wait(或者超时),
                // 如果返回true退出wait 1.m_free_list不为空 2.超时退出
                // 3. m_abort_request被置为true，要释放整个连接池
                cond_var_.wait_for(
                    lock, std::chrono::milliseconds(timeout_ms),
                    [this] { return (!free_list_.empty()) | abort_request_; });
                // 带超时功能时还要判断是否为空
                if (free_list_.empty()) // 如果连接池还是没有空闲则退出
                {
                    return NULL;
                }
            }

            if (abort_request_) {
                log_warn("have aboort\n");
                return NULL;
            }
        } else // 还没有到最大连接则创建连接
        {
            CacheConn *db_conn =
                new CacheConn(server_ip_.c_str(), m_server_port, db_index_,
                              password_.c_str(), pool_name_.c_str()); //新建连接
            int ret = db_conn->Init();
            if (ret) {
                log_error("Init DBConnecton failed\n\n");
                delete db_conn;
                return NULL;
            } else {
                free_list_.push_back(db_conn);
                cur_conn_cnt_++;
                // log_info("new db connection: %s, conn_cnt: %d\n",
                // m_pool_name.c_str(), m_cur_conn_cnt);
            }
        }
    }

    CacheConn *pConn = free_list_.front();
    free_list_.pop_front();

    return pConn;
}

void CachePool::RelCacheConn(CacheConn *p_cache_conn) {
    std::lock_guard<std::mutex> lock(m_mutex);

    list<CacheConn *>::iterator it = free_list_.begin();
    for (; it != free_list_.end(); it++) {
        if (*it == p_cache_conn) {
            break;
        }
    }

    if (it == free_list_.end()) {
        // m_used_list.remove(pConn);
        free_list_.push_back(p_cache_conn);
        cond_var_.notify_one(); // 通知取队列
    } else {
        log_error("RelDBConn failed\n"); // 不再次回收连接
    }
}

///////////
CacheManager::CacheManager() {}

CacheManager::~CacheManager() {}

void CacheManager::SetConfPath(const char *conf_path) {
    conf_path_ = conf_path;
}

CacheManager *CacheManager::getInstance() {
    if (!s_cache_manager) {
        s_cache_manager = new CacheManager();
        if (s_cache_manager->Init()) {
            delete s_cache_manager;
            s_cache_manager = NULL;
        }
    }

    return s_cache_manager;
}

int CacheManager::Init() {
    LOG_INFO << "Init";
    CConfigFileReader config_file(conf_path_.c_str());

    char *cache_instances = config_file.GetConfigName("CacheInstances");
    if (!cache_instances) {
        LOG_ERROR << "not configure CacheIntance";
        return 1;
    }

    char host[64];
    char port[64];
    char db[64];
    char maxconncnt[64];
    CStrExplode instances_name(cache_instances, ',');
    for (uint32_t i = 0; i < instances_name.GetItemCnt(); i++) {
        char *pool_name = instances_name.GetItem(i);
        // printf("%s", pool_name);
        snprintf(host, 64, "%s_host", pool_name);
        snprintf(port, 64, "%s_port", pool_name);
        snprintf(db, 64, "%s_db", pool_name);
        snprintf(maxconncnt, 64, "%s_maxconncnt", pool_name);

        char *cache_host = config_file.GetConfigName(host);
        char *str_cache_port = config_file.GetConfigName(port);
        char *str_cache_db = config_file.GetConfigName(db);
        char *str_max_conn_cnt = config_file.GetConfigName(maxconncnt);
        if (!cache_host || !str_cache_port || !str_cache_db ||
            !str_max_conn_cnt) {
            if(!cache_host)
                LOG_ERROR << "not configure cache instance: " <<  pool_name << ", cache_host is null";
            if(!str_cache_port)
                LOG_ERROR << "not configure cache instance: " << pool_name << ", str_cache_port is null";
            if(!str_cache_db)
                LOG_ERROR << "not configure cache instance: " << pool_name << ", str_cache_db is null";
            if(!str_max_conn_cnt)
                LOG_ERROR << "not configure cache instance: " << pool_name << ", str_max_conn_cnt is null";
            return 2;
        }

        CachePool *pCachePool =
            new CachePool(pool_name, cache_host, atoi(str_cache_port),
                          atoi(str_cache_db), "", atoi(str_max_conn_cnt));
        if (pCachePool->Init()) {
            LOG_ERROR << "Init cache pool failed";
            return 3;
        }

        m_cache_pool_map.insert(make_pair(pool_name, pCachePool));
    }

    return 0;
}

CacheConn *CacheManager::GetCacheConn(const char *pool_name) {
    map<string, CachePool *>::iterator it = m_cache_pool_map.find(pool_name);
    if (it != m_cache_pool_map.end()) {
        return it->second->GetCacheConn();
    } else {
        return NULL;
    }
}

void CacheManager::RelCacheConn(CacheConn *cache_conn) {
    if (!cache_conn) {
        return;
    }

    map<string, CachePool *>::iterator it =
        m_cache_pool_map.find(cache_conn->GetPoolName());
    if (it != m_cache_pool_map.end()) {
        return it->second->RelCacheConn(cache_conn);
    }
}
