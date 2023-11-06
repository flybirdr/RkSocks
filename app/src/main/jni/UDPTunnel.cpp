//
// Created by 张开海 on 2023/11/4.
//

#include <arpa/inet.h>
#include "UDPTunnel.h"
#include "Logger.h"
#include "Util.h"
#include <sstream>

namespace R {

    UDPTunnel::UDPTunnel(int bufferLen, std::shared_ptr<EventLoop> &loop) :
            Tunnel(bufferLen, loop) {

    }

    void UDPTunnel::handleRead(TunnelContext *context) {
        if (context == nullptr) {
            return;
        }
        int fd = context->fd;
        if (fd == 0) {
            LOGE("attempt to read a closed fd!");
            return;
        }
        auto buffer = context->inBuffer;
        auto addr = std::string{inet_ntoa(context->sockAddr.sin_addr)};
        auto port = ntohs(context->sockAddr.sin_port);
        std::string logPrefix = std::string("UDP ") +
                                "read " + addr + ":" +
                                std::to_string(port) +
                                " on fd=" +
                                std::to_string(fd) +
                                ", ";
        int nread = 0;
        socklen_t sockLen = sizeof(context->sockAddr);
        for (; buffer->available();) {
            int len = std::min(buffer->available(), mBufferLen);
            int c = recvfrom(fd, mBuffer, len, 0, (sockaddr *) &context->sockAddr, &sockLen);
            if (c > 0) {
                nread += c;
                buffer->write(mBuffer, c);
                if (context == inbound) {
                    LOGE("udp data=%s", Util::toHexString(mBuffer, c).c_str());
                }
            } else {
                if (c == -1) {
                    if (errno == EAGAIN) {
                        LOGI(TOCHAR(logPrefix + "try later"), 0);
                        break;
                    } else if (errno == EINTR) {
                        LOGI(TOCHAR(logPrefix + "interrupt by system call!"), 0);
                        continue;
                    } else {
                        LOGI(TOCHAR(logPrefix + "read error=%d:%s"), errno, strerror(errno), 0);
                        handleClosed(context);
                        break;
                    }
                }
                    //以下错误处理完直接返回
                    //eof
                else if (c == 0) {
                    LOGI(TOCHAR(logPrefix + "read nothing,errno=%d:%s"), errno, strerror(errno), 0);
                    handleClosed(context);
                }
                    //error
                else {
                    LOGI(TOCHAR(logPrefix + "read error=%d:%s"), errno, strerror(errno), 0);
                    handleClosed(context);
                }
                return;
            }
        }
        LOGI(TOCHAR(logPrefix + "data size=%d,current buffer size=%d"), nread, buffer->length(), 0);

        registerTunnelEvent();
    }

    void UDPTunnel::handleWrite(TunnelContext *context) {
        if (context == nullptr) {
            return;
        }
        int fd = context->fd;
        if (fd == 0) {
            LOGE("attempt to write closed fd!");
            return;
        }
        auto buffer = context->outBuffer;
        auto addr = std::string{inet_ntoa(context->sockAddr.sin_addr)};
        auto port = ntohs(context->sockAddr.sin_port);
        std::string logPrefix =
                std::string("UDP ") +
                "write " + addr +
                ":" +
                std::to_string(port) +
                " on fd=" +
                std::to_string(fd) +
                ", ";
        //握手没完成之前只和本机交互
        if (context == inbound &&
            context->tunnelStage < TunnelStage::STAGE_TRANSFER) {
            handleInboundHandshake();
        } else if (context->tunnelStage == TunnelStage::STAGE_TRANSFER) {
            //出站数据需要协议打包
            if (context == outbound) {
                packData();
            }
                //入站数据需要协议解包
            else if (context == inbound) {
                unpackData();
            }
        }
        LOGI(TOCHAR(logPrefix + "data=%d"), buffer->length(), 0);
        socklen_t sockLen = sizeof(context->sockAddr);
        while (buffer->notEmpty()) {
            int r = buffer->read(mBuffer, mBufferLen);
            int c = sendto(fd, mBuffer, r, 0, (sockaddr *) &context->sockAddr, sockLen);
            if (c > 0) {
                if (c < r) {
                    buffer->unread(r - c);
                }
            } else {
                if (c == -1) {
                    if (errno == EAGAIN) {
                        buffer->unread(r);
                        LOGI(TOCHAR(logPrefix + "write later!"), 0);
                        break;
                    } else if (errno == EINTR) {
                        buffer->unread(r);
                        LOGI(TOCHAR(logPrefix + "interrupt by system call!"), 0);
                        continue;
                    } else {
                        LOGI(TOCHAR(logPrefix + "closed, errno=%d"), errno, 0);
                        handleClosed(context);
                        break;
                    }
                } else {
                    LOGE(TOCHAR(logPrefix + "wirte failed,errno=%d:%s"), errno, strerror(errno), 0);
                    handleClosed(context);
                    break;
                }
            }
        }
        registerTunnelEvent();
    }

    void UDPTunnel::handleClosed(TunnelContext *context) {

        std::ostringstream os;
        os << "UDP ";
        os << "close tunnel ";
        if (inbound) {
            os << "inbound=" << inbound->fd;
            if (!mLooper->unregister(inbound->fd)) {
                LOGE("unable to unregister event!");
            }
        }
        os << ", ";
        if (outbound) {
            os << "outbound=" << outbound->fd;
            if (!mLooper->unregister(outbound->fd)) {
                LOGE("unable to unregister event!");
            }
        }
        LOGE("%s", os.str().c_str());
        if (inbound) {
            close(inbound->fd);
            inbound->fd = 0;
            delete inbound;
            inbound = nullptr;
        }
        if (outbound) {
            if (outbound->fd) {
                close(outbound->fd);
                outbound->fd = 0;
                delete outbound;
                outbound = nullptr;
            }
        }
    }

    void UDPTunnel::registerTunnelEvent() {
        bool canRead, canWrite;
        if (inbound) {
            if (inbound->tunnelStage == TunnelStage::STAGE_HANDSHAKE) {
                canRead = inbound->inBuffer->empty();
                canWrite = inbound->outBuffer->notEmpty() || inbound->inBuffer->notEmpty();
            } else {
                canRead = inbound->inBuffer->empty();
                canWrite = inbound->outBuffer->notEmpty() ||
                           (outbound && outbound->inBuffer->notEmpty());
            }
            if (canRead && canWrite) {
                mLooper->modOnReadWrite(inbound->fd, inbound);
            } else if (canRead) {
                mLooper->modOnRead(inbound->fd, inbound);
            } else if (canWrite) {
                mLooper->modOnWrite(inbound->fd, inbound);
            } else {
                mLooper->modNone(inbound->fd, inbound);
            }
        }
        if (outbound) {
            if (outbound->tunnelStage == TunnelStage::STAGE_HANDSHAKE) {
                canRead = outbound->inBuffer->empty();
                canWrite = outbound->outBuffer->notEmpty() || outbound->inBuffer->notEmpty();
            } else {
                canRead = outbound->inBuffer->empty();
                canWrite = outbound->outBuffer->notEmpty() || inbound->inBuffer->notEmpty();
            }
            if (canRead && canWrite) {
                mLooper->modOnReadWrite(outbound->fd, outbound);
            } else if (canRead) {
                mLooper->modOnRead(outbound->fd, outbound);
            } else if (canWrite) {
                mLooper->modOnWrite(outbound->fd, outbound);
            } else {
                mLooper->modNone(outbound->fd, outbound);
            }
        }
    }

} // R