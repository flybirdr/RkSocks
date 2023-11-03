//
// Created by 张开海 on 2023/11/3.
//

#include <arpa/inet.h>
#include "Socks5Server1.h"
#include "Logger.h"
#include "Util.h"

namespace R {
    Socks5Server1::Socks5Server1(Socks5Config &config) :
            mConfig(config),
            mServerSocketFd(0),
            mLooper(std::make_shared<EventLoop>(this)) {

    }

    void Socks5Server1::run() {
        mServerSocketFd = createSocks5ServerSocket();
        if (!mServerSocketFd) {
            LOGE("server socket not available!!");
            return;
        }
        mLooper->createLopper();
        mLooper->loop();
        mLooper->registerOnReadOnly(mServerSocketFd, nullptr);
        LOGI("run socks server!");
    }

    void Socks5Server1::quit() {
        mLooper->quit();
    }

    Socks5Server1::~Socks5Server1() {
        mLooper->quit();
    }

    int Socks5Server1::createSocks5ServerSocket() const {
        int ret;
        int fd;
        sockaddr_in addr_in{0};
        fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd <= 0) {
            LOGE("unable to create socket,%d:%s", errno, strerror(errno));
            goto create_fail;
        }
        Util::setNonBlocking(fd);
        addr_in.sin_addr.s_addr = inet_addr(mConfig.serverAddr.c_str());
        addr_in.sin_family = AF_INET;
        addr_in.sin_port = htons(mConfig.serverPort);
        ret = bind(fd, (sockaddr *) &addr_in, sizeof(addr_in));
        if (ret) {
            LOGE("unable to bind socket,%d:%s", errno, strerror(errno));
            goto created;
        }
        ret = listen(fd, 1024);
        if (ret) {
            LOGE("unable to listen socket at %s:%d,%d:%s", inet_ntoa(addr_in.sin_addr),
                 addr_in.sin_port, errno, strerror(errno));
            goto created;
        }
        LOGI("create socks server socket successfully!");
        return fd;

        created:
        close(fd);
        return 0;

        create_fail:
        return 0;
    }

    void Socks5Server1::handleServerSocketRead() {
        int fd = mServerSocketFd;
        sockaddr_in acceptAddr{0};
        socklen_t len = sizeof(acceptAddr);
        int acceptFd = accept(fd, (sockaddr *) &acceptAddr, &len);
        if (acceptFd < 0) {
            LOGE("failed to accept client,errno=%d:%s", errno, strerror(errno));
            return;
        }
        LOGI("accept client fd=%d,inbound addr=%s, inbound port=%d",
             acceptFd,
             inet_ntoa(acceptAddr.sin_addr),
             ntohs(acceptAddr.sin_port));
        //创建tunnel
        auto tunnel = new Socks5Tunnel{
                mConfig,
                mLooper
        };
        auto inbound = new TunnelContext{
                acceptFd,
                acceptAddr,
                mConfig.mtu,
                TunnelStage::STAGE_HANDSHAKE,
                tunnel
        };
        tunnel->inbound = inbound;

//        {
//            std::unique_lock<std::mutex> lock(mTunnelLock);
//            mTunnels.push_back(tunnel);
//        }
        if (!mLooper->registerOnReadOnly(inbound->fd, inbound)) {
            LOGE("unable to register read event,fd=%d", acceptFd);
            close(acceptFd);
        }
        LOGI("register read event,fd=%d", acceptFd);
    }

    void Socks5Server1::handleServerSocketClosed() {
        if (mServerSocketFd) {
            LOGE("close server fd=%d", mServerSocketFd);
            mLooper->unregister(mServerSocketFd);
            close(mServerSocketFd);
            mServerSocketFd = 0;
        }
    }

    void Socks5Server1::onReadEvent(void *ptr) {
        auto context = static_cast<TunnelContext *>(ptr);
        if (context == nullptr) {
            handleServerSocketRead();
        } else {
            context->tunnel->handleRead(context);
        }
    }

    void Socks5Server1::onWriteEvent(void *ptr) {
        auto context = static_cast<TunnelContext *>(ptr);
        if (context) {
            context->tunnel->handleWrite(context);
        }
    }

    void Socks5Server1::onErrorEvent(void *ptr) {
        auto context = static_cast<TunnelContext *>(ptr);
        if (context) {
            context->tunnel->handleClosed(context);
            handleTunnelClosed(context->tunnel);
        } else {
            handleServerSocketClosed();
        }
    }

    void Socks5Server1::onHupEvent(void *ptr) {
        auto context = static_cast<TunnelContext *>(ptr);
        if (context) {
            context->tunnel->handleClosed(context);
            handleTunnelClosed(context->tunnel);
        } else {
            handleServerSocketClosed();
        }
    }

    void Socks5Server1::handleTunnelClosed(Tunnel *tunnel) {
//        std::unique_lock<std::mutex> lock(mTunnelLock);
//        mTunnels.remove(tunnel);
//        delete tunnel;
        delete tunnel;
    }


} // R