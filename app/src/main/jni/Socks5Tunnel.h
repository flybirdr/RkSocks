//
// Created by 张开海 on 2023/11/3.
//

#ifndef R_SOCKS5TUNNEL_H
#define R_SOCKS5TUNNEL_H

#include "Tunnel.h"
#include "Socks5Config.h"
#include "TCPTunnel.h"
#include <vector>

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
        uint16_t remotePort;
    };

    class Socks5Tunnel : public TCPTunnel {

    private:
        Socks5Config mConfig;
        std::unique_ptr<Socks5Info> mSocksInfo;
        int mBindServerSocket;
        TunnelContext *mBindFakeTunnelContext;
        int mUdpServerSocketFd;
        TunnelContext *mUdpFakeTunnelContext;
        std::vector<Tunnel *> *mUdpTunnels;
    public:
        Socks5Tunnel() = delete;

        Socks5Tunnel(Socks5Config &socksInfo,
                     std::shared_ptr<EventLoop> &loop);

        ~Socks5Tunnel() override;

        bool attachOutboundContext() override;

        void handleInboundHandshake() override;

        void handleOutboundHandshake() override;

        void handleRead(TunnelContext *context) override;

        void handleClosed(TunnelContext *context) override;

        void packData() override;

        void unpackData() override;

        int createBindServerSocket(sockaddr_in *out);

        void handleBindServerSocketRead();

        void handleBindClosed();

        TunnelContext *createEmptyTunnelContext(int fd,sockaddr_in sockaddrIn);

        int createUdpServerSocket(sockaddr_in *in,sockaddr_in *out) const;

        void handleUdpServerSocketRead();

        void handleUdpClosed();
    };

} // R

#endif //R_SOCKS5TUNNEL_H
