//
// Created by 张开海 on 2023/11/12.
//

#ifndef R_CRYPTO_H
#define R_CRYPTO_H



#include <mbedtls/md.h>
#include <mbedtls/cipher.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>

#include <unistd.h>
#include <memory>
#include "Logger.h"

typedef mbedtls_cipher_info_t cipher_info_t;
typedef mbedtls_cipher_context_t cipher_context_t;
typedef mbedtls_md_info_t md_info_t;

#define MAX_KEY_LENGTH 64
#define MAX_NONCE_LENGTH 32
#define MAX_MD_SIZE MBEDTLS_MD_MAX_SIZE
/* we must have MBEDTLS_CIPHER_MODE_CFB defined */
#if !defined(MBEDTLS_CIPHER_MODE_CFB)
#error Cipher Feedback mode a.k.a CFB not supported by your mbed TLS.
#endif
#ifndef MBEDTLS_GCM_C
#error No GCM support detected
#endif
#ifdef crypto_aead_xchacha20poly1305_ietf_ABYTES
#define FS_HAVE_XCHACHA20IETF
#endif
#ifdef MBEDTLS_PSA_CRYPTO_C
#undef MBEDTLS_PSA_CRYPTO_C
#endif
#ifdef MBEDTLS_USE_PSA_CRYPTO
#undef MBEDTLS_USE_PSA_CRYPTO
#endif


#define SUBKEY_INFO "ss-subkey"
#define IV_INFO "ss-iv"

namespace R {

    class Crypto {
    private:
        static int hkdfExtract(const mbedtls_md_info_t *md,
                               const uint8_t *salt, int saltLen,
                               const uint8_t *ikm, int ikmLen,
                               uint8_t *prk) {
            int hashLen;
            uint8_t null_salt[MBEDTLS_MD_MAX_SIZE]{0};
            if (saltLen < 0) {
                return -1;
            }
            hashLen = mbedtls_md_get_size(md);
            if (salt == nullptr) {
                salt = null_salt;
                saltLen = hashLen;
            }
            return mbedtls_md_hmac(md, salt, saltLen, ikm, ikmLen, prk);
        };

        static int hkdfExpand(const mbedtls_md_info_t *md,
                              const uint8_t *prk, int prkLen,
                              const uint8_t *info, int infoLen,
                              uint8_t *okm, int okmLen) {
            int hashLen;
            int N;
            int T_len = 0, where = 0, i, ret;
            mbedtls_md_context_t ctx;
            uint8_t T[MBEDTLS_MD_MAX_SIZE]{0};
            if (infoLen < 0 || okmLen < 0 || okm == nullptr) {
                return -1;
            }
            hashLen = mbedtls_md_get_size(md);
            if (prkLen < hashLen) {
                return -1;
            }
            if (info == nullptr) {
                info = reinterpret_cast<const uint8_t *>("");
            }
            N = okmLen / hashLen;
            if ((okmLen % hashLen) != 0) {
                N++;
            }
            if (N > 255) {
                return -1;
            }

            mbedtls_md_init(&ctx);
            if (0 != (ret = mbedtls_md_setup(&ctx, md, 1))) {
                mbedtls_md_free(&ctx);
                return -1;
            }

            for (int i = 0; i <= N; ++i) {
                uint8_t c = i;
                ret = mbedtls_md_hmac_starts(&ctx, prk, prkLen) ||
                      mbedtls_md_hmac_update(&ctx, T, T_len) ||
                      mbedtls_md_hmac_update(&ctx, info, infoLen) ||
                      mbedtls_md_hmac_update(&ctx, &c, 1) ||
                      mbedtls_md_hmac_finish(&ctx, T);
                if (ret != 0) {
                    mbedtls_md_free(&ctx);
                    return ret;
                }
                memcpy(okm + where, T, (i != N) ? hashLen : (okmLen - where));
                where += hashLen;
                T_len = hashLen;
            }
            return hashLen;
        };
    public:
        static int hkdf(const mbedtls_md_info_t *md,
                        const uint8_t *salt, int saltLen,
                        const uint8_t *ikm, int ikmLen,
                        const uint8_t *info, int infoLen,
                        uint8_t *okm, int okmLen) {
            uint8_t prk[MBEDTLS_MD_MAX_SIZE];
            return hkdfExtract(md, salt, saltLen, ikm, ikmLen, prk) ||
                   hkdfExpand(md, prk, mbedtls_md_get_size(md), info, infoLen, okm, okmLen);
        };

