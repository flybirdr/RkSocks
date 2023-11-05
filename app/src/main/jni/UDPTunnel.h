//
// Created by 张开海 on 2023/11/4.
//

#ifndef R_UDPTUNNEL_H
#define R_UDPTUNNEL_H

#include "Tunnel.h"

namespace R {

    class UDPTunnel : public Tunnel {

    public:
        UDPTunnel(int bufferLen, std::shared_ptr<EventLoop> &loop);

        void handleRead(TunnelContext *context) override;

        void handleWrite(TunnelContext *context) override;

        void handleClosed(TunnelContext *context) override;

        void registerTunnelEvent() override;
    };

} // R

#endif //R_UDPTUNNEL_H
