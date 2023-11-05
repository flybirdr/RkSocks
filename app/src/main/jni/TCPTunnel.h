//
// Created by 张开海 on 2023/11/4.
//

#ifndef R_TCPTUNNEL_H
#define R_TCPTUNNEL_H

#include "Tunnel.h"

namespace R {

    class TCPTunnel : public Tunnel {
    public:
        TCPTunnel(int bufferLen, std::shared_ptr<EventLoop> &loop);

        void handleRead(TunnelContext *context) override;

        void handleWrite(TunnelContext *context) override;

        void handleClosed(TunnelContext *context) override;
    };

} // R

#endif //R_TCPTUNNEL_H
