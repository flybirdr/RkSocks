//
// Created by 张开海 on 2023/10/30.
//

#ifndef R_SOCKS5CONFIG_H
#define R_SOCKS5CONFIG_H

#include <string>

class Socks5Config {
public:
    std::string serverAddr;
    int serverPort;
    std::string userName;
    std::string password;
    int mtu;

    Socks5Config();
    Socks5Config(const std::string &addr,
                 int port,
                 const std::string &userName,
                 const std::string &password,
                 int mtu,
                 const std::string &udpAddr,
                 int udpPort);

};


#endif //R_SOCKS5CONFIG_H
