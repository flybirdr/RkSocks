//
// Created by 张开海 on 2023/11/4.
//

#include <arpa/inet.h>
#include "Socks5UdpTunnel.h"
#include "Logger.h"
#include "Util.h"
#include "VpnUtil.h"

namespace R {
    Socks5UdpTunnel *
    Socks5UdpTunnel::create(int bufferLen,
                            std::shared_ptr<EventLoop> &loop,
                            Socks5Config &mConfig,
                            sockaddr_in &remote) {

        return new Socks5UdpTunnel(bufferLen, loop, mConfig, remote);
    }

    void Socks5UdpTunnel::initialize(char *buffer, int len) {
        inbound->inBuffer->write(buffer, len);
        parseHeaderAndAddr(inbound->inBuffer);
        attachOutboundContext();
    }

    Socks5UdpTunnel::Socks5UdpTunnel(int bufferLen,
                                     std::shared_ptr<EventLoop> &loop,
                                     Socks5Config &mConfig,
                                     sockaddr_in &remote) :
            UDPTunnel(bufferLen, loop),
            mHeader(nullptr),
            mConfig(mConfig),
            mRemoteAddr(remote) {

    }

    int Socks5UdpTunnel::parseHeaderAndAddr(Buffer *buffer) {
        int rsv = buffer->read2();
        int frag = buffer->read1();
        int atyp = buffer->read1();
        int dstAddrLen = 0;
        char *addrStr;
        if (atyp == 0x01) {
            int dstAddr = buffer->read4();
            in_addr inAddr{0};
            inAddr.s_addr = dstAddr;
            addrStr = inet_ntoa(inAddr);
            dstAddrLen = 4;
            mRemoteAddr.sin_addr.s_addr = dstAddr;
        } else if (atyp == 0x03) {
            dstAddrLen = buffer->read1();
            char *domainBuffer = new char[dstAddrLen];
            buffer->read(domainBuffer, dstAddrLen);
            delete[] domainBuffer;
        } else {
            int dstAddr = buffer->read1();
            dstAddrLen = 16;
        }
        int port = buffer->read2();
        int headerLen = 2 + 1 + 1 + dstAddrLen + 2;
        int dataLen = buffer->length();
        buffer->unread(headerLen);
        if (!mHeader) {
            mHeader = new Buffer{headerLen};
        }
        mRemoteAddr.sin_family = AF_INET;
        mRemoteAddr.sin_port = port;
        return headerLen;
    }

    bool Socks5UdpTunnel::attachOutboundContext() {
        int fd;
        fd = socket(AF_INET, SOCK_DGRAM, 0);
        if (fd == -1) {
            LOGE("failed to create udp outbound socket!");
            return false;
        }
        Util::setNonBlocking(fd);
        VpnUtil::protect(fd);
        LOGI("prepare to construct udp tunnel to %s:%d", inet_ntoa(mRemoteAddr.sin_addr),
             ntohs(mRemoteAddr.sin_port));
        if (connect(fd, (sockaddr *) &mRemoteAddr, sizeof(mRemoteAddr)) == -1) {
            LOGE("failed to connect to udp out bound,errno=%d:%s", errno, strerror(errno));
            return false;
        }
        outbound = new TunnelContext{
                fd,
                mRemoteAddr,
                mBufferLen,
                TunnelStage::STAGE_TRANSFER,
                this
        };
        if (!mLooper->registerOnReadOnly(outbound->fd, outbound)) {
            LOGE("unable to register read event,fd=%d", outbound->fd);
            handleClosed(outbound);
            return false;
        }
        return true;
    }


    Socks5UdpTunnel::~Socks5UdpTunnel() {
        if (mHeader) {
            delete mHeader;
            mHeader = nullptr;
        }
    }

    void Socks5UdpTunnel::handleInboundHandshake() {
        Tunnel::handleInboundHandshake();
    }

    void Socks5UdpTunnel::handleOutboundHandshake() {
        Tunnel::handleOutboundHandshake();
    }

    void Socks5UdpTunnel::packData() {
        auto readBuffer = inbound->inBuffer;
        auto writeBuffer = outbound->outBuffer;
        if (readBuffer->length() < 10) {
            return;
        }
        int headerLen = parseHeaderAndAddr(readBuffer);
        mHeader->readFrom(readBuffer, headerLen);
        int frag = mHeader->get(2);
        if (frag) {
            //drop
        } else {
            writeBuffer->readFrom(readBuffer);
        }

    }

    void Socks5UdpTunnel::unpackData() {
        auto readBuffer = outbound->inBuffer;
        auto writeBuffer = inbound->outBuffer;
        mHeader->writeTo(writeBuffer);
        readBuffer->writeTo(writeBuffer);
    }


} // R