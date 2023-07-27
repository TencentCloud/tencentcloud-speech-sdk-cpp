
// Copyright 1998-2020 Tencent Copyright
#ifndef _TCLOUD_PUBLIC_REQUEST_BUILDER_H_
#define _TCLOUD_PUBLIC_REQUEST_BUILDER_H_

#include <string>
#include <map>

class RequestBuilder {
 public:
    RequestBuilder();
    ~RequestBuilder();

    int SetKeyValue(std::string key, std::string value);
    std::string GetURLParamString(bool bUrlEncode);
    std::map<std::string, std::string> GetHttpHeader();
    std::string GetAuthorization();

 protected:
    std::map<std::string, std::string> m_key_value_map;
    std::string m_appid;
    std::string m_secret_key;
};

#endif
