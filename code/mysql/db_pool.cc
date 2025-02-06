#include "db_pool.h"
#include <string.h>
#include "muduo/base/Logging.h"
#include "config_file_reader.h"



#define MIN_DB_CONN_CNT 1
#define MAX_DB_CONN_FAIL_NUM 10

CDBManager *CDBManager::s_db_manager = NULL;
std::string CDBManager::conf_path_ = "tc_http_server.conf";
CResultSet::CResultSet(MYSQL_RES *res) {
    res_ = res;
    // map table field key to index in the result array
    int num_fields = mysql_num_fields(res_); // 返回结果集中的行数。
    MYSQL_FIELD *fields = mysql_fetch_fields(res_); // 关于结果集所有列的MYSQL_FIELD结构的数组
    for (int i = 0; i < num_fields; i++) {
        // 多行
        key_map_.insert(make_pair(fields[i].name, i)); // 每个结构提供了结果集中1列的字段定义
        LOG_DEBUG << " num_fields fields["<< i << "].name: " <<  fields[i].name;
    }
}

CResultSet::~CResultSet() {
    if (res_) {
        mysql_free_result(res_);
        res_ = NULL;
    }
}

bool CResultSet::Next() {
    row_ = mysql_fetch_row(res_); // 检索结果集的下一行,行内值的数目由mysql_num_fields(result)给出
    if (row_) {
        return true;
    } else {
        return false;
    }
}

int CResultSet::_GetIndex(const char *key) {
    map<string, int>::iterator it = key_map_.find(key);
    if (it == key_map_.end()) {
        return -1;
    } else {
        return it->second;
    }
}

int CResultSet::GetInt(const char *key) {
    int idx = _GetIndex(key); // 查找列的索引
    if (idx == -1) {
        return 0;
    } else {
        return atoi(row_[idx]); // 有索引
    }
}

char *CResultSet::GetString(const char *key) {
    int idx = _GetIndex(key);
    if (idx == -1) {
        return NULL;
    } else {
        return row_[idx]; // 列
    }
}

/////////////////////////////////////////
CPrepareStatement::CPrepareStatement() {
    stmt_ = NULL;
    param_bind_ = NULL;
    param_cnt_ = 0;
}

CPrepareStatement::~CPrepareStatement() {
    if (stmt_) {
        mysql_stmt_close(stmt_);
        stmt_ = NULL;
    }

    if (param_bind_) {
        delete[] param_bind_;
        param_bind_ = NULL;
    }
}

bool CPrepareStatement::Init(MYSQL *mysql, string &sql) {
    mysql_ping(mysql); // 当mysql连接丢失的时候，使用mysql_ping能够自动重连数据库

    // g_master_conn_fail_num ++;
    stmt_ = mysql_stmt_init(mysql);
    if (!stmt_) {
        LOG_ERROR << "mysql_stmt_init failed";
        return false;
    }

    if (mysql_stmt_prepare(stmt_, sql.c_str(), sql.size())) {
        LOG_ERROR << "mysql_stmt_prepare failed: " <<  mysql_stmt_error(stmt_);

        return false;
    }

    param_cnt_ = mysql_stmt_param_count(stmt_);
    if (param_cnt_ > 0) {
        param_bind_ = new MYSQL_BIND[param_cnt_];
        if (!param_bind_) {
            LOG_ERROR << "new failed";
            return false;
        }

        memset(param_bind_, 0, sizeof(MYSQL_BIND) * param_cnt_);
    }

    return true;
}

void CPrepareStatement::SetParam(uint32_t index, int &value) {
    if (index >= param_cnt_) {
        LOG_ERROR << "index too large: " <<  index;
        return;
    }

    param_bind_[index].buffer_type = MYSQL_TYPE_LONG;
    param_bind_[index].buffer = &value;
}

void CPrepareStatement::SetParam(uint32_t index, uint32_t &value) {
    if (index >= param_cnt_) {
        LOG_ERROR << "index too large: " <<  index;
        return;
    }

    param_bind_[index].buffer_type = MYSQL_TYPE_LONG;
    param_bind_[index].buffer = &value;
}

void CPrepareStatement::SetParam(uint32_t index, string &value) {
    if (index >= param_cnt_) {
        LOG_ERROR << "index too large: " <<  index;
        return;
    }

    param_bind_[index].buffer_type = MYSQL_TYPE_STRING;
    param_bind_[index].buffer = (char *)value.c_str();
    param_bind_[index].buffer_length = value.size();
}

