#include <string>
#include <map>
#include <atomic>
#include <signal.h>
#include <boost/any.hpp>
#include "muduo/net/TcpServer.h"
#include "muduo/net/EventLoop.h"
#include "muduo/base/ThreadPool.h"
#include "http_conn.h"
#include "config_file_reader.h"
#include "cache_pool.h"
#include "db_pool.h"
#include "api_upload.h"
#include "api_common.h"
#include "api_dealfile.h" 
#include "log.h"
 
using CHttpConnPtr = std::shared_ptr<CHttpConn>;


std::map<uint32_t, CHttpConnPtr> s_http_map;

 
class HttpServer
{
public:
    HttpServer(EventLoop *loop, const InetAddress &addr, const std::string &name, 
                int num_event_loops,
                int num_threads,
                bool nodelay)
        : server_(loop, addr, name)
        , loop_(loop)
        ,num_threads_(num_threads)
        ,tcp_no_delay_(nodelay)
    {
        LOG_INFO("Use {} IO threads,  {} work Threds.", num_event_loops, num_threads);
        LOG_INFO("TCP no delay {}", nodelay);

        // 注册连接事件的回调函数，新连接或者断开都回调
        server_.setConnectionCallback(std::bind(&HttpServer::onConnection, this, std::placeholders::_1));
        //注册收到数据的回调函数
        server_.setMessageCallback(
            std::bind(&HttpServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
        //注册数据发送完毕的回调函数
        server_.setWriteCompleteCallback(std::bind(&HttpServer::onWriteComplete, this, std::placeholders::_1));
        // 设置合适的subloop线程数量
        server_.setThreadNum(num_event_loops);
    }
    void start()
    {
        thread_pool_.start(num_threads_);
        server_.start();
    }

private:
    // 连接建立或断开的回调函数
    void onConnection(const TcpConnectionPtr &conn)   
    {
        if (conn->connected())
        {
            if(tcp_no_delay_)      
                conn->setTcpNoDelay(true);
            LOG_INFO("Connection UP : {}", conn->peerAddress().toIpPort());
            
            uint32_t uuid = conn_uuid_generator_++;
            LOG_INFO("Connection UP: {}, uuid: {}", conn.get()->getTcpInfoString(), uuid);
            conn->setContext(uuid);
            // 新建一个CHttpConn，然后绑定 tcp TcpConnectionPtr
            CHttpConnPtr http_conn =  std::make_shared<CHttpConn>(conn);
            {
                std::lock_guard<std::mutex> ulock(mtx_); //自动释放
                s_http_map.insert({ uuid, http_conn});
            }
        }
        else
        {
            uint32_t uuid = boost::any_cast<uint32_t>( conn->getContext());
            LOG_INFO("Connection DOWN : {}, uuid: {}", conn.get()->getTcpInfoString(), uuid);
            std::lock_guard<std::mutex> ulock(mtx_); //自动释放
            CHttpConnPtr &http_conn = s_http_map[uuid];
            if(http_conn) {
                //处理 相关业务
                s_http_map.erase(uuid); //这里移除的时候会自动释放http_conn
            }    
        }
    }

    // 可读事件回调 conn 具体是哪个连接； buf是读buffer；time是当前时间
    void onMessage(const TcpConnectionPtr &conn, Buffer *buf, Timestamp time)
    {
        uint32_t uuid = boost::any_cast<uint32_t>(conn->getContext());
        LOG_INFO("TCP Conn: {}, uuid: {}, have msg", conn.get()->getTcpInfoString(), uuid);
        //然后管理tcp长连接？
        // std::string msg = buf->retrieveAllAsString();  //这里
   
        mtx_.lock();  
        CHttpConnPtr &http_conn = s_http_map[uuid];
        mtx_.unlock();
        if(http_conn) {
            //处理 相关业务
            // http_conn->OnRead(buf);  // 直接在io线程处理
            thread_pool_.run(std::bind(&CHttpConn::OnRead, http_conn, buf)); //给到业务线程处理
        } else {
            LOG_ERROR("TCP Conn: {}, uuid: {}, have broken", conn.get()->getTcpInfoString(), uuid);
            conn->shutdown();   // 
        }
    }

    void onWriteComplete(const TcpConnectionPtr& conn) //这个函数不是用来关闭的
    {
        uint32_t uuid =  boost::any_cast<uint32_t>(conn->getContext());
        LOG_INFO("TCP Conn: {}, uuid: {}, have WriteComplete", conn.get()->getTcpInfoString(), uuid);
    }

    EventLoop *loop_;
    TcpServer server_;
    ThreadPool thread_pool_;
    const int num_threads_ = 32;
    const bool tcp_no_delay_;    
    std::mutex mtx_;
    // conn_handle 从0开始递增，可以防止因socket handle重用引起的一些冲突
    std::atomic<uint32_t> conn_uuid_generator_ = 0;  //这里是用于http请求，不会一直保持链接
};

int main(int argc, char *argv[]) {
    // 默认情况下，往一个读端关闭的管道或socket连接中写数据将引发SIGPIPE信号。我们需要在代码中捕获并处理该信号，
    // 或者至少忽略它，因为程序接收到SIGPIPE信号的默认行为是结束进程，而我们绝对不希望因为错误的写操作而导致程序退出。
    // SIG_IGN 忽略信号的处理程序
    signal(SIGPIPE, SIG_IGN); //忽略SIGPIPE信号
    int ret = 0;
    char *str_tc_http_server_conf = NULL;
    if(argc > 1) {
        str_tc_http_server_conf = argv[1];  // 指向配置文件路径
    } else {
        str_tc_http_server_conf = (char *)"tc_http_server.conf";
    }
    
     // 读取配置文件
    CConfigFileReader config_file(str_tc_http_server_conf);     //读取配置文件

    //读取网络框架的配置  目前是epoll + io线程 + 业务线程
    char *http_bind_ip = config_file.GetConfigName("http_bind_ip");
    char *str_http_bind_port = config_file.GetConfigName("http_bind_port");        //8081 -- nginx.conf,当前服务的端口
    uint16_t http_bind_port = atoi(str_http_bind_port);
    char *str_num_event_loops = config_file.GetConfigName("num_event_loops");  
    int num_event_loops = atoi(str_num_event_loops);
    char *str_num_threads = config_file.GetConfigName("num_threads");  
    int num_threads = atoi(str_num_threads);
    char *str_nodelay =  config_file.GetConfigName("nodelay");  
    bool nodelay = atoi(str_nodelay);  

    //日志设置级别
    char *str_log_level =  config_file.GetConfigName("log_level");  
    //Logger::LogLevel log_level = static_cast<Logger::LogLevel>(atoi(str_log_level));
    //Logger::setLogLevel(log_level);



     // 短链主要是将图片链接转成短链 这个版本暂时先不用短链
    char *str_enable_shorturl = config_file.GetConfigName("enable_shorturl");
    uint16_t enable_shorturl = atoi(str_enable_shorturl);   //1开启短链，0不开启短链
    char *shorturl_server_address = config_file.GetConfigName("shorturl_server_address");// 短链服务地址
    char *shorturl_server_access_token = config_file.GetConfigName("shorturl_server_access_token");

    
    char *dfs_path_client = config_file.GetConfigName("dfs_path_client"); // /etc/fdfs/client.conf
    char *storage_web_server_ip = config_file.GetConfigName("storage_web_server_ip"); //后续可以配置域名
    char *storage_web_server_port = config_file.GetConfigName("storage_web_server_port");
    
     // 初始化mysql、redis连接池，内部也会读取读取配置文件tc_http_server.conf
    CacheManager::SetConfPath(str_tc_http_server_conf); //设置配置文件路径
    CacheManager *cache_manager = CacheManager::getInstance();
    if (!cache_manager) {
        LOG_ERROR("CacheManager init failed");
        return -1;
    }

    CDBManager::SetConfPath(str_tc_http_server_conf);   //设置配置文件路径
    CDBManager *db_manager = CDBManager::getInstance();
    if (!db_manager) {
        LOG_ERROR("DBManager init failed");
        return -1;
    }

    // 将配置文件的参数传递给对应模块
    if(enable_shorturl == 1) {
        ApiUploadInit(dfs_path_client, storage_web_server_ip, storage_web_server_port, shorturl_server_address,
        shorturl_server_access_token);
    } else {
        ApiUploadInit(dfs_path_client, storage_web_server_ip, storage_web_server_port, "", "");
    }
    
    ret = ApiDealfileInit(dfs_path_client);

    if (ApiInit() < 0) {
        LOG_ERROR("ApiInit failed");
        return -1;
    }

    EventLoop loop;     //主循环
    InetAddress addr(http_bind_port,  http_bind_ip);
    HttpServer server(&loop, addr, "HttpServer", num_event_loops, num_threads, nodelay);
    server.start();
    loop.loop();
    return 0;
}

 