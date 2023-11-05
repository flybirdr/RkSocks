//
// Created by 张开海 on 2023/11/3.
//

#include <arpa/inet.h>
#include "Socks5Tunnel.h"
#include "Util.h"
#include "Logger.h"
#include "VpnUtil.h"
#include "TCPTunnel.h"

namespace R {
    Socks5Tunnel::Socks5Tunnel(Socks5Config &socksInfo,
                               std::shared_ptr<EventLoop> &loop) :
            TCPTunnel(socksInfo.mtu, loop),
            mConfig(socksInfo),
            mSocksInfo(new Socks5Info{
                    .handshake=SOCKS5_HANDSHAKE::SOCKS5_STEP1
            }) {

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

    void Socks5Tunnel::inboundHandleHandshake() {
        int fd = inbound->fd;
        auto inBuffer = inbound->inBuffer;
        auto outBuffer = inbound->outBuffer;
        int length = inBuffer->length();
        auto handshake = mSocksInfo->handshake;
        //握手第一阶段：
        //1、校验协议版本号
        //2、选择验证类型
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
                LOGI("client inboundHandleHandshake step1 fd=%d", fd);
            }
            return;
        }
        //握手第二阶段
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
                    char *userNameChar = new char[userLen];
                    inBuffer->read(userNameChar, userLen);
                    std::string userName(userNameChar, userLen);
                    delete[] userNameChar;
                    int passLen = inBuffer->read1();
                    if (passLen - inBuffer->length() > 0) {
                        LOGE("readFrom invalid length=%d", length);
                        return;
                    }
                    char *pwChar = new char[passLen];
                    inBuffer->read(pwChar, passLen);
                    std::string passWord(pwChar, passLen);
                    delete[] pwChar;
                    if (userName == mConfig.userName &&
                        passWord == mConfig.password) {
                        mSocksInfo->handshake = SOCKS5_HANDSHAKE::SOCKS5_STEP3;
                        //0100表示认证成功
                        response[1] = 0x00;
                    }
                    std::string responseStr = Util::toHexString(response, sizeof(response));
                    LOGI("step2 response=%s", responseStr.c_str());
                    outBuffer->write(response, sizeof(response));
                    LOGI("client inboundHandleHandshake step2 fd=%d", fd);
                    break;
                }
                default: {
                    LOGE("Unsupported auth method.");
                    break;
                }
            }
            return;
        }
        if (handshake == SOCKS5_HANDSHAKE::SOCKS5_STEP3) {
            int version = inBuffer->read1();
            if (version != 0x05) {
                LOGE("unsupported socket version=%d", version);
                return;
            }
            int cmd = inBuffer->read1();
            int rev = inBuffer->read1();
            int atyp = inBuffer->read1();
            int addrOffset = 0;
            int addrLen = 0;
            //ipv4
            if (atyp == 0x01) {
                addrLen = 4;
            }
                //域名
            else if (atyp == 0x03) {
                addrOffset = 1;
                addrLen = inBuffer->read1();
            }
                //ipv6
            else if (atyp == 0x04) {
                addrLen = 16;
            }
            std::string addr;
            if (atyp == 0x01) {
                //ipv4
                struct in_addr inAddr{0};
//                inAddr.s_addr = (inBuffer->read1() << 24) |
//                                (inBuffer->read1() << 16) |
//                                (inBuffer->read1() << 8) |
//                                (inBuffer->read1() << 0);
                inAddr.s_addr = inBuffer->read4();

                char *addrc = inet_ntoa(inAddr);
                addr = std::string(addrc);
            } else if (atyp == 0x03) {
                //域名
                char *addrChar = new char[addrLen];
                inBuffer->read(addrChar, addrLen);
                addr = std::string(addrChar, addrLen);
                delete[] addrChar;
            } else if (atyp == 0x04) {
                //ipv6
            }

            int16_t port = htons(inBuffer->read2());

            mSocksInfo->handshake = SOCKS5_HANDSHAKE::SOCKS5_FINISH;
            mSocksInfo->remoteAddr = addr;
            mSocksInfo->remotePort = port;
            mSocksInfo->cmd = cmd;
            mSocksInfo->atyp = atyp;
            inbound->tunnelStage = TunnelStage::STAGE_TRANSFER;

            int responseLen = 1 + 1 + 1 + 1 + addrOffset + addrLen + 2;
            char *response = new char[responseLen]{0};
            response[0] = version;
            response[1] = 0x00;
            response[2] = 0x00;
            response[3] = atyp;
            if (atyp == 0x03) {
                response[4] = addrLen;
            }
            in_addr_t addr_t = inet_addr(addr.c_str());
            if (cmd == 0x01) {
                memcpy(response + 4 + addrOffset, &addr_t, addrLen);
                memcpy(response + 4 + addrOffset + addrLen, &port, sizeof(int16_t));
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
                //UDP ASSOCIATE
            else if (cmd == 0x03) {
                sockaddr_in sockaddrIn{0};
                sockaddrIn.sin_port = htons(mConfig.udpServerPort);
                sockaddrIn.sin_family = AF_INET;
                sockaddrIn.sin_addr.s_addr = inet_addr(mConfig.udpServerAddr.c_str());

                memcpy(response + 4 + addrOffset, &sockaddrIn.sin_addr,
                       sizeof(sockaddrIn.sin_addr));
                memcpy(response + 4 + addrOffset + sizeof(sockaddrIn.sin_addr),
                       &sockaddrIn.sin_port, sizeof(sockaddrIn.sin_port));

                LOGI("handle udp associate to %s:%d", addr.c_str(), port);
                createUdpAssociateTunnel();
            }
            std::string responseStr = Util::toHexString(response, responseLen);
            LOGI("step3 response=%s", responseStr.c_str());
            LOGI("client inboundHandleHandshake step3 fd=%d", fd);
            outBuffer->write(response, responseLen);
            delete[] response;

            return;
        }
    }

    void Socks5Tunnel::outboundHandleHandshake() {

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

    void Socks5Tunnel::createUdpAssociateTunnel() {

    }

} // R