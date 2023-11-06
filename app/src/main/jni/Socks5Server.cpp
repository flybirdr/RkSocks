//
// Created by 张开海 on 2023/11/3.
//

#include <arpa/inet.h>
#include "Socks5Server.h"
#include "Logger.h"
#include "Util.h"
#include "Socks5UdpTunnel.h"

namespace R {
    Socks5Server::Socks5Server(Socks5Config &config) :
            mConfig(config),
            mServerSocketFd(0),
            mLooper(std::make_shared<EventLoop>(this)) {

    }

    void Socks5Server::run() {
        mServerSocketFd = createSocks5ServerSocket();
        if (!mServerSocketFd) {
            LOGE("server socket not available!!");
            return;
        }
        mLooper->createLopper();
        mLooper->loop();
        mLooper->registerOnReadOnly(mServerSocketFd, &mServerSocketFd);
        LOGI("run socks server!");
    }

    void Socks5Server::quit() {
        mLooper->quit();
    }

    Socks5Server::~Socks5Server() {
        mLooper->quit();
    }

    int Socks5Server::createSocks5ServerSocket() const {
        int ret;
        int fd;
        int opt = 1;
        sockaddr_in addr_in{0};
        fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd == -1) {
            LOGE("unable to create socket,%d:%s", errno, strerror(errno));
            goto create_fail;
        }
        ret = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        if (ret == -1) {
            LOGE("failed to reuse addr on tcp fd=%d,errno=%d:%s", fd, errno,
                 strerror(errno));
            close(fd);
            return 0;
        }
        ret = setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
        if (ret == -1) {
            LOGE("failed to reuse port on tcp fd=%d,errno=%d:%s", fd, errno,
                 strerror(errno));
            close(fd);
            return 0;
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

    void Socks5Server::handleServerSocketRead() {
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
        if (!mLooper->registerOnReadOnly(inbound->fd, inbound)) {
            LOGE("unable to register read event,fd=%d", acceptFd);
            close(acceptFd);
        }
        tunnel->registerTunnelEvent();
        LOGI("register read event,fd=%d", acceptFd);
    }

    void Socks5Server::handleServerSocketClosed() {
        if (mServerSocketFd) {
            LOGE("close server fd=%d", mServerSocketFd);
            mLooper->unregister(mServerSocketFd);
            close(mServerSocketFd);
            mServerSocketFd = 0;
        }
    }

    void Socks5Server::onReadEvent(void *ptr) {
        if (ptr == &mServerSocketFd) {
            handleServerSocketRead();
        }  else if (ptr) {
            auto context = static_cast<TunnelContext *>(ptr);
            context->tunnel->handleRead(context);
        }
    }

    void Socks5Server::onWriteEvent(void *ptr) {
        auto context = static_cast<TunnelContext *>(ptr);
        if (context) {
            context->tunnel->handleWrite(context);
        }
    }

    void Socks5Server::onErrorEvent(void *ptr) {
        if (ptr == &mServerSocketFd) {
            handleServerSocketClosed();
        }  else {
            auto context = static_cast<TunnelContext *>(ptr);
            context->tunnel->handleClosed(context);
            handleTunnelClosed(context->tunnel);
        }
    }

    void Socks5Server::onHupEvent(void *ptr) {
        if (ptr == &mServerSocketFd) {
            handleServerSocketClosed();
        } else {
            auto context = static_cast<TunnelContext *>(ptr);
            context->tunnel->handleClosed(context);
            handleTunnelClosed(context->tunnel);
        }
    }

    void Socks5Server::handleTunnelClosed(Tunnel *tunnel) {
        delete tunnel;
    }
} // R