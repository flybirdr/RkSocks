//
// Created by 张开海 on 2023/11/3.
//

#ifndef R_SOCKS5TUNNEL_H
#define R_SOCKS5TUNNEL_H

#include "Tunnel.h"
#include "Socks5Config.h"

namespace R {

    enum SOCKS5_HANDSHAKE {
        SOCKS5_STEP1,      //1、验证方法
        SOCKS5_STEP2,      //2、鉴权
        SOCKS5_STEP3,      //3、发送请求
        SOCKS5_FINISH      //4、传输阶段
    };
    struct HandshakeStep1 {
        char version;
        char nmethods;
    };

    class Socks5Info {
    public:
        SOCKS5_HANDSHAKE handshake;
        //0x00=无需验证 0x01=GSSAPI 0x02=UserName/Password ...
        int method;
        //0x01=CONNECT 0x02=BIND 0x03=UDP
        int cmd;
        //0x01=IPv4 0x03=Domain 0x04=IPv6
        int atyp;
        //远程地址,因为可能是IP地址也可能是域名，所有用字符串来承载
        std::string remoteAddr;
        //远程端口
        int remotePort;
    };

    class Socks5Tunnel : public Tunnel {

    private:
        Socks5Config mConfig;
        std::unique_ptr<Socks5Info> mSocksInfo;
    public:
        Socks5Tunnel() = delete;

        Socks5Tunnel(Socks5Config& socksInfo,
                     std::shared_ptr<EventLoop> &loop);

        ~Socks5Tunnel() override;

        bool attachOutboundContext() override;

        void inboundHandleHandshake() override;

        void outboundHandleHandshake() override;

        void packData() override;

        void unpackData() override;
    };

} // R

#endif //R_SOCKS5TUNNEL_H
