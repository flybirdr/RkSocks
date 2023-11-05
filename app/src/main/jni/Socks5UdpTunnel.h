//
// Created by 张开海 on 2023/11/4.
//

#ifndef R_SOCKS5UDPTUNNEL_H
#define R_SOCKS5UDPTUNNEL_H

#include "Tunnel.h"
#include "Socks5Config.h"
#include "UDPTunnel.h"

namespace R {

    class Socks5UdpTunnel : public UDPTunnel {
    private:
        RingBuffer *mHeader;

        Socks5UdpTunnel(int bufferLen,
                        std::shared_ptr<EventLoop> &loop,
                        Socks5Config &mConfig,
                        sockaddr_in &remote);


    public:
        Socks5Config mConfig;
        sockaddr_in mRemoteAddr;

        static Socks5UdpTunnel *create(int bufferLen,
                                       std::shared_ptr<EventLoop> &loop,
                                       Socks5Config &mConfig,
                                       sockaddr_in &remote);

        void initialize(char *buffer, int len);

        int parseHeaderAndAddr(RingBuffer* buffer);

        bool attachOutboundContext() override;

        ~Socks5UdpTunnel() override;

        void handleInboundHandshake() override;

        void handleOutboundHandshake() override;

        void packData() override;

        void unpackData() override;


    };

} // R

#endif //R_SOCKS5UDPTUNNEL_H