void CPrepareStatement::SetParam(uint32_t index, const string &value) {
    if (index >= param_cnt_) {
        LOG_ERROR << "index too large: " <<  index;
        return;
    }

    param_bind_[index].buffer_type = MYSQL_TYPE_STRING;
    param_bind_[index].buffer = (char *)value.c_str();
    param_bind_[index].buffer_length = value.size();
}

bool CPrepareStatement::ExecuteUpdate() {
    if (!stmt_) {
        LOG_ERROR << "no m_stmt"; 
        return false;
    }

    if (mysql_stmt_bind_param(stmt_, param_bind_)) {
        LOG_ERROR << "mysql_stmt_bind_param failed: " <<  mysql_stmt_error(stmt_);
        return false;
    }

    if (mysql_stmt_execute(stmt_)) {
        LOG_ERROR << "mysql_stmt_execute failed: " <<  mysql_stmt_error(stmt_);
        return false;
    }

    if (mysql_stmt_affected_rows(stmt_) == 0) {
        LOG_ERROR << "ExecuteUpdate have no effect"; 
        return false;
    }

    return true;
}

uint32_t CPrepareStatement::GetInsertId() {
    return mysql_stmt_insert_id(stmt_);
}

/////////////////////
CDBConn::CDBConn(CDBPool *pPool) {
    db_pool_ = pPool;
    mysql_ = NULL;
}

CDBConn::~CDBConn() {
    if (mysql_) {
        mysql_close(mysql_);
    }
}

int CDBConn::Init() {
    mysql_ = mysql_init(NULL); // mysql_标准的mysql c client对应的api
    if (!mysql_) {
        LOG_ERROR << "mysql_init failed"; 

        return 1;
    }

    int reconnect = 1;
    // mysql_options(mysql_, MYSQL_OPT_RECONNECT,  &reconnect); // 配合mysql_ping实现自动重连
    mysql_options(mysql_, MYSQL_SET_CHARSET_NAME, "utf8mb4"); // utf8mb4和utf8区别

    // ip 端口 用户名 密码 数据库名
    if (!mysql_real_connect(mysql_, db_pool_->GetDBServerIP(),
                            db_pool_->GetUsername(), db_pool_->GetPasswrod(),
                            db_pool_->GetDBName(), db_pool_->GetDBServerPort(),
                            NULL, 0)) {
        LOG_ERROR << "mysql_real_connect failed: " <<  mysql_error(mysql_);
        return 2;
    }

    return 0;
}

const char *CDBConn::GetPoolName() { return db_pool_->GetPoolName(); }

bool CDBConn::ExecuteCreate(const char *sql_query) {
    mysql_ping(mysql_);
    // mysql_real_query 实际就是执行了SQL
    if (mysql_real_query(mysql_, sql_query, strlen(sql_query))) {
        LOG_ERROR << "mysql_real_query failed: " <<  mysql_error(mysql_); 
        return false;
    }

    return true;
}

bool CDBConn::ExecutePassQuery(const char *sql_query) {
    mysql_ping(mysql_);
    // mysql_real_query 实际就是执行了SQL
    if (mysql_real_query(mysql_, sql_query, strlen(sql_query))) {
        LOG_ERROR << "mysql_real_query failed: " <<  mysql_error(mysql_); 
        return false;
    }

    return true;
}

bool CDBConn::ExecuteDrop(const char *sql_query) {
    mysql_ping(mysql_); // 如果端开了，能够自动重连

    if (mysql_real_query(mysql_, sql_query, strlen(sql_query))) {
        LOG_ERROR << "mysql_real_query failed: " <<  mysql_error(mysql_); 
        return false;
    }

    return true;
}

CResultSet *CDBConn::ExecuteQuery(const char *sql_query) {
    mysql_ping(mysql_);
    row_num = 0;
    if (mysql_real_query(mysql_, sql_query, strlen(sql_query))) {
        LOG_ERROR << "mysql_real_query failed: " << mysql_error(mysql_) << ", sql:" << sql_query;
        return NULL;
    }
    // 返回结果
    MYSQL_RES *res = mysql_store_result(
        mysql_); // 返回结果 https://www.mysqlzh.com/api/66.html
    if (!res) // 如果查询未返回结果集和读取结果集失败都会返回NULL
    {
        LOG_ERROR << "mysql_store_result failed: " <<  mysql_error(mysql_);
        return NULL;
    }
    row_num = mysql_num_rows(res);
    // LOG_INFO << "row_num: " <<  row_num;
    CResultSet *result_set = new CResultSet(res); // 存储到CResultSet
    return result_set;
}

