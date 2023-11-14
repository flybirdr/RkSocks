//
// Created by 张开海 on 2023/11/2.
//

#include "Buffer.h"

#include <algorithm>
#include <memory>
#include <sstream>

namespace R {
    Buffer::Buffer(int capacity)
            : Buffer(capacity, 0, true) {

    }

    Buffer::Buffer(int capacity, int start, bool ring)
            : mBuffer(new char[capacity]),
              mCapacity(capacity),
              mAvailable(capacity),
              mHead(start),
              mTail(start),
              mRing(ring) {
        if (mHead < 0 || mHead >= mCapacity) {
            mHead = mTail = 0;
        }
    }

    int Buffer::read(char *dst, int length) {
        if (dst == nullptr) {
            return 0;
        }
        int readMax = std::min(mCapacity - mAvailable, length);
        if (readMax == 0) {
            return 0;
        }
        if (mHead + readMax >= mCapacity) {
            int first = mCapacity - mHead;
            memcpy(dst, mBuffer + mHead, first);
            memcpy(dst + first, mBuffer, readMax - first);
            if (mRing)
                mHead = readMax - first;
        } else {
            memcpy(dst, mBuffer + mHead, readMax);
            if (mRing)
                mHead += readMax;
        }
        mAvailable += readMax;

        return readMax;
    }

    int Buffer::write(const char *src, int length) {
        if (src == nullptr) {
            return 0;
        }
        int writeMax = std::min(length, mAvailable);
        if (writeMax == 0) {
            return 0;
        }
        if (mTail + writeMax >= mCapacity) {
            int first = mCapacity - mTail;
            memcpy(mBuffer + mTail, src, first);
            memcpy(mBuffer, src + first, writeMax - first);
            mTail = writeMax - first;
        } else {
            memcpy(mBuffer + mTail, src, writeMax);
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

    uint8_t Buffer::read8() {
        uint8_t b;
        int c = read(reinterpret_cast<char *>(&b), 1);
        return c < 0 ? c : b;
    }

    uint16_t Buffer::read16() {
        uint16_t b;
        int c = read(reinterpret_cast<char *>(&b), 2);
        return c < 0 ? c : b;
    }

    uint32_t Buffer::read32() {
        uint32_t b;
        int c = read(reinterpret_cast<char *>(&b), 4);
        return c < 0 ? c : b;
    }

    uint64_t Buffer::read64() {
        uint64_t b;
        int c = read(reinterpret_cast<char *>(&b), 8);
        return c < 0 ? c : b;
    }

    int Buffer::writeTo(Buffer *dst) {
        if (dst == nullptr) {
            return 0;
        }
        int writeSize = std::min(dst->mAvailable, length());
        if (writeSize == 0) {
            return 0;
        }
        if (mHead + writeSize >= mCapacity) {
            int first = mCapacity - mHead;
            dst->write(mBuffer + mHead, first);
            dst->write(mBuffer, writeSize - first);
            if (mRing)
                mHead = writeSize - first;
        } else {
            dst->write(mBuffer + mHead, writeSize);
            if (mRing)
                mHead += writeSize;
        }
        mAvailable += writeSize;
        return writeSize;
    }

    int Buffer::readFrom(Buffer *src) {
        return readFrom(src, src->length());
    }

    int Buffer::readFrom(Buffer *src, int length) {
        if (src == nullptr) {
            return 0;
        }
        int readSize = std::min(length, std::min(src->length(), mAvailable));
        if (readSize == 0) {
            return 0;
        }
        if (mTail + readSize >= mCapacity) {
            int first = mCapacity - mTail;
            src->read(mBuffer + mTail, first);
            src->read(mBuffer, readSize - first);
            mTail = readSize - first;
        } else {
            src->read(mBuffer + mTail, readSize);
            mTail += readSize;
        }
        mAvailable -= readSize;
        return readSize;
    }

    bool Buffer::notEmpty() const {
        return length();
    }

    void Buffer::unread(int n) {
        if (!mRing || length() == 0 || available() == 0) {
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

    int Buffer::write8(uint8_t data) {
        return write(reinterpret_cast<const char *>(&data), sizeof(data));
    }

    int Buffer::write16(uint16_t data) {
        return write(reinterpret_cast<const char *>(&data), sizeof(data));
    }

    int Buffer::write32(uint32_t data) {
        return write(reinterpret_cast<const char *>(&data), sizeof(data));
    }

    int Buffer::write64(uint64_t data) {
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