//
// Created by 张开海 on 2023/11/3.
//

#include <arpa/inet.h>
#include "Tunnel.h"
#include "Logger.h"
#include "Util.h"
#include <sstream>

namespace R {
    Tunnel::Tunnel(int bufferLen, std::shared_ptr<EventLoop> &loop) :
            mBuffer(new char[bufferLen]),
            mBufferLen(bufferLen),
            mLooper(loop),
            inbound(nullptr),
            outbound(nullptr) {


    }

    void Tunnel::registerTunnelEvent() {
        bool canRead, canWrite;
        if (inbound) {
            if (inbound->tunnelStage == TunnelStage::STAGE_HANDSHAKE) {
                canRead = inbound->inBuffer->available();
                canWrite = inbound->outBuffer->notEmpty() || inbound->inBuffer->notEmpty();
            } else {
                canRead = inbound->inBuffer->available();
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
                canRead = outbound->inBuffer->available();
                canWrite = outbound->outBuffer->notEmpty() || outbound->inBuffer->notEmpty();
            } else {
                canRead = outbound->inBuffer->available();
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

    Tunnel::~Tunnel() {
        if (mBuffer) {
            delete[] mBuffer;
            mBuffer = nullptr;
        }
    }

} // R