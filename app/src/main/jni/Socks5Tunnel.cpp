//
// Created by 张开海 on 2023/11/3.
//

#include <arpa/inet.h>
#include "Socks5Tunnel.h"
#include "Util.h"
#include "Logger.h"
#include "VpnUtil.h"
#include "TCPTunnel.h"
#include "Socks5UdpTunnel.h"

namespace R {
    Socks5Tunnel::Socks5Tunnel(Socks5Config &socksInfo,
                               std::shared_ptr<EventLoop> &loop) :
            TCPTunnel(socksInfo.mtu, loop),
            mConfig(socksInfo),
            mSocksInfo(new Socks5Info{
                    .handshake=SOCKS5_HANDSHAKE::SOCKS5_STEP1
            }),
            mBindServerSocket(0),
            mBindFakeTunnelContext(nullptr),
            mUdpServerSocketFd(0),
            mUdpFakeTunnelContext(nullptr),
            mUdpTunnels(nullptr) {

    }

    Socks5Tunnel::~Socks5Tunnel() {

    };

    bool Socks5Tunnel::attachOutboundContext() {
        int fd, ret;

        fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

        if (fd == -1) {
            LOGE("Unable to create outbound socket!!!");
            return false;
        }

        Util::setNonBlocking(fd);
        VpnUtil::protect(fd);

        sockaddr_in sockaddrIn{0};
        sockaddrIn.sin_port = htons(mSocksInfo->remotePort);
        sockaddrIn.sin_family = AF_INET;
        sockaddrIn.sin_addr.s_addr = inet_addr(mSocksInfo->remoteAddr.c_str());

        ret = connect(fd, (sockaddr *) &sockaddrIn, sizeof(sockaddrIn));
        LOGI("fd=%d,connect to %s:%d", fd, inet_ntoa(sockaddrIn.sin_addr),
             ntohs(sockaddrIn.sin_port));
        //非阻塞模式返回-1需要判断errno
        if (ret == -1 && errno != EINPROGRESS) {
            LOGE("Unable to connect outbound socket, %d:%s", errno, strerror(errno));
            close(fd);
            return false;
        }
        outbound = new TunnelContext{
                fd,
                sockaddrIn,
                mBufferLen,
                TunnelStage::STAGE_TRANSFER,
                this
        };
        return true;
    }

