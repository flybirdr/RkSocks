//
// Created by 张开海 on 2023/10/29.
//

#include "VpnUtil.h"
#include <jni.h>
#include "ScopeJNIEnv.h"

extern jobject vpnService;

void VpnUtil::protect(int fd) {
    ScopeJNIEnv env;
    auto clazz = env->FindClass("android/net/VpnService");
    auto method = env->GetMethodID(clazz, "protect", "(I)Z");
    env->CallBooleanMethod(vpnService, method, fd);
}
