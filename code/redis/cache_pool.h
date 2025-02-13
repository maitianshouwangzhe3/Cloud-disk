/*
 * @Author: your name
 * @Date: 2019-12-07 10:54:57
 * @LastEditTime : 2020-01-10 16:35:13
 * @LastEditors  : Please set LastEditors
 * @Description: In User Settings Edit
 * @FilePath: \src\cache_pool\cache_pool.h
 */
#ifndef CACHEPOOL_H_
#define CACHEPOOL_H_

#include <condition_variable>
#include <iostream>
#include <list>
#include <unordered_map>
#include <mutex>
#include <vector>
#include <hiredis/hiredis.h>

using std::list;
using std::unordered_map;
using std::string;
using std::vector;

#define REDIS_COMMAND_SIZE 300 /* redis Command 指令最大长度 */
#define FIELD_ID_SIZE 100      /* redis hash表field域字段长度 */
#define VALUES_ID_SIZE 1024    /* redis        value域字段长度 */
typedef char (
    *RFIELDS)[FIELD_ID_SIZE]; /* redis hash表存放批量field字符串数组类型 */

//数组指针类型，其变量指向 char[1024]
typedef char (
    *RVALUES)[VALUES_ID_SIZE]; /* redis 表存放批量value字符串数组类型 */

class CachePool;

class CacheConn {
  public:
    CacheConn(const char *server_ip, int server_port, int db_index,
              const char *password, const char *pool_name = "");
    CacheConn(CachePool *pCachePool);
    virtual ~CacheConn();

    int Init();
    void DeInit();
    const char *GetPoolName();
    // 通用操作
    // 判断一个key是否存在
    bool IsExists(string &key);
    // 删除某个key
    long Del(string key);

    // ------------------- 字符串相关 -------------------
    string Get(string key);
    string Set(string key, string value);
    string SetEx(string key, int timeout, string value);

    // string mset(string key, map);
    //批量获取
    bool MGet(const vector<string> &keys, unordered_map<string, string> &ret_value);
    //原子加减1
    int Incr(string key, int64_t &value);
    int Decr(string key, int64_t &value);

    // ---------------- 哈希相关 ------------------------
    long Hdel(string key, string field);
    string Hget(string key, string field);
    int Hget(string key, char *field, char *value);
    bool HgetAll(string key, unordered_map<string, string> &ret_value);
    long Hset(string key, string field, string value);

    long HincrBy(string key, string field, long value);
    long IncrBy(string key, long value);
    string Hmset(string key, unordered_map<string, string> &hash);
    bool Hmget(string key, list<string> &fields, list<string> &ret_value);

    // ------------ 链表相关 ------------
    long Lpush(string key, string value);
    long Rpush(string key, string value);
    long Llen(string key);
    bool Lrange(string key, long start, long end, list<string> &ret_value);

    // zset 相关
    int ZsetExit(string key, string member);
    int ZsetAdd(string key, long score, string member);
    int ZsetZrem(string key, string member);
    int ZsetIncr(string key, string member);
    int ZsetZcard(string key);
    int ZsetZrevrange(string key, int from_pos, int end_pos, RVALUES values,
                      int &get_num);
    int ZsetGetScore(string key, string member);

    bool FlushDb();

  private:
    CachePool *cache_pool_;
    redisContext *context_; // 每个redis连接 redisContext redis客户端编程的对象
    uint64_t last_connect_time_;
    uint16_t server_port_;
    string server_ip_;
    string password_;
    uint16_t db_index_;
    string pool_name_;
};

class CachePool {
  public:
    // db_index和mysql不同的地方
    CachePool(const char *pool_name, const char *server_ip, int server_port,
              int db_index, const char *password, int max_conn_cnt);
    virtual ~CachePool();

    int Init();
    // 获取空闲的连接资源
    CacheConn *GetCacheConn(const int timeout_ms = 0);
    // Pool回收连接资源
    void RelCacheConn(CacheConn *cache_conn);

    const char *GetPoolName() { return pool_name_.c_str(); }
    const char *GetServerIP() { return server_ip_.c_str(); }
    const char *GetPassword() { return password_.c_str(); }
    int GetServerPort() { return m_server_port; }
    int GetDBIndex() { return db_index_; }

  private:
    string pool_name_;
    string server_ip_;
    string password_;
    int m_server_port;
    int db_index_; // mysql 数据库名字， redis db index

    int cur_conn_cnt_;
    int max_conn_cnt_;
    list<CacheConn *> free_list_;

    std::mutex m_mutex;
    std::condition_variable cond_var_;
    bool abort_request_ = false;
};

class CacheManager {
  public:
    virtual ~CacheManager();
    /// @brief 
    /// @param conf_path 
    static void SetConfPath(const char *conf_path);
    static CacheManager *getInstance();

    int Init();
    CacheConn *GetCacheConn(const char *pool_name);
    void RelCacheConn(CacheConn *cache_conn);

  private:
    CacheManager();

  private:
    static CacheManager *s_cache_manager;
    unordered_map<string, CachePool *> m_cache_pool_map;
    static std::string conf_path_;
};

class AutoRelCacheCon {
  public:
    AutoRelCacheCon(CacheManager *manger, CacheConn *conn)
        : manger_(manger), conn_(conn) {}
    ~AutoRelCacheCon() {
        if (manger_) {
            manger_->RelCacheConn(conn_);
        }
    } //在析构函数规划
  private:
    CacheManager *manger_ = NULL;
    CacheConn *conn_ = NULL;
};

#define AUTO_REL_CACHECONN(m, c) AutoRelCacheCon autorelcacheconn(m, c)

#endif /* CACHEPOOL_H_ */