    void Socks5Tunnel::handleInboundHandshake() {
        int fd = inbound->fd;
        auto inBuffer = inbound->inBuffer;
        auto outBuffer = inbound->outBuffer;
        int length = inBuffer->length();
        auto handshake = mSocksInfo->handshake;
        //握手第一阶段:
        //1、校验协议版本号
        //2、选择验证方法
        if (handshake == SOCKS5_HANDSHAKE::SOCKS5_STEP1) {
            if (length < 3) {
                LOGE("invaid socks request length=%d", length);
                return;
            }
            HandshakeStep1 handshake1{0};
            inBuffer->read(reinterpret_cast<char *>(&handshake1), sizeof(handshake1));
            if (handshake1.version != 0x05) {
                LOGI("Unsupported socks version:%d", handshake1.version);
                return;
            }
            //验证下长度
            if (length < handshake1.nmethods + sizeof(handshake1)) {
                LOGI("handshake1 length not enough");
                return;
            }
            char *meghtods = new char[handshake1.nmethods];
            inBuffer->read(meghtods, handshake1.nmethods);
            char method = 0xff;
            //挑选支持的验证方式
            for (int i = 0; i < handshake1.nmethods; ++i) {
                //只支持用户名密码认证
                if ((*meghtods & 0xff) == 0x00 ||
                    (*meghtods & 0xff) == 0x02) {
                    method = *meghtods;
                    break;
                }
                meghtods += 1;
            }
            delete[] meghtods;
            //处理支持的验证方式
            char response[2] = {0x05, method};
            std::string responseStr = Util::toHexString(response, sizeof(response));
            LOGI("step1 response=%s", responseStr.c_str());
            outBuffer->write(response, sizeof(response));
            if (method == 0xff) {
                //不支持所有验证方式
                return;
            } else {
                if (method == 0x00) {
                    mSocksInfo->handshake = SOCKS5_HANDSHAKE::SOCKS5_STEP3;
                    mSocksInfo->method = 0x00;
                } else {
                    mSocksInfo->handshake = SOCKS5_HANDSHAKE::SOCKS5_STEP2;
                    mSocksInfo->method = method;
                }
                LOGI("client handleInboundHandshake step1 fd=%d", fd);
            }
            return;
        }
        //握手第二阶段:
        //1、进行鉴权
        if (handshake == SOCKS5_HANDSHAKE::SOCKS5_STEP2) {

            int version = inBuffer->read1();
            if (version != 0x01) {
                LOGE("unsupported socket version=%d", version);
                return;
            }
            //0101表示认证失败
            char response[2] = {0x01, 0x01};
            switch (mSocksInfo->method) {
                case 0x02: {
                    int userLen = inBuffer->read1();
                    if (userLen - inBuffer->length() > 0) {
                        LOGE("readFrom invalid length=%d", length);
                        return;
                    }
                    char userNameChar[userLen];
                    inBuffer->read(userNameChar, userLen);
                    std::string userName(userNameChar, userLen);

                    int passLen = inBuffer->read1();
                    if (passLen - inBuffer->length() > 0) {
                        LOGE("readFrom invalid length=%d", length);
                        return;
                    }
                    char pwChar[passLen];
                    inBuffer->read(pwChar, passLen);
                    std::string passWord(pwChar, passLen);
                    if (userName == mConfig.userName &&
                        passWord == mConfig.password) {
                        mSocksInfo->handshake = SOCKS5_HANDSHAKE::SOCKS5_STEP3;
                        //0100表示认证成功
                        response[1] = 0x00;
                    }
                    std::string responseStr = Util::toHexString(response, sizeof(response));
                    LOGI("step2 response=%s", responseStr.c_str());
                    outBuffer->write(response, sizeof(response));
                    LOGI("client handleInboundHandshake step2 fd=%d", fd);
                    break;
                }
                default: {
                    LOGE("Unsupported auth method.");
                    break;
                }
            }
            return;
        }
        //握手第三阶段:
        //1、处理请求[CONNECT/BIND/UDP ASSOCIATE]
        //2、建立CONNECT转发通道/建立BIND监听/建立UDP监听
        //3、对于BIND、UDP当客户端连入后建立数据转发通道
        if (handshake == SOCKS5_HANDSHAKE::SOCKS5_STEP3) {
            int oriLength = inBuffer->length();
            int version = inBuffer->read1();
            if (version != 0x05) {
                LOGE("unsupported socket version=%d", version);
                return;
            }
            int cmd = inBuffer->read1();
            int rev = inBuffer->read1();
            int atyp = inBuffer->read1();
            int addrLen = 0;

            //ipv4
            if (atyp == 0x01) {
                addrLen = 4;
            }
                //域名
            else if (atyp == 0x03) {
                addrLen = inBuffer->read1();
            }
                //ipv6
            else if (atyp == 0x04) {
                addrLen = 16;
            }
            char addrBuffer[addrLen];

            inBuffer->read(addrBuffer, addrLen);
            uint16_t port = inBuffer->read2();

            mSocksInfo->handshake = SOCKS5_HANDSHAKE::SOCKS5_FINISH;
            mSocksInfo->remoteAddr = inet_ntoa(*(in_addr *) addrBuffer);
            mSocksInfo->remotePort = ntohs(port);
            mSocksInfo->cmd = cmd;
            mSocksInfo->atyp = atyp;
            inbound->tunnelStage = TunnelStage::STAGE_TRANSFER;

            int headerLen = oriLength - inBuffer->length();

            Buffer response{headerLen};
            response.write1(version);
            response.write1(0x00);
            response.write1(0x00);
            response.write1(atyp);
            //域名和IPV6目前不支持，设置REP为0x08
            if (atyp == 0x03) {
                response.write1(addrLen);
                response.set(1, 0x08);
            } else if (atyp == 0x04) {
                response.set(1, 0x08);
            }
            if (cmd == 0x01) {
                response.write(addrBuffer, addrLen);
                response.write2(port);
                //创建远程连接
                if (!attachOutboundContext()) {
                    LOGE("connect to remote failed!");
                    handleClosed(inbound);
                    return;
                }
                if (!mLooper->registerOnReadOnly(outbound->fd, outbound)) {
                    LOGE("unable to register read event,fd=%d", outbound->fd);
                    handleClosed(outbound);
                }
            }
                //BIND
            else if (cmd == 0x02) {
                sockaddr_in bindAddrIn{0};
                mBindServerSocket = createBindServerSocket(&bindAddrIn);
                if (mBindServerSocket) {
                    mBindFakeTunnelContext = createEmptyTunnelContext(mBindServerSocket,
                                                                      bindAddrIn);
                    if (!mLooper->registerOnReadOnly(mBindServerSocket, mBindFakeTunnelContext)) {
                        LOGE("unable to register read event,fd=%d", mBindServerSocket);
                        handleClosed(mBindFakeTunnelContext);
                    }
                    //todo ipv6 domain
                    response.write4(bindAddrIn.sin_addr.s_addr);
                    response.write2(bindAddrIn.sin_port);
                } else {
                    response.set(1, 0x01);
                    response.write(addrBuffer, addrLen);
                    response.write2(port);
                }
            }
                //UDP ASSOCIATE
            else if (cmd == 0x03) {
                sockaddr_in udpServerAddrIn{0};
                mUdpServerSocketFd = createUdpServerSocket(nullptr, &udpServerAddrIn);
                if (mUdpServerSocketFd) {
                    mUdpFakeTunnelContext = createEmptyTunnelContext(mUdpServerSocketFd,
                                                                     udpServerAddrIn);
                    if (!mLooper->registerOnReadOnly(mUdpServerSocketFd, mUdpFakeTunnelContext)) {
                        LOGE("unable to register read event,fd=%d", mUdpServerSocketFd);
                        handleClosed(mUdpFakeTunnelContext);
                    }
                    //todo ipv6 domain
                    response.write4(udpServerAddrIn.sin_addr.s_addr);
                    response.write2(udpServerAddrIn.sin_port);
                } else {
                    response.set(1, 0x01);
                    response.write(addrBuffer, addrLen);
                    response.write2(port);
                }
                LOGI("handle udp associate to %s:%d", inet_ntoa(*(in_addr *) addrBuffer),
                     ntohs(port));
            } else {
                response.set(1, 0x07);
                response.write(addrBuffer, addrLen);
                response.write2(port);
            }
            LOGI("step3 response=%s", response.toString().c_str());
            response.writeTo(outBuffer);
            LOGI("client handleInboundHandshake step3 fd=%d", fd);
            return;
        }
    }