        static int keyFromPassword(const uint8_t *passwd, int len, uint8_t *key, int keyLen) {
            size_t datal = len;
            const md_info_t *mdInfo = mbedtls_md_info_from_string("MD5");
            if (!mdInfo) {
                LOGE("MD5 Digest not found!");
                return 0;
            }
            mbedtls_md_context_t mdContext{0};
            unsigned char mdBuf[MAX_MD_SIZE];
            int addmd;
            unsigned int i, j, mds;

            mds = mbedtls_md_get_size(mdInfo);
            if (passwd == nullptr || datal == 0) {
                LOGE("Empty password!");
                return 0;
            }
            if (mbedtls_md_setup(&mdContext, mdInfo, 1)) {
                LOGE("setup mbedtls md failed!");
                return 0;
            }

            for (int i = 0, addmd = 0; i < keyLen; ++addmd) {
                mbedtls_md_starts(&mdContext);
                if (addmd) {
                    mbedtls_md_update(&mdContext, mdBuf, mds);
                }
                mbedtls_md_update(&mdContext, passwd, mds);
                mbedtls_md_finish(&mdContext, mdBuf);

                for (int j = 0; j < mds; ++j, ++i) {
                    if (i >= keyLen) {
                        break;
                    }
                    key[i] = mdBuf[j];
                }
            }
            mbedtls_md_free(&mdContext);

            return keyLen;
        }

        static void mbedtlsRandom(unsigned char *buffer, int length) {
            mbedtls_entropy_context ctx;
            mbedtls_ctr_drbg_context drbg;
            int ret;
            const char *custom = __FILE_NAME__;
            mbedtls_entropy_init(&ctx);
            mbedtls_ctr_drbg_init(&drbg);
            ret = mbedtls_ctr_drbg_seed(&drbg, mbedtls_entropy_func, &ctx,
                                        reinterpret_cast<const unsigned char *>(custom),
                                        strlen(custom));
            if (ret) {
                goto cleanup;
            }
            ret = mbedtls_ctr_drbg_random(&drbg, reinterpret_cast<unsigned char *>(buffer), length);
            if (ret) {
                goto cleanup;
            }
            cleanup:
            mbedtls_entropy_free(&ctx);
            mbedtls_ctr_drbg_free(&drbg);
        }


        static void
        sodium_increment(unsigned char *n, const size_t nlen) {
            size_t i = 0U;
            uint_fast16_t c = 1U;

#ifdef HAVE_AMD64_ASM
            uint64_t t64, t64_2;
    uint32_t t32;

    if (nlen == 12U) {
        __asm__ __volatile__(
            "xorq %[t64], %[t64] \n"
            "xorl %[t32], %[t32] \n"
            "stc \n"
            "adcq %[t64], (%[out]) \n"
            "adcl %[t32], 8(%[out]) \n"
            : [t64] "=&r"(t64), [t32] "=&r"(t32)
            : [out] "D"(n)
            : "memory", "flags", "cc");
        return;
    } else if (nlen == 24U) {
        __asm__ __volatile__(
            "movq $1, %[t64] \n"
            "xorq %[t64_2], %[t64_2] \n"
            "addq %[t64], (%[out]) \n"
            "adcq %[t64_2], 8(%[out]) \n"
            "adcq %[t64_2], 16(%[out]) \n"
            : [t64] "=&r"(t64), [t64_2] "=&r"(t64_2)
            : [out] "D"(n)
            : "memory", "flags", "cc");
        return;
    } else if (nlen == 8U) {
        __asm__ __volatile__("incq (%[out]) \n"
                             :
                             : [out] "D"(n)
                             : "memory", "flags", "cc");
        return;
    }
#endif
            for (; i < nlen; i++) {
                c += (uint_fast16_t) n[i];
                n[i] = (unsigned char) c;
                c >>= 8;
            }
        }

    };


} // R

#endif //R_CRYPTO_H
