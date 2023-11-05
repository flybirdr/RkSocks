//
// Created by 张开海 on 2023/10/31.
//

#ifndef R_UTIL_H
#define R_UTIL_H

#include <string>
#include <fcntl.h>

#define TOCHAR(X) ((X).c_str())

class Util {
public:
    static void setNonBlocking(int fd) {
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags & O_NONBLOCK) {
            return;
        }
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }

    static std::string toHexString(char *buffer, int length) {
        const char tab[] = "0123456789ABCDEF";    // 0x0-0xF 的字符查找表
        std::string hexString;
        for (int i = 0; i < length; ++i) {
            (hexString += tab[buffer[i] >> 4]) += tab[buffer[i] & 0x0f];
        }
        return hexString;
    }

};


#endif //R_UTIL_H
