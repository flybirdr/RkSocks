//
// Created by 张开海 on 2023/10/30.
//

#include "Socks5Config.h"

Socks5Config::Socks5Config(const std::string &addr,
                           int port,
                           const std::string &userName,
                           const std::string &password,
                           int mtu,
                           const std::string &udpAddr,
                           int udpPort) :
        serverAddr(addr),
        serverPort(port),
        userName(userName),
        password(password),
        mtu(mtu),
        udpServerAddr(udpAddr),
        udpServerPort(udpPort) {

}

Socks5Config::Socks5Config() :
        serverAddr(""),
        serverPort(0),
        userName(""),
        password(""),
        mtu(0),
        udpServerAddr(""),
        udpServerPort(0) {

}