/*
1.执行成功，则返回受影响的行的数目，如果最近一次查询失败的话，函数返回 -1

2.对于delete,将返回实际删除的行数.

3.对于update,如果更新的列值原值和新值一样,如update tables set col1=10 where
id=1; id=1该条记录原值就是10的话,则返回0。

mysql_affected_rows返回的是实际更新的行数,而不是匹配到的行数。
*/
bool CDBConn::ExecuteUpdate(const char *sql_query, bool care_affected_rows) {
    mysql_ping(mysql_);

    if (mysql_real_query(mysql_, sql_query, strlen(sql_query))) {
        LOG_ERROR << "mysql_real_query failed: " << mysql_error(mysql_) << ", sql:" << sql_query;
        return false;
    }

    if (mysql_affected_rows(mysql_) > 0) {
        return true;
    } else {                      // 影响的行数为0时
        if (care_affected_rows) { // 如果在意影响的行数时, 返回false,否则返回true            
            LOG_ERROR << "mysql_real_query failed: " << mysql_error(mysql_) << ", sql:" << sql_query;
            return false;
        } else {
            LOG_WARN << "affected_rows=0, sql: " <<  sql_query;
            return true;
        }
    }
}

bool CDBConn::StartTransaction() {
    mysql_ping(mysql_);

    if (mysql_real_query(mysql_, "start transaction\n", 17)) {
        LOG_ERROR << "mysql_real_query failed: " << mysql_error(mysql_) << " start transaction failed";
        return false;
    }

    return true;
}

bool CDBConn::Rollback() {
    mysql_ping(mysql_);

    if (mysql_real_query(mysql_, "rollback\n", 8)) {
        LOG_ERROR << "mysql_real_query failed: " << mysql_error(mysql_) << ", sql: rollback";
        return false;
    }

    return true;
}

bool CDBConn::Commit() {
    mysql_ping(mysql_);

    if (mysql_real_query(mysql_, "commit\n", 6)) {
        LOG_ERROR << "mysql_real_query failed: " << mysql_error(mysql_) << ", sql: commit";
        return false;
    }

    return true;
}
uint32_t CDBConn::GetInsertId() { return (uint32_t)mysql_insert_id(mysql_); }

////////////////
CDBPool::CDBPool(const char *pool_name, const char *db_server_ip,
                 uint16_t db_server_port, const char *username,
                 const char *password, const char *db_name, int max_conn_cnt) {
    pool_name_ = pool_name;
    db_server_ip_ = db_server_ip;
    db_server_port_ = db_server_port;
    username_ = username;
    password_ = password;
    db_name_ = db_name;
    db_max_conn_cnt_ = max_conn_cnt;    //
    db_cur_conn_cnt_ = MIN_DB_CONN_CNT; // 最小连接数量
}

// 释放连接池
CDBPool::~CDBPool() {
    std::lock_guard<std::mutex> lock(mutex_);
    abort_request_ = true;
    cond_var_.notify_all(); // 通知所有在等待的

    for (list<CDBConn *>::iterator it = free_list_.begin();
         it != free_list_.end(); it++) {
        CDBConn *pConn = *it;
        delete pConn;
    }

    free_list_.clear();
}

int CDBPool::Init() {
    // 创建固定最小的连接数量
    for (int i = 0; i < db_cur_conn_cnt_; i++) {
        CDBConn *db_conn = new CDBConn(this);
        int ret = db_conn->Init();
        if (ret) {
            delete db_conn;
            return ret;
        }

        free_list_.push_back(db_conn);
    }

    // log_info("db pool: %s, size: %d\n", m_pool_name.c_str(),
    // (int)free_list_.size());
    return 0;
}

/*
 *TODO:
 *增加保护机制，把分配的连接加入另一个队列，这样获取连接时，如果没有空闲连接，
 *TODO:
 *检查已经分配的连接多久没有返回，如果超过一定时间，则自动收回连接，放在用户忘了调用释放连接的接口
 * timeout_ms默认为 0死等
 * timeout_ms >0 则为等待的时间
 */
CDBConn *CDBPool::GetDBConn(const int timeout_ms) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (abort_request_) {
        LOG_WARN << "have aboort"; 
        return NULL;
    }

    if (free_list_.empty()) // 2 当没有连接可以用时
    {
        // 第一步先检测 当前连接数量是否达到最大的连接数量
        if (db_cur_conn_cnt_ >= db_max_conn_cnt_) // 等待的逻辑
        {
            // 如果已经到达了，看看是否需要超时等待
            if (timeout_ms <= 0) // 死等，直到有连接可以用 或者 连接池要退出
            {
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
                LOG_WARN << "have abort"; 
                return NULL;
            }
        } else // 还没有到最大连接则创建连接
        {
            CDBConn *db_conn = new CDBConn(this); //新建连接
            int ret = db_conn->Init();
            if (ret) {
                LOG_ERROR << "Init DBConnecton failed"; 
                delete db_conn;
                return NULL;
            } else {
                free_list_.push_back(db_conn);
                db_cur_conn_cnt_++;
                // log_info("new db connection: %s, conn_cnt: %d\n",
                // m_pool_name.c_str(), m_db_cur_conn_cnt);
            }
        }
    }

    CDBConn *pConn = free_list_.front(); // 获取连接
    free_list_.pop_front(); // STL 吐出连接，从空闲队列删除

    return pConn;
}

