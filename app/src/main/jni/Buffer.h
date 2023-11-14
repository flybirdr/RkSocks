//
// Created by 张开海 on 2023/11/2.
//

#ifndef R_BUFFER_H
#define R_BUFFER_H

#include <unistd.h>
#include <string>
#include "BufferUtil.h"

namespace R {

    class Buffer {
        friend class BufferUtil;

        char *mBuffer;
        int mCapacity;
        int mAvailable;
        int mHead;
        int mTail;
        bool mRing;
    public:
        Buffer(int capacity);

        Buffer(int capacity, int start, bool ring);

        virtual ~Buffer();

        char *getBuffer() {
            return mBuffer + mTail;
        };

        /**
         * 将本buffer写入dst
         * @return 实际写入的字节数
         */
        int writeTo(Buffer *dst);

        /**
         * 从srcBuffer中拷贝内容
         * @return 实际拷贝的字节数
         */
        int readFrom(Buffer *src);

        int readFrom(Buffer *src, int length);

        int write(const char *src, int length);

        int read(char *dst, int length);

        uint8_t read8();

        uint16_t read16();

        uint32_t read32();

        uint64_t read64();

        /**
         * 空闲字节数
         */
        int available() const;

        /**
         * 已使用数据长度
         */
        int length() const;

        /**
         * 容量
         */
        int capacity() const;

        bool empty() const;

        bool notEmpty() const;

        /**
         * 回滚
         */
        void unread(int n);

        uint8_t get(int i);

        void set(int i, uint8_t value);

        int write8(uint8_t data);

        int write16(uint16_t data);

        int write32(uint32_t data);

        int write64(uint64_t data);

        std::string toString();


    };

} // R

#endif //R_BUFFER_H
