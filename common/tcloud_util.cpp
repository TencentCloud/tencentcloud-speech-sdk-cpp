
// Copyright 1998-2020 Tencent Copyright
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include "tcloud_util.h"

namespace TCloudUtil {

    unsigned char ToHex(unsigned char x) {
        static char table[16] = {
            '0', '1', '2', '3', '4', '5', '6', '7',
            '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
        return table[x];
    }

    std::string UrlEncode(const std::string &url) {
        std::string strTemp = "";
        size_t length = url.length();
        for (size_t i = 0; i < length; i++) {
            if (isalnum((unsigned char)url[i]) ||
                (url[i] == '-') ||
                (url[i] == '_') ||
                (url[i] == '.') ||
                (url[i] == '~')) {
                    strTemp += url[i];
                } else {
                strTemp += '%';
                strTemp += ToHex((unsigned char)url[i] >> 4);
                strTemp += ToHex((unsigned char)url[i] % 16);
            }
        }
        return strTemp;
    }

    static const std::string base64_chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";

    std::string Base64Encode(const unsigned char *pBuffer, int length) {
        std::string ret;
        int i = 0;
        int j = 0;
        unsigned char char_array_3[3];
        unsigned char char_array_4[4];

        while (length--) {
            char_array_3[i++] = *(pBuffer++);
            if (i == 3) {
                char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
                char_array_4[1] = ((char_array_3[0] & 0x03) << 4) +
                    ((char_array_3[1] & 0xf0) >> 4);
                char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) +
                    ((char_array_3[2] & 0xc0) >> 6);
                char_array_4[3] = char_array_3[2] & 0x3f;

                for (i = 0; (i < 4); i++)
                    ret += base64_chars[char_array_4[i]];
                i = 0;
            }
        }

        if (i) {
            for (j = i; j < 3; j++)
                char_array_3[j] = '\0';

            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) +
                ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) +
                ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for (j = 0; (j < i + 1); j++)
                ret += base64_chars[char_array_4[j]];

            while ((i++ < 3))
                ret += '=';
        }

        return ret;
    }

    uint64_t gettid() {
        pthread_t ptid = pthread_self();
        uint64_t threadId = 0;
        int size = sizeof(threadId) < sizeof(ptid) ?
            sizeof(threadId) : sizeof(ptid);
        memcpy(&threadId, &ptid, size);
        return threadId;
    }

};  // namespace TCloudUtil
