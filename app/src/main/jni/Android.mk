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

########################################################
## mbed TLS
########################################################

include $(CLEAR_VARS)
LOCAL_PATH := $(CUR_PATH)
LOCAL_MODULE := mbedtls

LOCAL_C_INCLUDES := $(LOCAL_PATH)/mbedtls/include

MBEDTLS_PSA :=	$(wildcard $(LOCAL_PATH)/mbedtls/library/psa*.c)
MBEDTLS_SOURCES_ := $(wildcard $(LOCAL_PATH)/mbedtls/library/*.c)
MBEDTLS_SOURCES := $(filter-out $(MBEDTLS_PSA),$(MBEDTLS_SOURCES_))

LOCAL_SRC_FILES := $(MBEDTLS_SOURCES:$(LOCAL_PATH)/%=%)

include $(BUILD_STATIC_LIBRARY)


########################################################
## libsodium
########################################################

include $(CLEAR_VARS)
LOCAL_PATH := $(CUR_PATH)
SODIUM_SOURCE := \
	crypto_aead/aes256gcm/aesni/aead_aes256gcm_aesni.c \
	crypto_aead/chacha20poly1305/aead_chacha20poly1305.c \
	crypto_aead/xchacha20poly1305/aead_xchacha20poly1305.c \
	crypto_core/ed25519/ref10/ed25519_ref10.c \
	crypto_core/hchacha20/core_hchacha20.c \
	crypto_core/salsa/ref/core_salsa_ref.c \
	crypto_generichash/blake2b/ref/blake2b-compress-ref.c \
	crypto_generichash/blake2b/ref/blake2b-ref.c \
	crypto_generichash/blake2b/ref/generichash_blake2b.c \
	crypto_onetimeauth/poly1305/onetimeauth_poly1305.c \
	crypto_onetimeauth/poly1305/donna/poly1305_donna.c \
	crypto_pwhash/crypto_pwhash.c \
	crypto_pwhash/argon2/argon2-core.c \
	crypto_pwhash/argon2/argon2.c \
	crypto_pwhash/argon2/argon2-encoding.c \
	crypto_pwhash/argon2/argon2-fill-block-ref.c \
	crypto_pwhash/argon2/blake2b-long.c \
	crypto_pwhash/argon2/pwhash_argon2i.c \
	crypto_scalarmult/curve25519/scalarmult_curve25519.c \
	crypto_scalarmult/curve25519/ref10/x25519_ref10.c \
	crypto_stream/chacha20/stream_chacha20.c \
	crypto_stream/chacha20/ref/chacha20_ref.c \
	crypto_stream/salsa20/stream_salsa20.c \
	crypto_stream/salsa20/ref/salsa20_ref.c \
	crypto_verify/verify.c \
	randombytes/randombytes.c \
	randombytes/sysrandom/randombytes_sysrandom.c \
	sodium/core.c \
	sodium/runtime.c \
	sodium/utils.c \
	sodium/version.c

LOCAL_MODULE := sodium
LOCAL_CFLAGS +=  \
				-DPACKAGE_NAME=\"libsodium\" -DPACKAGE_TARNAME=\"libsodium\" \
				-DPACKAGE_VERSION=\"1.0.15\" -DPACKAGE_STRING=\"libsodium-1.0.15\" \
				-DPACKAGE_BUGREPORT=\"https://github.com/jedisct1/libsodium/issues\" \
				-DPACKAGE_URL=\"https://github.com/jedisct1/libsodium\" \
				-DPACKAGE=\"libsodium\" -DVERSION=\"1.0.15\" \
				-DHAVE_PTHREAD=1                  \
				-DSTDC_HEADERS=1                  \
				-DHAVE_SYS_TYPES_H=1              \
				-DHAVE_SYS_STAT_H=1               \
				-DHAVE_STDLIB_H=1                 \
				-DHAVE_STRING_H=1                 \
				-DHAVE_MEMORY_H=1                 \
				-DHAVE_STRINGS_H=1                \
				-DHAVE_INTTYPES_H=1               \
				-DHAVE_STDINT_H=1                 \
				-DHAVE_UNISTD_H=1                 \
				-D__EXTENSIONS__=1                \
				-D_ALL_SOURCE=1                   \
				-D_GNU_SOURCE=1                   \
				-D_POSIX_PTHREAD_SEMANTICS=1      \
				-D_TANDEM_SOURCE=1                \
				-DHAVE_DLFCN_H=1                  \
				-DLT_OBJDIR=\".libs/\"            \
				-DHAVE_SYS_MMAN_H=1               \
				-DNATIVE_LITTLE_ENDIAN=1          \
				-DASM_HIDE_SYMBOL=.hidden         \
				-DHAVE_WEAK_SYMBOLS=1             \
				-DHAVE_ATOMIC_OPS=1               \
				-DHAVE_ARC4RANDOM=1               \
				-DHAVE_ARC4RANDOM_BUF=1           \
				-DHAVE_MMAP=1                     \
				-DHAVE_MLOCK=1                    \
				-DHAVE_MADVISE=1                  \
				-DHAVE_MPROTECT=1                 \
				-DHAVE_NANOSLEEP=1                \
				-DHAVE_POSIX_MEMALIGN=1           \
				-DHAVE_GETPID=1                   \
				-DCONFIGURED=1

LOCAL_C_INCLUDES += $(LOCAL_PATH)/libsodium/src/libsodium/include \
                    $(LOCAL_PATH)/libsodium/src/libsodium/include/sodium

LOCAL_SRC_FILES := $(addprefix libsodium/src/libsodium/,$(SODIUM_SOURCE))

include $(BUILD_STATIC_LIBRARY)

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
				$(LOCAL_PATH)/Buffer.cpp \
				$(LOCAL_PATH)/Tunnel.cpp \
				$(LOCAL_PATH)/Socks5Tunnel.cpp \
				$(LOCAL_PATH)/Socks5Server.cpp \
				$(LOCAL_PATH)/Socks5UdpTunnel.cpp \
				$(LOCAL_PATH)/TCPTunnel.cpp \
				$(LOCAL_PATH)/UDPTunnel.cpp \
				$(LOCAL_PATH)/AeadCrypto.cpp \
				$(LOCAL_PATH)/ProxyConfig.cpp \
				$(LOCAL_PATH)/ShadowSocksHandler.cpp \
				$(LOCAL_PATH)/Crypto.cpp \
				$(LOCAL_PATH)/BufferUtil.cpp

#LOCAL_SRC_FILES := $(addprefix $(LOCAL_PATH)/, $(SRC_FILES))
LOCAL_SRC_FILES := $(MY_SRC_FILES)
LOCAL_C_INCLUDES += $(LOCAL_PATH)/ \
					$(LOCAL_PATH)/hev-socks5-tunnel/src/ \
					$(LOCAL_PATH)/hev-socks5-tunnel/third-part/yaml/include \
LOCAL_C_INCLUDES += $(LOCAL_PATH)/mbedtls/include
LOCAL_CFLAGS += -DFD_SET_DEFINED -DSOCKLEN_T_DEFINED $(VERSION_CFLAGS)
LOCAL_CXXFLAGS += -DHAVE_PTHREADS -DFD_SET_DEFINED -DSOCKLEN_T_DEFINED $(VERSION_CFLAGS)
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
LOCAL_STATIC_LIBRARIES := libmbedtls
include $(BUILD_SHARED_LIBRARY)