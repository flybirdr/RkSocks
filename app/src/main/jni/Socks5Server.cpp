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
            mUdpServerSocketFd(0),
            mLooper(std::make_shared<EventLoop>(this)) {

    }

    void Socks5Server::run() {
        mServerSocketFd = createSocks5ServerSocket();
        mUdpServerSocketFd = createUdpServerSocket(false);
        if (!mServerSocketFd) {
            LOGE("server socket not available!!");
            return;
        }
        if (!mUdpServerSocketFd) {
            LOGE("udp server socket not available!!");
            return;
        }
        mLooper->createLopper();
        mLooper->loop();
        mLooper->registerOnReadOnly(mServerSocketFd, &mServerSocketFd);
        mLooper->registerOnReadOnly(mUdpServerSocketFd, &mUdpServerSocketFd);
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

//        {
//            std::unique_lock<std::mutex> lock(mTunnelLock);
//            mTunnels.push_back(tunnel);
//        }
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
        } else if (ptr == &mUdpServerSocketFd) {
            handleUdpServerSocketRead();
        } else if (ptr) {
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
        } else if (ptr == &mUdpServerSocketFd) {
            handleUdpServerSocketClosed();
        } else {
            auto context = static_cast<TunnelContext *>(ptr);
            context->tunnel->handleClosed(context);
            handleTunnelClosed(context->tunnel);
        }
    }

    void Socks5Server::onHupEvent(void *ptr) {
        if (ptr == &mServerSocketFd) {
            handleServerSocketClosed();
        } else if (ptr == &mUdpServerSocketFd) {
            handleUdpServerSocketClosed();
        } else {
            auto context = static_cast<TunnelContext *>(ptr);
            context->tunnel->handleClosed(context);
            handleTunnelClosed(context->tunnel);
        }
    }

    void Socks5Server::handleTunnelClosed(Tunnel *tunnel) {
//        std::unique_lock<std::mutex> lock(mTunnelLock);
//        mTunnels.remove(tunnel);
//        delete tunnel;
        delete tunnel;
    }

    int Socks5Server::createUdpServerSocket(bool forTunnel) const {

        int fd = socket(AF_INET, SOCK_DGRAM, 0);
        if (fd == -1) {
            LOGE("failed to create udp server socket,errno=%d:%s", errno, strerror(errno));
            return 0;
        }
        int opt = 1, ret;
        ret = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        if (ret == -1) {
            LOGE("failed to reuse addr on udp fd=%d,errno=%d:%s", fd, errno,
                 strerror(errno));
            close(fd);
            return 0;
        }
        ret = setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
        if (ret == -1) {
            LOGE("failed to reuse port on udp fd=%d,errno=%d:%s", fd, errno,
                 strerror(errno));
            close(fd);
            return 0;
        }

        sockaddr_in sockaddrIn{0};
        sockaddrIn.sin_port = htons(mConfig.udpServerPort);
        sockaddrIn.sin_family = AF_INET;
        sockaddrIn.sin_addr.s_addr = INADDR_ANY;
        if (bind(fd, (sockaddr *) &sockaddrIn, sizeof(sockaddrIn)) == -1) {
            close(fd);
            LOGE("failed to bind udp server socket,errno=%d:%s", errno, strerror(errno));
            return 0;
        }
        Util::setNonBlocking(fd);
        LOGI("create udp server successfully!!!");
        return fd;
    }

    void Socks5Server::handleUdpServerSocketRead() {

        sockaddr_in sockaddrIn{0};
        socklen_t sockaddrLen = sizeof(sockaddrIn);
        char tmp[mConfig.mtu];
        //得到UDP客户端地址和端口
        int r = recvfrom(mUdpServerSocketFd, tmp, mConfig.mtu, 0,
                         (sockaddr *) &sockaddrIn, &sockaddrLen);
        LOGE("udp data=%s",Util::toHexString(tmp,r).c_str());
        LOGI("prepare to construct udp tunnel for %s:%d", inet_ntoa(sockaddrIn.sin_addr),
             ntohs(sockaddrIn.sin_port));
        int tunnelFd = createUdpServerSocket(true);
        if (!tunnelFd) {
            return;
        }
        sockaddrIn.sin_family = AF_INET;
        auto udpTunnel = Socks5UdpTunnel::create(mConfig.mtu,
                                                 mLooper,
                                                 mConfig,
                                                 sockaddrIn);
        auto inbound = new TunnelContext{
                tunnelFd,
                sockaddrIn,
                mConfig.mtu,
                TunnelStage::STAGE_TRANSFER,
                udpTunnel
        };
        udpTunnel->inbound = inbound;
        udpTunnel->initialize(tmp, r);
        //注册事件
        if (!mLooper->registerOnReadOnly(inbound->fd, inbound)) {
            LOGE("unable to register read event,fd=%d", inbound->fd);
            close(tunnelFd);
            delete udpTunnel;
            return;
        }
        udpTunnel->registerTunnelEvent();
        if (connect(tunnelFd, (sockaddr *) &sockaddrIn, sockaddrLen) == -1) {
            LOGE("failed to connect client udp, errno=%d:%s", errno, strerror(errno));
            return;
        }

        LOGI("register read event,fd=%d", inbound->fd);
    }

    void Socks5Server::handleUdpServerSocketClosed() {
        if (mUdpServerSocketFd) {
            LOGE("close server fd=%d", mUdpServerSocketFd);
            mLooper->unregister(mUdpServerSocketFd);
            close(mUdpServerSocketFd);
            mUdpServerSocketFd = 0;
        }
    }


} // R