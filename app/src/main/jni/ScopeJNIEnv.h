//
// Created by 张开海 on 2023/10/29.
//

#ifndef R_SCOPEJNIENV_H
#define R_SCOPEJNIENV_H

#include <jni.h>

extern JavaVM *gJvm;

class ScopeJNIEnv {

public:
    ScopeJNIEnv() : env(0) {
        if (gJvm->GetEnv((void **) &env, JNI_VERSION_1_6) < 0) {
            JavaVMAttachArgs args = {JNI_VERSION_1_6, __FUNCTION__, __null};
            if (gJvm->AttachCurrentThread(&env, &args) < 0) {
                env = 0;
            }
        }
    }

    JNIEnv *operator->() {
        return env;
    }

    ~ScopeJNIEnv() {

    }

private:
    JNIEnv *env;
};


#endif //R_SCOPEJNIENV_H
