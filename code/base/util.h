#ifndef __UTIL_H__
#define __UTIL_H__

#define _CRT_SECURE_NO_DEPRECATE // remove warning C4996,

// #include "lock.h"
// #include "ostype.h"
// #include "util_pdu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cstdint>
#ifndef _WIN32
#include <strings.h>
#endif

#include <assert.h>
#include <sys/stat.h>
#include <map>
#include <iostream>

using namespace std;
#ifdef _WIN32
#define snprintf sprintf_s
#else
#include <pthread.h>
#include <stdarg.h>
#include <sys/time.h>
#include <time.h>
#endif

#define NOTUSED_ARG(v)                                                         \
    ((void)v) // used this to remove warning C4100, unreferenced parameter

class CStrExplode {
  public:
    CStrExplode(char *str, char seperator);
    virtual ~CStrExplode();

    uint32_t GetItemCnt() { return item_cnt_; }
    char *GetItem(uint32_t idx) { return item_list_[idx]; }

  private:
    uint32_t item_cnt_;
    char **item_list_;
};
char *ReplaceStr(char *src, char old_char, char new_char);
string Int2String(uint32_t user_id);
uint32_t String2Int(const string &value);
void ReplaceMark(string &str, string &new_value, uint32_t &begin_pos);
void ReplaceMark(string &str, uint32_t new_value, uint32_t &begin_pos);

void WritePid();
inline unsigned char ToHex(const unsigned char &x);
inline unsigned char FromHex(const unsigned char &x);
string URLEncode(const string &sIn);
string URLDecode(const string &sIn);

int64_t GetFileSize(const char *path);
const char *MemFind(const char *src_str, size_t src_len, const char *sub_str,
                    size_t sub_len, bool flag = true);

#endif
