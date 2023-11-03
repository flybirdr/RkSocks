//
// Created by 张开海 on 2023/11/3.
//

#ifndef R_SOCKS5SERVER1_H
#define R_SOCKS5SERVER1_H

#include "Socks5Tunnel.h"
#include "Socks5Config.h"
#include "EventLoop.h"
#include <list>

namespace R {
    class Socks5Server1 : public EventListener {
    private:
        Socks5Config mConfig;
        int mServerSocketFd;
        std::shared_ptr<EventLoop> mLooper;
//        std::list<Tunnel *> mTunnels;
//        std::mutex mTunnelLock;
    public:
        void run();

        void quit();

        Socks5Server1(Socks5Config& config);

        int createSocks5ServerSocket() const;

        void handleServerSocketRead();

        void handleServerSocketClosed();

        void handleTunnelClosed(Tunnel *tunnel);

        virtual ~Socks5Server1();

    public:
        void onReadEvent(void *ptr) override;

        void onWriteEvent(void *ptrn) override;

        void onErrorEvent(void *ptr) override;

        void onHupEvent(void *ptr) override;
    };

} // R

#endif //R_SOCKS5SERVER1_H
