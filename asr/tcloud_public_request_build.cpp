
// Copyright 1998-2020 Tencent Copyright
#include <vector>
#include <string>
#include <algorithm>
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include "tcloud_util.h"
#include "tcloud_public_request_builder.h"

#define MAX_BUFFER_LEN 1024

RequestBuilder::RequestBuilder() {
}

RequestBuilder::~RequestBuilder() {
}

int RequestBuilder::SetKeyValue(std::string key, std::string value) {
    if (value.length() == 0) {
        return 1;
    }

    if (key == "appid") {
        m_appid = value;
        return 0;
    }

    if (key == "secret_key") {
        m_secret_key = value;
        return 0;
    }

    m_key_value_map[key] = value;

    return 0;
}

std::string RequestBuilder::GetURLParamString(bool bUrlEncode) {
    std::string strParam;

    std::vector<std::string> keyVector;
    std::map<std::string, std::string>::iterator it = m_key_value_map.begin();
    while (it != m_key_value_map.end()) {
        std::string key = it->first;
        keyVector.push_back(key);
        it++;
    }

    sort(keyVector.begin(), keyVector.end());

    std::vector<std::string>::iterator vit = keyVector.begin();
    while (vit != keyVector.end()) {
        std::string key = *vit;
        std::string param = key + "=";
        param += bUrlEncode ? TCloudUtil::UrlEncode(m_key_value_map[key]) :
            m_key_value_map[key];

        strParam += param;
        vit++;

        if (vit != keyVector.end()) {
            strParam += "&";
        }
    }

    return strParam;
}

std::map<std::string, std::string> RequestBuilder::GetHttpHeader() {
    std::map<std::string, std::string> retMap;

    retMap["Content-Type"] = "application/octet-stream";
    retMap["Authorization"] = GetAuthorization();
    return retMap;
}

std::string RequestBuilder::GetAuthorization() {
    std::string strRet;

    char *pBuffer = new char[MAX_BUFFER_LEN];
    memset(pBuffer, 0, MAX_BUFFER_LEN);
    snprintf(pBuffer, MAX_BUFFER_LEN - 1, "asr.cloud.tencent.com/asr/v2/%s?%s",
        m_appid.c_str(), GetURLParamString(true).c_str());

    unsigned char *pOutput = new unsigned char[128];
    memset(pOutput, 0, 128);
    unsigned int outputLength = 0;

    const EVP_MD *engine = EVP_sha1();
    HMAC_CTX *hm = HMAC_CTX_new();
    HMAC_CTX_reset(hm);
    HMAC_Init_ex(hm, m_secret_key.c_str(), m_secret_key.length(), engine, NULL);
    HMAC_Update(hm, (const unsigned char *)pBuffer, strlen(pBuffer));
    HMAC_Final(hm, pOutput, &outputLength);
    HMAC_CTX_free(hm);
    strRet = TCloudUtil::Base64Encode(pOutput, outputLength);
    strRet = TCloudUtil::UrlEncode(strRet);

    delete[] pBuffer;
    pBuffer = NULL;
    delete[] pOutput;
    pOutput = NULL;

    return strRet;
}
