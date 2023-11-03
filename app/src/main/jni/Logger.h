//
// Created by 张开海 on 2023/10/28.
//

#ifndef R_LOGGER_H
#define R_LOGGER_H

#include <android/log.h>


#define TAG "R"

//#define LOGV(...) __android_log_print(ANDROID_LOG_VERBOSE,TAG,__VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,TAG,__VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG,TAG,__VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR,TAG,__VA_ARGS__)

#define LOGERRNO(...) __android_log_print(ANDROID_LOG_ERROR,TAG,__VA_ARGS__)


#endif //R_LOGGER_H
