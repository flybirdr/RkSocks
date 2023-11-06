//
// Created by 张开海 on 2023/11/2.
//

#include "Buffer.h"

#include <algorithm>
#include <memory>
#include <sstream>

namespace R {
    Buffer::Buffer(int capacity)
            : mBuffer(new char[capacity]),
              mCapacity(capacity),
              mAvailable(capacity),
              mHead(capacity / 2),
              mTail(capacity / 2) {}

    int Buffer::read(char *buffer, int length) {
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

    int Buffer::write(const char *buffer, int length) {
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

    int Buffer::length() const { return mCapacity - mAvailable; }

    int Buffer::capacity() const { return mCapacity; }

    int Buffer::available() const { return mAvailable; }

    Buffer::~Buffer() { delete[] mBuffer; }

    bool Buffer::empty() const { return mAvailable == mCapacity; }

    uint8_t Buffer::read1() {
        uint8_t b;
        int c = read(reinterpret_cast<char *>(&b), 1);
        return c < 0 ? c : b;
    }

    uint16_t Buffer::read2() {
        uint16_t b;
        int c = read(reinterpret_cast<char *>(&b), 2);
        return c < 0 ? c : b;
    }

    uint32_t Buffer::read4() {
        uint32_t b;
        int c = read(reinterpret_cast<char *>(&b), 4);
        return c < 0 ? c : b;
    }

    uint64_t Buffer::read64() {
        uint64_t b;
        int c = read(reinterpret_cast<char *>(&b), 8);
        return c < 0 ? c : b;
    }

    int Buffer::writeTo(Buffer *dstBuffer) {
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

    int Buffer::readFrom(Buffer *buffer) {
        return readFrom(buffer, buffer->length());
    }

    int Buffer::readFrom(Buffer *buffer, int size) {
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

    bool Buffer::notEmpty() const {
        return length();
    }

    void Buffer::unread(int n) {
        if (length() == 0 || available() == 0) {
            return;
        }
        int max = std::min(available(), n);
        mHead -= max;
        mAvailable -= max;
    }

    uint8_t Buffer::get(int i) {
        if (length() <= i) {
            return -1;
        }
        int index = mHead + i;
        if (index >= mCapacity) {
            index -= mCapacity;
        }
        return mBuffer[index];
    }

    void Buffer::set(int i, uint8_t value) {
        if (i < 0 && i >= length()) {
            return;
        }
        int index = mHead + i;
        if (index >= mCapacity) {
            index -= mCapacity;
        }
        mBuffer[index] = value;
    }

    int Buffer::write1(uint8_t data) {
        return write(reinterpret_cast<const char *>(&data), sizeof(data));
    }

    int Buffer::write2(uint16_t data) {
        return write(reinterpret_cast<const char *>(&data), sizeof(data));
    }

    int Buffer::write4(uint32_t data) {
        return write(reinterpret_cast<const char *>(&data), sizeof(data));
    }

    int Buffer::write8(uint64_t data) {
        return write(reinterpret_cast<const char *>(&data), sizeof(data));
    }

    std::string Buffer::toString() {
        if (length() == 0) {
            return "";
        }
        static char tab[] = "0123456789ABCDEF";
        std::ostringstream os;
        if (mTail < mHead) {
            for (int i = mHead; i < mCapacity; ++i) {
                char c = mBuffer[i];
                os << tab[c & 0x0f] << tab[(c >> 4) & 0x0f];
            }
            for (int i = 0; i <= mTail; ++i) {
                char c = mBuffer[i];
                os << tab[c & 0x0f] << tab[(c >> 4) & 0x0f];
            }
        } else {
            for (int i = mHead; i <= mTail; ++i) {
                char c = mBuffer[i];
                os << tab[c & 0x0f] << tab[(c >> 4) & 0x0f];
            }
        }
        return os.str();
    }
}  // namespace R