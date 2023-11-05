# Copyright (C) 2021 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

CUR_PATH := $(call my-dir)
include $(CUR_PATH)/hev-socks5-tunnel/Android.mk

include $(CLEAR_VARS)
LOCAL_PATH := $(CUR_PATH)
LOCAL_MODULE    := r
MY_SRC_FILES := \
				$(LOCAL_PATH)/jni.cpp \
				$(LOCAL_PATH)/ScopeJNIEnv.cpp \
				$(LOCAL_PATH)/VpnUtil.cpp \
				$(LOCAL_PATH)/Socks5Config.cpp \
				$(LOCAL_PATH)/Util.cpp \
				$(LOCAL_PATH)/EventLoop.cpp \
				$(LOCAL_PATH)/RingBuffer.cpp \
				$(LOCAL_PATH)/Tunnel.cpp \
				$(LOCAL_PATH)/Socks5Tunnel.cpp \
				$(LOCAL_PATH)/Socks5Server.cpp \
				$(LOCAL_PATH)/Socks5UdpTunnel.cpp \
				$(LOCAL_PATH)/TCPTunnel.cpp \
				$(LOCAL_PATH)/UDPTunnel.cpp

#LOCAL_SRC_FILES := $(addprefix $(LOCAL_PATH)/, $(SRC_FILES))
LOCAL_SRC_FILES := $(MY_SRC_FILES)
LOCAL_C_INCLUDES := \
					$(LOCAL_PATH)/ \
					$(LOCAL_PATH)/hev-socks5-tunnel/src/ \
					$(LOCAL_PATH)/hev-socks5-tunnel/third-part/yaml/include \

LOCAL_CFLAGS += -DFD_SET_DEFINED -DSOCKLEN_T_DEFINED $(VERSION_CFLAGS)
LOCAL_CXXFLAGS := -DHAVE_PTHREADS -DFD_SET_DEFINED -DSOCKLEN_T_DEFINED $(VERSION_CFLAGS)
#ifeq ($(BUILD_TYPE),debug)
## 编译选项
#LOCAL_CFLAGS := -g -DDEBUG
## 调试标志
#LOCAL_LDFLAGS := -g
## 添加符号表
#LOCAL_EXPORT_CFLAGS += -g
## 添加调试信息
#LOCAL_EXPORT_LDFLAGS += -g
#endif
ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
LOCAL_CFLAGS += -mfpu=neon
endif
LOCAL_LDLIBS := -llog -landroid
LOCAL_SHARED_LIBRARIES := hev-socks5-tunnel
include $(BUILD_SHARED_LIBRARY)