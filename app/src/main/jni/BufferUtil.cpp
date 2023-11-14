//
// Created by 张开海 on 2023/11/12.
//

#include "BufferUtil.h"
#include "Buffer.h"

namespace R {

    void BufferUtil::incBufferTail(Buffer *buffer, int inc) {
        buffer->mTail += inc;
        buffer->mAvailable -= inc;
    }
} // R