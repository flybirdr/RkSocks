//
// Created by 张开海 on 2023/11/8.
//

#ifndef R_AEADCRYPTO_H
#define R_AEADCRYPTO_H

#include "Crypto.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <mbedtls/cipher.h>
#include <mbedtls/md.h>
#include "Logger.h"
#include "Buffer.h"

#ifdef MBEDTLS_PSA_CRYPTO_C
#undef MBEDTLS_PSA_CRYPTO_C
#endif
#ifdef MBEDTLS_USE_PSA_CRYPTO
#undef MBEDTLS_USE_PSA_CRYPTO
#endif
namespace R {

    enum SUPPORED_AEAD_CIPHERS {
        AES_128_GCM,
        AES_192_GCM,
        AES_256_GCM,
        CHACHA20_POLY1305,
        SUPPORTED_CIPHER_NUM
    };
    const char *SUPPORTED_CIPHER_NAMES[SUPPORTED_CIPHER_NUM] = {
            "aes-128-gcm",
            "aes-192-gcm",
            "aes-256-gcm",
            "chacha20-ietf-poly1305",
    };

    static const int SUPPORTED_CIPHERS_NONCE_SIZE_IN_BYTES[SUPPORTED_CIPHER_NUM] = {
            12, 12, 12, 12,

    };

    static const int SUPPORTED_CIPHERS_KEY_SIZE_IN_BYTES[SUPPORTED_CIPHER_NUM] = {
            16, 24, 32, 32,

    };

    static const int SUPPORTED_CIPHERS_TAG_SIZE_IN_BYTES[SUPPORTED_CIPHER_NUM] = {
            16, 16, 16, 16,
    };

    class AeadCrypto {
    private:
        std::string userPass;
        std::string userKey;
        std::string userMethod;
        const cipher_info_t *cipherInfo;
        cipher_context_t *cipherContext;

        //key from password
        uint8_t key[MAX_KEY_LENGTH];

        //used by udp
        uint8_t salt[MAX_KEY_LENGTH];
        //iv
        uint8_t nonce[MAX_NONCE_LENGTH];
        //crypto key
        uint8_t skey[MAX_KEY_LENGTH];

        int saltLen;
        int keyLen;
        int nonceLen;
        int tagLen;
        int method;

        bool isFirstTransfer;
    public:
        /**
         * 初始化加密算法，产生salt、nonce、key以及skey
         */
        void initialize() {
            int m = AES_128_GCM;
            if (!userMethod.empty()) {
                for (int i = 0; i < SUPPORTED_CIPHER_NUM; ++i) {
                    if (userMethod == SUPPORTED_CIPHER_NAMES[i]) {
                        m += i;
                        break;
                    }
                }
            }
            method = m;
            cipherContext = new cipher_context_t;
            cipherInfo = mbedtls_cipher_info_from_string(userMethod.c_str());

            mbedtls_cipher_init(cipherContext);
            if (mbedtls_cipher_setup(cipherContext, cipherInfo)) {
                LOGE("failed to init cipher!!");
            }

            keyLen = Crypto::keyFromPassword(reinterpret_cast<const uint8_t *>(userPass.c_str()),
                                             userPass.length(), key, keyLen);
            nonceLen = SUPPORTED_CIPHERS_NONCE_SIZE_IN_BYTES[method];
            tagLen = SUPPORTED_CIPHERS_TAG_SIZE_IN_BYTES[method];


            Crypto::mbedtlsRandom(&salt[0], keyLen);

            auto md = mbedtls_md_info_from_string("SHA1");
            Crypto::hkdf(md, salt, keyLen,
                         key, keyLen,
                         reinterpret_cast<const uint8_t *>(SUBKEY_INFO), strlen(SUBKEY_INFO),
                         skey, keyLen);

            memset(nonce, 0, nonceLen);
            mbedtls_cipher_setkey(cipherContext, skey, keyLen * 8, MBEDTLS_ENCRYPT);
            mbedtls_cipher_reset(cipherContext);
        }

        std::string encrypt(char *plainText, size_t len);

        std::string decrypt(char *cipherText, size_t len, Buffer *outBuffer);


        Buffer *encryptAll(char *plainText, size_t len);

        Buffer *decryptAll(char *cipherText, size_t len);
    };

} // R

#endif //R_AEADCRYPTO_H
