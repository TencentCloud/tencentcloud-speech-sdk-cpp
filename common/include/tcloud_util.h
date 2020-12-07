
// Copyright 1998-2020 Tencent Copyright
#ifndef _TCLOUD_UTIL_H_
#define _TCLOUD_UTIL_H_

#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include <string>

namespace TCloudUtil {
    std::string UrlEncode(const std::string &url);
    std::string Base64Encode(const unsigned char *pBuffer, int length);
    uint64_t gettid();
}  // namespace TCloudUtil

#endif  // _TCLOUD_UTIL_H_