void CDBPool::RelDBConn(CDBConn *pConn) {
    std::lock_guard<std::mutex> lock(mutex_);

    list<CDBConn *>::iterator it = free_list_.begin();
    for (; it != free_list_.end(); it++) // 避免重复归还
    {
        if (*it == pConn) {
            break;
        }
    }

    if (it == free_list_.end()) {
        // used_list_.remove(pConn);
        free_list_.push_back(pConn);
        cond_var_.notify_one(); // 通知取队列
    } else {
        LOG_WARN << "RelDBConn failed";  // 不再次回收连接
    }
}
// 遍历检测是否超时未归还
// pConn->isTimeout(); // 当前时间 - 被请求的时间
// 强制回收  从m_used_list 放回 free_list_

/////////////////
CDBManager::CDBManager() {}

CDBManager::~CDBManager() {}

CDBManager *CDBManager::getInstance() {
    if (!s_db_manager) {
        s_db_manager = new CDBManager();
        if (s_db_manager->Init()) {
            delete s_db_manager;
            s_db_manager = NULL;
        }
    }

    return s_db_manager;
}

void CDBManager::SetConfPath(const char *conf_path)
{
    conf_path_ = conf_path;
}

int CDBManager::Init() {
    LOG_INFO << "Init";
    CConfigFileReader config_file(conf_path_.c_str());

    char *db_instances = config_file.GetConfigName("DBInstances");

    if (!db_instances) {
        LOG_ERROR << "not configure DBInstances"; 
        return 1;
    }

    char host[64];
    char port[64];
    char dbname[64];
    char username[64];
    char password[64];
    char maxconncnt[64];
    CStrExplode instances_name(db_instances, ',');

    for (uint32_t i = 0; i < instances_name.GetItemCnt(); i++) {
        char *pool_name = instances_name.GetItem(i);
        snprintf(host, 64, "%s_host", pool_name);
        snprintf(port, 64, "%s_port", pool_name);
        snprintf(dbname, 64, "%s_dbname", pool_name);
        snprintf(username, 64, "%s_username", pool_name);
        snprintf(password, 64, "%s_password", pool_name);
        snprintf(maxconncnt, 64, "%s_maxconncnt", pool_name);

        char *db_host = config_file.GetConfigName(host);
        char *str_db_port = config_file.GetConfigName(port);
        char *db_dbname = config_file.GetConfigName(dbname);
        char *db_username = config_file.GetConfigName(username);
        char *db_password = config_file.GetConfigName(password);
        char *str_maxconncnt = config_file.GetConfigName(maxconncnt);

        LOG_INFO << "db_host: " << db_host << ", db_port:" << str_db_port << 
                ", db_dbname:" << db_dbname << ", db_username:" << db_username << 
                ", db_password: " << db_password;

        if (!db_host || !str_db_port || !db_dbname || !db_username ||
            !db_password || !str_maxconncnt) {
            LOG_ERROR << "not configure db instance: " << pool_name;
            return 2;
        }

        int db_port = atoi(str_db_port);
        int db_maxconncnt = atoi(str_maxconncnt);
        CDBPool *pDBPool = new CDBPool(pool_name, db_host, db_port, db_username,
                                       db_password, db_dbname, db_maxconncnt);
        if (pDBPool->Init()) {
            LOG_ERROR << "init db instance failed: " << pool_name;
            return 3;
        }
        dbpool_map_.insert(make_pair(pool_name, pDBPool));
    }

    return 0;
}
//1. 先找连接池  2.从连接池获取连接
CDBConn *CDBManager::GetDBConn(const char *dbpool_name) {
    map<string, CDBPool *>::iterator it = dbpool_map_.find(dbpool_name); // 主从
    if (it == dbpool_map_.end()) {
        return NULL;
    } else {
        return it->second->GetDBConn();
    }
}

void CDBManager::RelDBConn(CDBConn *pConn) {
    if (!pConn) {
        return;
    }

    map<string, CDBPool *>::iterator it = dbpool_map_.find(pConn->GetPoolName());
    if (it != dbpool_map_.end()) {
        it->second->RelDBConn(pConn);
    }
}
