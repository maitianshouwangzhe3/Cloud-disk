//
//  HttpPdu.cpp
//  http_msg_server
//
//  Created by jianqing.du on 13-9-29.
//  Copyright (c) 2013年 ziteng. All rights reserved.
//

#include "http_parser_wrapper.h"
#include "http_parser.h"

#define MAX_REFERER_LEN 32

CHttpParserWrapper::CHttpParserWrapper() {}

void CHttpParserWrapper::ParseHttpContent(const char *buf, uint32_t len) {
    http_parser_init(&http_parser_, HTTP_REQUEST);
    memset(&settings_, 0, sizeof(settings_));
    settings_.on_url = OnUrl;
    settings_.on_header_field = OnHeaderField;
    settings_.on_header_value = OnHeaderValue;
    settings_.on_headers_complete = OnHeadersComplete;
    settings_.on_body = OnBody;
    settings_.on_message_complete = OnMessageComplete;
    settings_.object = this;

    read_all_ = false;
    read_referer_ = false;
    read_forward_ip_ = false;
    read_user_agent_ = false;
    read_content_type_ = false;
    read_content_len_ = false;
    read_host_ = false;
    total_length_ = 0;
    url_.clear();
    body_content_.clear();
    referer_.clear();
    forward_ip_.clear();
    user_agent_.clear();
    content_type_.clear();
    content_len_ = 0;
    host_.clear();

    http_parser_execute(&http_parser_, &settings_, buf, len);
}

int CHttpParserWrapper::OnUrl(http_parser *parser, const char *at,
                              size_t length, void *obj) {
    ((CHttpParserWrapper *)obj)->SetUrl(at, length);
    return 0;
}

int CHttpParserWrapper::OnHeaderField(http_parser *parser, const char *at,
                                      size_t length, void *obj) {
    if (!((CHttpParserWrapper *)obj)->HasReadReferer()) {
        if (strncasecmp(at, "Referer", 7) == 0) {
            ((CHttpParserWrapper *)obj)->SetReadReferer(true);
        }
    }

    if (!((CHttpParserWrapper *)obj)->HasReadForwardIP()) {
        if (strncasecmp(at, "X-Forwarded-For", 15) == 0) {
            ((CHttpParserWrapper *)obj)->SetReadForwardIP(true);
        }
    }

    if (!((CHttpParserWrapper *)obj)->HasReadUserAgent()) {
        if (strncasecmp(at, "User-Agent", 10) == 0) {
            ((CHttpParserWrapper *)obj)->SetReadUserAgent(true);
        }
    }

    if (!((CHttpParserWrapper *)obj)->HasReadContentType()) {
        if (strncasecmp(at, "Content-Type", 12) == 0) {
            ((CHttpParserWrapper *)obj)->SetReadContentType(true);
        }
    }

    if (!((CHttpParserWrapper *)obj)->HasReadContentLen()) {
        if (strncasecmp(at, "Content-Length", 14) == 0) {
            ((CHttpParserWrapper *)obj)->SetReadContentLen(true);
        }
    }
    if (!((CHttpParserWrapper *)obj)->HasReadHost()) {
        if (strncasecmp(at, "Host", 4) == 0) {
            ((CHttpParserWrapper *)obj)->SetReadHost(true);
        }
    }
    return 0;
}

int CHttpParserWrapper::OnHeaderValue(http_parser *parser, const char *at,
                                      size_t length, void *obj) {
    if (((CHttpParserWrapper *)obj)->IsReadReferer()) {
        size_t referer_len =
            (length > MAX_REFERER_LEN) ? MAX_REFERER_LEN : length;
        ((CHttpParserWrapper *)obj)->SetReferer(at, referer_len);
        ((CHttpParserWrapper *)obj)->SetReadReferer(false);
    }

    if (((CHttpParserWrapper *)obj)->IsReadForwardIP()) {
        ((CHttpParserWrapper *)obj)->SetForwardIP(at, length);
        ((CHttpParserWrapper *)obj)->SetReadForwardIP(false);
    }

    if (((CHttpParserWrapper *)obj)->IsReadUserAgent()) {
        ((CHttpParserWrapper *)obj)->SetUserAgent(at, length);
        ((CHttpParserWrapper *)obj)->SetReadUserAgent(false);
    }

    if (((CHttpParserWrapper *)obj)->IsReadContentType()) {
        ((CHttpParserWrapper *)obj)->SetContentType(at, length);
        ((CHttpParserWrapper *)obj)->SetReadContentType(false);
    }

    if (((CHttpParserWrapper *)obj)->IsReadContentLen()) {
        string strContentLen(at, length);
        ((CHttpParserWrapper *)obj)->SetContentLen(atoi(strContentLen.c_str()));
        ((CHttpParserWrapper *)obj)->SetReadContentLen(false);
    }

    if (((CHttpParserWrapper *)obj)->IsReadHost()) {
        ((CHttpParserWrapper *)obj)->SetHost(at, length);
        ((CHttpParserWrapper *)obj)->SetReadHost(false);
    }
    return 0;
}

int CHttpParserWrapper::OnHeadersComplete(http_parser *parser, void *obj) {
    ((CHttpParserWrapper *)obj)
        ->SetTotalLength(parser->nread + (uint32_t)parser->content_length);
    return 0;
}

int CHttpParserWrapper::OnBody(http_parser *parser, const char *at,
                               size_t length, void *obj) {
    ((CHttpParserWrapper *)obj)->SetBodyContent(at, length);
    return 0;
}

int CHttpParserWrapper::OnMessageComplete(http_parser *parser, void *obj) {
    ((CHttpParserWrapper *)obj)->SetReadAll();
    return 0;
}