    void Socks5Tunnel::handleOutboundHandshake() {

    }

    void Socks5Tunnel::packData() {
        auto readBuffer = inbound->inBuffer;
        auto writeBuffer = outbound->outBuffer;
        writeBuffer->readFrom(readBuffer);
    }

    void Socks5Tunnel::unpackData() {
        auto readBuffer = outbound->inBuffer;
        auto writeBuffer = inbound->outBuffer;
        writeBuffer->readFrom(readBuffer);
    }

    int Socks5Tunnel::createBindServerSocket(sockaddr_in *out) {
        int ret;
        int fd;
        int opt = 1;
        sockaddr_in addr_in{0};

        socklen_t len = sizeof(*out);
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
        addr_in.sin_port = 0;
        ret = bind(fd, (sockaddr *) &addr_in, sizeof(addr_in));
        if (ret) {
            LOGE("unable to bind socket,%d:%s", errno, strerror(errno));
            goto created;
        }

        if (getsockname(fd, (sockaddr *) out, &len) == -1) {
            LOGE("unable to get socket,%d:%s", errno, strerror(errno));
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

    TunnelContext *Socks5Tunnel::createEmptyTunnelContext(int fd, sockaddr_in sockaddrIn) {
        return new TunnelContext{
                fd,
                sockaddrIn,
                0,
                TunnelStage::STAGE_TRANSFER,
                this
        };
    }

    void Socks5Tunnel::handleRead(TunnelContext *context) {
        //处理BIND请求,建立本地和BIND Server的数据通道
        if (context == mBindFakeTunnelContext) {
            //accept远端服务器作为outbound
            handleBindServerSocketRead();
        } else if (context == mUdpFakeTunnelContext) {
            handleUdpServerSocketRead();
        }
            //处理数据转发
        else {
            TCPTunnel::handleRead(context);
        }
    }

    int Socks5Tunnel::createUdpServerSocket(sockaddr_in *in, sockaddr_in *out) const {

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
        if (in) {
            if (bind(fd, (sockaddr *) in, sizeof(*in)) == -1) {
                close(fd);
                LOGE("failed to bind udp server socket,errno=%d:%s", errno, strerror(errno));
                return 0;
            }
        } else {
            sockaddr_in addrIn{0};
            addrIn.sin_family = AF_INET;
            addrIn.sin_addr.s_addr = inet_addr(mConfig.serverAddr.c_str());
            addrIn.sin_port = 0;
            if (bind(fd, (sockaddr *) &addrIn, sizeof(addrIn)) == -1) {
                close(fd);
                LOGE("failed to bind udp server socket,errno=%d:%s", errno, strerror(errno));
                return 0;
            }
        }

        if (out) {
            socklen_t len = sizeof(*out);
            if (getsockname(fd, (sockaddr *) out, &len) == -1) {
                LOGE("unable to get socket,%d:%s", errno, strerror(errno));
                close(fd);
                return 0;
            }
        }
        Util::setNonBlocking(fd);
        LOGI("create udp server successfully!!!");
        return fd;
    }

    void Socks5Tunnel::handleUdpServerSocketRead() {

        sockaddr_in sockaddrIn{0};
        socklen_t sockaddrLen = sizeof(sockaddrIn);
        char tmp[mConfig.mtu];
        //得到UDP客户端地址和端口
        int r = recvfrom(mUdpServerSocketFd, tmp, mConfig.mtu, 0,
                         (sockaddr *) &sockaddrIn, &sockaddrLen);
        LOGE("udp data=%s", Util::toHexString(tmp, r).c_str());
        LOGI("prepare to construct udp tunnel for %s:%d", inet_ntoa(sockaddrIn.sin_addr),
             ntohs(sockaddrIn.sin_port));

        int tunnelFd = createUdpServerSocket(&mUdpFakeTunnelContext->sockAddr, nullptr);
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
        if (!mUdpTunnels) {
            mUdpTunnels = new std::vector<Tunnel *>();
        }
        mUdpTunnels->push_back(udpTunnel);
        LOGI("register read event,fd=%d", inbound->fd);
    }

    void Socks5Tunnel::handleUdpClosed() {
        if (mUdpServerSocketFd) {
            LOGE("close udp server fd=%d", mUdpServerSocketFd);
            mLooper->unregister(mUdpServerSocketFd);
            close(mUdpServerSocketFd);
            mUdpServerSocketFd = 0;
        }
        if (mUdpFakeTunnelContext) {
            delete mUdpFakeTunnelContext;
            mUdpFakeTunnelContext = nullptr;
        }
        if (mUdpTunnels) {
            for (const auto &item: *mUdpTunnels) {
                item->handleClosed(nullptr);
            }
            mUdpTunnels->clear();
            mUdpTunnels = nullptr;
        }
    }

    void Socks5Tunnel::handleBindClosed() {
        if (mBindServerSocket) {
            LOGE("close bind server fd=%d", mBindServerSocket);
            mLooper->unregister(mBindServerSocket);
            close(mBindServerSocket);
            mBindServerSocket = 0;
        }
        if (mBindFakeTunnelContext) {
            delete mBindFakeTunnelContext;
            mBindFakeTunnelContext = nullptr;
        }
    }

    void Socks5Tunnel::handleBindServerSocketRead() {
        int fd = mBindServerSocket;
        sockaddr_in acceptAddr{0};
        socklen_t len = sizeof(acceptAddr);
        int acceptFd = accept(fd, (sockaddr *) &acceptAddr, &len);
        if (acceptFd < 0) {
            LOGE("failed to accept bind server,errno=%d:%s", errno, strerror(errno));
            return;
        }
        LOGI("accept bind server fd=%d,outbound addr=%s, outbound port=%d",
             acceptFd,
             inet_ntoa(acceptAddr.sin_addr),
             ntohs(acceptAddr.sin_port));
        outbound = new TunnelContext{
                fd,
                acceptAddr,
                mBufferLen,
                TunnelStage::STAGE_TRANSFER,
                this
        };

        //BIND response
        Buffer response(100);
        response.write1(0x05);
        response.write1(0x00);
        response.write1(0x00);
        response.write1(0x01);
        response.write4(acceptAddr.sin_addr.s_addr);
        response.write2(acceptAddr.sin_port);
        response.writeTo(outbound->inBuffer);

        registerTunnelEvent();
    }

    void Socks5Tunnel::handleClosed(TunnelContext *context) {
        //BIND和UDP ASSOCIATE生命周期处理
        handleBindClosed();
        handleUdpClosed();
        TCPTunnel::handleClosed(context);
    }

} // R