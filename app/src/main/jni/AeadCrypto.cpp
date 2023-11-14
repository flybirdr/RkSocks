//
// Created by 张开海 on 2023/11/8.
//

#include "AeadCrypto.h"
#include "Buffer.h"
#include <endian.h>
#include <mbedtls/cipher.h>
#include "BufferUtil.h"

#define CHUNK_SIZE_LEN          2
#define CHUNK_SIZE_MASK         0x3FFF
namespace R {
    std::string AeadCrypto::encrypt(char *plainText, size_t len) {
        size_t outLen = isFirstTransfer ? saltLen : 0 + len + tagLen + CHUNK_SIZE_LEN + tagLen;
        Buffer cipherText(outLen, 0, false);
        if (isFirstTransfer) {
            isFirstTransfer = false;
            cipherText.write(reinterpret_cast<const char *>(&salt[0]), saltLen);
        }
        uint8_t sizeBuffer[CHUNK_SIZE_LEN];
        uint16_t sizeNum = htons(len & CHUNK_SIZE_MASK);
        int clen = CHUNK_SIZE_LEN + tagLen;
        size_t writeSize = 0;
        char *cipherBuffer = cipherText.getBuffer();
        mbedtls_cipher_write_tag(cipherContext, nonce, tagLen);
        mbedtls_cipher_auth_encrypt_ext(cipherContext,
                                        nonce, nonceLen,
                                        nullptr, 0,
                                        sizeBuffer, clen,
                                        reinterpret_cast<unsigned char *>(cipherBuffer), clen,
                                        &writeSize, tagLen);
        BufferUtil::incBufferTail(&cipherText, clen);
        Crypto::sodium_increment(nonce, nonceLen);
        clen = len + tagLen;
        cipherBuffer = cipherText.getBuffer();
        mbedtls_cipher_write_tag(cipherContext, nonce, tagLen);
        mbedtls_cipher_auth_encrypt_ext(cipherContext,
                                        nonce, nonceLen,
                                        nullptr, 0,
                                        reinterpret_cast<const unsigned char *>(plainText), clen,
                                        reinterpret_cast<unsigned char *>(cipherBuffer), clen,
                                        &writeSize, tagLen);
        BufferUtil::incBufferTail(&cipherText, clen);
        Crypto::sodium_increment(nonce, nonceLen);

        return "";
    }

    std::string AeadCrypto::decrypt(char *cipherText, size_t len, Buffer *outBuffer) {
        if (isFirstTransfer) {
            isFirstTransfer = false;
            memcpy(salt, cipherText, saltLen);
            initialize();
        }
        int outLen = CHUNK_SIZE_MASK + tagLen + CHUNK_SIZE_LEN + tagLen;
        uint8_t c[outLen];
        size_t decryptSize = 0;
        int mlen;
        while (len > 0) {
            uint8_t lenBuffer[CHUNK_SIZE_LEN];
            mbedtls_cipher_auth_decrypt_ext(cipherContext,
                                            nonce, nonceLen,
                                            nullptr, 0,
                                            reinterpret_cast<const unsigned char *>(cipherText),
                                            CHUNK_SIZE_LEN + tagLen,
                                            lenBuffer, CHUNK_SIZE_LEN,
                                            &decryptSize, tagLen
            );
            mlen = ntohs(*((uint16_t *) c));
            if (mlen == 0 || mlen > CHUNK_SIZE_MASK) {
                return "";
            }
            size_t chunkLen = 2 * tagLen + CHUNK_SIZE_LEN + mlen;
            Crypto::sodium_increment(nonce, nonceLen);
            mbedtls_cipher_auth_decrypt_ext(cipherContext,
                                            nonce, nonceLen,
                                            nullptr, 0,
                                            reinterpret_cast<const unsigned char *>(cipherText) +
                                            CHUNK_SIZE_LEN + tagLen,
                                            mlen + tagLen,
                                            c, mlen + tagLen,
                                            &decryptSize, tagLen
            );
            Crypto::sodium_increment(nonce, nonceLen);
            len -= chunkLen;
            outBuffer->write(reinterpret_cast<const char *>(c), decryptSize);
        }

        return std::string();
    }

    Buffer *AeadCrypto::encryptAll(char *plainText, size_t len) {
        int outLen = saltLen + tagLen + len;
        auto *buffer = new Buffer(outLen, 0, false);
        int cipherTextLen = tagLen + len;
        buffer->write(reinterpret_cast<const char *>(salt), saltLen);
        size_t encryptSize;
        mbedtls_cipher_write_tag(cipherContext, nonce, nonceLen);
        mbedtls_cipher_auth_encrypt_ext(cipherContext,
                                        nonce, nonceLen,
                                        nullptr, 0,
                                        reinterpret_cast<const unsigned char *>(plainText), len,
                                        reinterpret_cast<unsigned char *>(buffer->getBuffer()),
                                        cipherTextLen,
                                        &encryptSize, tagLen);
        BufferUtil::incBufferTail(buffer, encryptSize);
        Crypto::sodium_increment(nonce, nonceLen);
        return buffer;
    }

    Buffer *AeadCrypto::decryptAll(char *cipherText, size_t len) {
        char *c = cipherText;
        memcpy(salt, c, saltLen);
        c += saltLen;
        int clen = len - saltLen;
        Buffer *buffer = new Buffer(clen - tagLen, 0, false);
        size_t decryptSize;
        mbedtls_cipher_auth_decrypt_ext(cipherContext, nonce, nonceLen,
                                        nullptr, 0,
                                        reinterpret_cast<const unsigned char *>(c), clen,
                                        reinterpret_cast<unsigned char *>(buffer->getBuffer()),
                                        buffer->available(),
                                        &decryptSize, tagLen);
        Crypto::sodium_increment(nonce, nonceLen);
        return buffer;
    }
} // R