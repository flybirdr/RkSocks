//
// Created by 张开海 on 2023/11/8.
//

#ifndef R_PROXYCONFIG_H
#define R_PROXYCONFIG_H

#include <string>

namespace R {

    class ProxyConfig {
    public:
        enum PROXY_TYPE {
            SS,
            VMESS,
            VLESS,
            TORJAN
        };
        std::string name;
        PROXY_TYPE type;
        std::string server;
        uint16_t port;
        std::string cipher;
        std::string password;
        bool udp;

        //VMESS
        std::string uuid;
        int alterId;
        std::string network;
    };

} // R

#endif //R_PROXYCONFIG_H
