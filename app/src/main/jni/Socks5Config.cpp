//
// Created by 张开海 on 2023/10/30.
//

#include "Socks5Config.h"

Socks5Config::Socks5Config(const std::string &addr,
                           int port,
                           const std::string &userName,
                           const std::string &password,
                           int mtu) :
        serverAddr(addr),
        serverPort(port),
        userName(userName),
        password(password),
        mtu(mtu) {

}

Socks5Config::Socks5Config() :
        serverAddr(""),
        serverPort(0),
        userName(""),
        password(""),
        mtu(0) {

}
