//
// Created by 张开海 on 2023/11/4.
//

#include "TCPTunnel.h"
#include "Logger.h"
#include "Util.h"
#include <string>
#include <arpa/inet.h>
#include <sstream>

namespace R {
    TCPTunnel::TCPTunnel(int bufferLen, std::shared_ptr<EventLoop> &loop) :
            Tunnel(bufferLen, loop) {

    }

    void TCPTunnel::handleRead(TunnelContext *context) {
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
        std::string logPrefix = std::string("TCP ") +
                                "read " + addr + ":" + std::to_string(port) + " on fd=" +
                                std::to_string(fd) + ", ";
        int nread = 0;
        for (; buffer->available();) {
            int len = std::min(buffer->available(), mBufferLen);
            int c = read(fd, mBuffer, len);
            if (c > 0) {
                nread += c;
                buffer->write(mBuffer, c);
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

    void TCPTunnel::handleWrite(TunnelContext *context) {
        if (context == nullptr) {
            return;
        }
        int fd = context->fd;
        if (fd == 0) {
            LOGE("attempt to write closed fd!");
            return;
        }
        auto tunnel = context->tunnel;
        auto buffer = context->outBuffer;
        auto addr = std::string{inet_ntoa(context->sockAddr.sin_addr)};
        auto port = ntohs(context->sockAddr.sin_port);
        std::string logPrefix = std::string("TCP ") +
                                "write " + addr +
                                ":" +
                                std::to_string(port) +
                                " on fd=" +
                                std::to_string(fd) +
                                ", ";
        //握手没完成之前只和本机交互
        if (context == context->tunnel->inbound &&
            context->tunnelStage < TunnelStage::STAGE_TRANSFER) {
            inboundHandleHandshake();
        } else if (context->tunnelStage == TunnelStage::STAGE_TRANSFER) {
            //出站数据需要协议打包
            if (context == tunnel->outbound) {
                packData();
            }
                //入站数据需要协议解包
            else if (context->tunnel->inbound) {
                unpackData();
            }
        }
        LOGI(TOCHAR(logPrefix + "data=%d"), buffer->length(), 0);
        int nwrite = 0;
        for (; buffer->notEmpty();) {
            int r = buffer->read(mBuffer, mBufferLen);
            int c = write(fd, mBuffer, r);
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

    void TCPTunnel::handleClosed(TunnelContext *context) {

        auto tunnel = context->tunnel;
        std::ostringstream os;
        os << "TCP ";
        os << "close tunnel ";
        if (tunnel->inbound) {
            os << "inbound=" << tunnel->inbound->fd;
            if (!mLooper->unregister(tunnel->inbound->fd)) {
                LOGE("unable to unregister event!");
            }
        }
        os << ", ";
        if (tunnel->outbound) {
            os << "outbound=" << tunnel->outbound->fd;
            if (!mLooper->unregister(tunnel->outbound->fd)) {
                LOGE("unable to unregister event!");
            }
        }
        LOGE("%s", os.str().c_str());
        //todo
//            {
//                std::unique_lock<std::mutex> lock(mTunnelLock);
//                mTunnels.remove(tunnel);
//            }
        if (tunnel->inbound) {
            close(tunnel->inbound->fd);
            tunnel->inbound->fd = 0;
            delete tunnel->inbound;
            tunnel->inbound = nullptr;
        }
        if (tunnel->outbound) {
            if (tunnel->outbound->fd) {
                close(tunnel->outbound->fd);
                tunnel->outbound->fd = 0;
                delete tunnel->outbound;
                tunnel->outbound = nullptr;
            }
        }
    }
} // R