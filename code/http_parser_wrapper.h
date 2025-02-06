/**
 * 本文件对http解析器的封装
 */
#ifndef _HTTP_PARSER_WRAPPER_H_
#define _HTTP_PARSER_WRAPPER_H_
#include "http_parser.h"
#include "util.h"

// extract url and content body from an ajax request
class CHttpParserWrapper {
  public:
    CHttpParserWrapper();

    virtual ~CHttpParserWrapper() {}

    void ParseHttpContent(const char *buf, uint32_t len);

    bool IsReadAll() { return read_all_; }
    bool IsReadReferer() { return read_referer_; }
    bool HasReadReferer() { return referer_.size() > 0; }
    bool IsReadForwardIP() { return read_forward_ip_; }
    bool HasReadForwardIP() { return forward_ip_.size() > 0; }
    bool IsReadUserAgent() { return read_user_agent_; }
    bool HasReadUserAgent() { return user_agent_.size() > 0; }
    bool IsReadContentType() { return read_content_type_; }
    bool HasReadContentType() { return content_type_.size() > 0; }
    bool IsReadContentLen() { return read_content_len_; }
    bool HasReadContentLen() { return content_len_ != 0; }
    bool IsReadHost() { return read_host_; }
    bool HasReadHost() { return host_.size() > 0; }

    uint32_t GetTotalLength() { return total_length_; }
    char *GetUrl() { return (char *)url_.c_str(); }
    string &GetUrlString() {return url_;}
    char *GetBodyContent() { return (char *)body_content_.c_str(); }
    string &GetBodyContentString() { return body_content_; }
    uint32_t GetBodyContentLen() { return (uint32_t)body_content_.length(); }
    char *GetReferer() { return (char *)referer_.c_str(); }
    char *GetForwardIP() { return (char *)forward_ip_.c_str(); }
    char *GetUserAgent() { return (char *)user_agent_.c_str(); }
    char GetMethod() { return (char)http_parser_.method; }
    char *GetContentType() { return (char *)content_type_.c_str(); }
    uint32_t GetContentLen() { return content_len_; }
    char *GetHost() { return (char *)host_.c_str(); }

    void SetUrl(const char *url, size_t length) { url_.append(url, length); }
    void SetReferer(const char *referer, size_t length) {
        referer_.append(referer, length);
    }
    void SetForwardIP(const char *forward_ip, size_t length) {
        forward_ip_.append(forward_ip, length);
    }
    void SetUserAgent(const char *user_agent, size_t length) {
        user_agent_.append(user_agent, length);
    }
    void SetBodyContent(const char *content, size_t length) {
        body_content_.append(content, length);
    }
    void SetContentType(const char *content_type, size_t length) {
        content_type_.append(content_type, length);
    }
    void SetContentLen(uint32_t content_len) { content_len_ = content_len; }
    void SetTotalLength(uint32_t total_len) { total_length_ = total_len; }
    void SetHost(const char *host, size_t length) {
        host_.append(host, length);
    }
    void SetReadAll() { read_all_ = true; }
    void SetReadReferer(bool read_referer) { read_referer_ = read_referer; }
    void SetReadForwardIP(bool read_forward_ip) {
        read_forward_ip_ = read_forward_ip;
    }
    void SetReadUserAgent(bool read_user_agent) {
        read_user_agent_ = read_user_agent;
    }
    void SetReadContentType(bool read_content_type) {
        read_content_type_ = read_content_type;
    }
    void SetReadContentLen(bool read_content_len) {
        read_content_len_ = read_content_len;
    }
    void SetReadHost(bool read_host) { read_host_ = read_host; }

    static int OnUrl(http_parser *parser, const char *at, size_t length,
                     void *obj);
    static int OnHeaderField(http_parser *parser, const char *at, size_t length,
                             void *obj);
    static int OnHeaderValue(http_parser *parser, const char *at, size_t length,
                             void *obj);
    static int OnHeadersComplete(http_parser *parser, void *obj);
    static int OnBody(http_parser *parser, const char *at, size_t length,
                      void *obj);
    static int OnMessageComplete(http_parser *parser, void *obj);

  private:
    http_parser http_parser_;
    http_parser_settings settings_;

    bool read_all_;
    bool read_referer_;
    bool read_forward_ip_;
    bool read_user_agent_;
    bool read_content_type_;
    bool read_content_len_;
    bool read_host_;
    uint32_t total_length_;
    string url_;
    string body_content_;
    string referer_;
    string forward_ip_;
    string user_agent_;
    string content_type_;
    uint32_t content_len_;
    string host_;
};

#endif
