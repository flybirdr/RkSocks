//
// Created by 张开海 on 2023/11/2.
//

#include "RingBuffer.h"

#include <algorithm>
#include <memory>

namespace R {
    RingBuffer::RingBuffer(int capacity)
            : mBuffer(new char[capacity]),
              mCapacity(capacity),
              mAvailable(capacity),
              mHead(capacity / 2),
              mTail(capacity / 2) {}

    int RingBuffer::read(char *buffer, int length) {
        if (buffer == nullptr) {
            return 0;
        }
        int readMax = std::min(mCapacity - mAvailable, length);
        if (readMax == 0) {
            return 0;
        }
        if (mHead + readMax >= mCapacity) {
            int first = mCapacity - mHead;
            memcpy(buffer, mBuffer + mHead, first);
            memcpy(buffer + first, mBuffer, readMax - first);
            mHead = readMax - first;
        } else {
            memcpy(buffer, mBuffer + mHead, readMax);
            mHead += readMax;
        }
        mAvailable += readMax;

        return readMax;
    }

    int RingBuffer::write(const char *buffer, int length) {
        if (buffer == nullptr) {
            return 0;
        }
        int writeMax = std::min(length, mAvailable);
        if (writeMax == 0) {
            return 0;
        }
        if (mTail + writeMax >= mCapacity) {
            int first = mCapacity - mTail;
            memcpy(mBuffer + mTail, buffer, first);
            memcpy(mBuffer, buffer + first, writeMax - first);
            mTail = writeMax - first;
        } else {
            memcpy(mBuffer + mTail, buffer, writeMax);
            mTail += writeMax;
        }
        mAvailable -= writeMax;
        return writeMax;
    }

    int RingBuffer::length() const { return mCapacity - mAvailable; }

    int RingBuffer::capacity() const { return mCapacity; }

    int RingBuffer::available() const { return mAvailable; }

    RingBuffer::~RingBuffer() { delete[] mBuffer; }

    bool RingBuffer::empty() const { return mAvailable == mCapacity; }

    uint8_t RingBuffer::read1() {
        uint8_t b;
        int c = read(reinterpret_cast<char *>(&b), 1);
        return c < 0 ? c : b;
    }

    uint16_t RingBuffer::read2() {
        uint16_t b;
        int c = read(reinterpret_cast<char *>(&b), 2);
        return c < 0 ? c : b;
    }

    uint32_t RingBuffer::read4() {
        uint32_t b;
        int c = read(reinterpret_cast<char *>(&b), 4);
        return c < 0 ? c : b;
    }

    uint64_t RingBuffer::read64() {
        uint64_t b;
        int c = read(reinterpret_cast<char *>(&b), 8);
        return c < 0 ? c : b;
    }

    int RingBuffer::writeTo(RingBuffer *dstBuffer) {
        if (dstBuffer == nullptr) {
            return 0;
        }
        int writeSize = std::min(dstBuffer->mAvailable, length());
        if (writeSize == 0) {
            return 0;
        }
        if (mHead + writeSize >= mCapacity) {
            int first = mCapacity - mHead;
            dstBuffer->write(mBuffer + mHead, first);
            dstBuffer->write(mBuffer, writeSize - first);
            mHead = writeSize - first;
        } else {
            dstBuffer->write(mBuffer + mHead, writeSize);
            mHead += writeSize;
        }
        mAvailable += writeSize;
        return writeSize;
    }

    int RingBuffer::readFrom(RingBuffer *buffer) {
        return readFrom(buffer, buffer->length());
    }

    int RingBuffer::readFrom(RingBuffer *buffer, int size) {
        if (buffer == nullptr) {
            return 0;
        }
        int readSize = std::min(size, std::min(buffer->length(), mAvailable));
        if (readSize == 0) {
            return 0;
        }
        if (mTail + readSize >= mCapacity) {
            int first = mCapacity - mTail;
            buffer->read(mBuffer + mTail, first);
            buffer->read(mBuffer, readSize - first);
            mTail = readSize - first;
        } else {
            buffer->read(mBuffer + mTail, readSize);
            mTail += readSize;
        }
        mAvailable -= readSize;
        return readSize;
    }

    bool RingBuffer::notEmpty() const {
        return length();
    }

    void RingBuffer::unread(int n) {
        if (length() == 0 || available() == 0) {
            return;
        }
        int max = std::min(available(), n);
        mHead -= max;
        mAvailable -= max;
    }

    uint8_t RingBuffer::get1(int i) {
        if (length() <= i) {
            return -1;
        }
        return mBuffer[mHead + i];

    }

}  // namespace R