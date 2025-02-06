#ifndef __HTTP_CONN_H__
#define __HTTP_CONN_H__

#include <iostream>
#include <memory>
#include "muduo/base/noncopyable.h"
#include "http_parser_wrapper.h"
#include "muduo/net/TcpServer.h"
#include "muduo/net/TcpConnection.h"
#include "muduo/net/Buffer.h"

using namespace muduo;
using namespace muduo::net;
#define READ_BUF_SIZE 2048

#define HTTP_RESPONSE_JSON                                                     \
    "HTTP/1.1 200 OK\r\n"                                                      \
    "Connection:close\r\n"                                                     \
    "Content-Length:%d\r\n"                                                    \
    "Content-Type:application/json;charset=utf-8\r\n\r\n%s"

#define HTTP_RESPONSE_JSON_MAX 4096


enum HttpConnState
{
    kHttpDisconnected, // 已经断开连接
    kHttpConnecting,   // 正在连接
    kHttpConnected,    // 已连接
    kHttpDisconnecting, // 正在断开连接
    kHttpResponseFinish  //已经应答完毕了
};

class CHttpConn :  public std::enable_shared_from_this<CHttpConn>
{
public:
    CHttpConn(TcpConnectionPtr tcp_conn);
    virtual ~CHttpConn();

    uint32_t GetConnHandle() { return conn_handle_; }
    char *GetPeerIP() { return (char *)peer_ip_.c_str(); }

 
    void Close();
    void OnRead(Buffer *buf);
    void OnWrite();
    void OnClose();
    void OnTimer(uint64_t curr_tick);
    void OnWriteComlete();
 

  private:
    // 文件上传处理
    int _HandleUploadRequest(string &url, string &post_data);
    // 账号注册处理
    int _HandleRegisterRequest(string &url, string &post_data);
    // 账号登陆处理
    int _HandleLoginRequest(string &url, string &post_data);
    //
    int _HandleDealfileRequest(string &url, string &post_data);
    //
    int _HandleDealsharefileRequest(string &url, string &post_data);
    //
    int _HandleMd5Request(string &url, string &post_data);
    //
    int _HandleMyfilesRequest(string &url, string &post_data);
    //
    int _HandleSharefilesRequest(string &url, string &post_data);
    //
    int _HandleSharepictureRequest(string &url, string &post_data);
    int  _HandleHtml(string &url, string &post_data);

  protected:
     uint32_t conn_handle_;
    bool busy_;

    uint32_t state_;
    std::string peer_ip_;
    uint16_t peer_port_;
 

    uint64_t last_send_tick_;
    uint64_t last_recv_tick_;

    CHttpParserWrapper http_parser_;

    static uint32_t s_uuid_alloctor; // uuid分配
    uint32_t uuid_ = 0;                  // 自己的uuid

    TcpConnectionPtr tcp_conn_;

    enum HttpConnState http_state_ = kHttpDisconnected;
    uint64_t init_time_ = 0;
};

#endif