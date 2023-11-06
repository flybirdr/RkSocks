//
// Created by 张开海 on 2023/10/29.
//

#ifndef R_SCOPEJNIENV_H
#define R_SCOPEJNIENV_H

#include <jni.h>

extern JavaVM *gJvm;

class ScopeJNIEnv {

public:
    ScopeJNIEnv() : env(nullptr), attached(false) {
        if (gJvm->GetEnv((void **) &env, JNI_VERSION_1_6) == JNI_EDETACHED) {
            attached = false;
            JavaVMAttachArgs args = {JNI_VERSION_1_6, __FUNCTION__, __null};
            if (gJvm->AttachCurrentThread(&env, &args) < 0) {
                env = nullptr;
            }
        } else {
            attached = true;
        }
    }

    JNIEnv *operator->() {
        return env;
    }

    ~ScopeJNIEnv() {
        if (!attached) {
            gJvm->DetachCurrentThread();
        }
    }

private:
    JNIEnv *env;
    bool attached;
};


#endif //R_SCOPEJNIENV_H
