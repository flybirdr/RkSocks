//
// Created by 张开海 on 2023/11/12.
//

#ifndef R_BUFFERUTIL_H
#define R_BUFFERUTIL_H


namespace R {

    class Buffer;

    class BufferUtil {
    public:
        static void incBufferTail(Buffer *buffer, int inc);
    };

} // R

#endif //R_BUFFERUTIL_H
