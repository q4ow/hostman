#include "encryption.h"
#include "logging.h"
#include "utils.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <openssl/conf.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>

// This isnt all that secure but i honestly dont care, its an API key stored in your ~

#define KEY_SIZE 32
#define IV_SIZE 16

static unsigned char encryption_key[KEY_SIZE];
static int encryption_initialized = 0;

static char *base64_encode(const unsigned char *input, int length)
{
    BIO *bio = BIO_new(BIO_s_mem());
    BIO *b64 = BIO_new(BIO_f_base64());
    BIO_push(b64, bio);
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(b64, input, length);
    BIO_flush(b64);

    BUF_MEM *bufferPtr;
    BIO_get_mem_ptr(b64, &bufferPtr);

    char *result = malloc(bufferPtr->length + 1);
    if (result)
    {
        memcpy(result, bufferPtr->data, bufferPtr->length);
        result[bufferPtr->length] = 0;
    }

    BIO_free_all(b64);
    return result;
}

static unsigned char *base64_decode(const char *input, int *output_length)
{
    BIO *bio = BIO_new_mem_buf(input, -1);
    BIO *b64 = BIO_new(BIO_f_base64());
    BIO_push(b64, bio);
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);

    int length = strlen(input);
    unsigned char *buffer = malloc(length);
    if (!buffer)
    {
        BIO_free_all(b64);
        return NULL;
    }

    *output_length = BIO_read(b64, buffer, length);
    BIO_free_all(b64);

    return buffer;
}

static int generate_key_from_file(void)
{
    const char *home = getenv("HOME");
    if (home)
    {
        unsigned char seed[64];
        memset(seed, 0, sizeof(seed));
        snprintf((char *)seed, sizeof(seed), "hostman-fixed-salt-%s", home);

        EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
        if (mdctx)
        {
            EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL);
            EVP_DigestUpdate(mdctx, seed, strlen((char *)seed));
            EVP_DigestFinal_ex(mdctx, encryption_key, NULL);
            EVP_MD_CTX_free(mdctx);
            return 1;
        }
    }

    memset(encryption_key, 0x42, KEY_SIZE);
    return 1;
}

bool encryption_init(void)
{
    if (encryption_initialized)
    {
        return true;
    }

    ERR_load_crypto_strings();
    OpenSSL_add_all_algorithms();

    if (!generate_key_from_file())
    {
        log_error("Failed to generate encryption key");
        return false;
    }

    encryption_initialized = 1;
    return true;
}

char *encryption_encrypt_api_key(const char *api_key)
{
    if (!api_key || !encryption_initialized)
    {
        if (!encryption_init())
        {
            return NULL;
        }
    }

    unsigned char iv[IV_SIZE];
    if (RAND_bytes(iv, IV_SIZE) != 1)
    {
        log_error("Failed to generate random IV");
        return NULL;
    }

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
    {
        log_error("Failed to create cipher context");
        return NULL;
    }

    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, encryption_key, iv) != 1)
    {
        log_error("Failed to initialize encryption");
        EVP_CIPHER_CTX_free(ctx);
        return NULL;
    }

    int len;
    int ciphertext_len;
    unsigned char *ciphertext = malloc(strlen(api_key) + EVP_MAX_BLOCK_LENGTH);
    if (!ciphertext)
    {
        log_error("Failed to allocate memory for ciphertext");
        EVP_CIPHER_CTX_free(ctx);
        return NULL;
    }

    if (EVP_EncryptUpdate(ctx, ciphertext, &len, (unsigned char *)api_key, strlen(api_key)) != 1)
    {
        log_error("Failed to encrypt");
        EVP_CIPHER_CTX_free(ctx);
        free(ciphertext);
        return NULL;
    }
    ciphertext_len = len;

    if (EVP_EncryptFinal_ex(ctx, ciphertext + len, &len) != 1)
    {
        log_error("Failed to finalize encryption");
        EVP_CIPHER_CTX_free(ctx);
        free(ciphertext);
        return NULL;
    }
    ciphertext_len += len;

    unsigned char *encrypted_data = malloc(IV_SIZE + ciphertext_len);
    if (!encrypted_data)
    {
        log_error("Failed to allocate memory for encrypted data");
        EVP_CIPHER_CTX_free(ctx);
        free(ciphertext);
        return NULL;
    }

    memcpy(encrypted_data, iv, IV_SIZE);
    memcpy(encrypted_data + IV_SIZE, ciphertext, ciphertext_len);

    char *encoded = base64_encode(encrypted_data, IV_SIZE + ciphertext_len);

    EVP_CIPHER_CTX_free(ctx);
    free(ciphertext);
    free(encrypted_data);

    return encoded;
}

char *encryption_decrypt_api_key(const char *encrypted_key)
{
    if (!encrypted_key || !encryption_initialized)
    {
        if (!encryption_init())
        {
            return NULL;
        }
    }

    int encrypted_len;
    unsigned char *encrypted_data = base64_decode(encrypted_key, &encrypted_len);
    if (!encrypted_data || encrypted_len < IV_SIZE)
    {
        log_error("Invalid encrypted data");
        free(encrypted_data);
        return NULL;
    }

    unsigned char *iv = encrypted_data;
    unsigned char *ciphertext = encrypted_data + IV_SIZE;
    int ciphertext_len = encrypted_len - IV_SIZE;

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
    {
        log_error("Failed to create cipher context");
        free(encrypted_data);
        return NULL;
    }

    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, encryption_key, iv) != 1)
    {
        log_error("Failed to initialize decryption");
        EVP_CIPHER_CTX_free(ctx);
        free(encrypted_data);
        return NULL;
    }

    int len;
    unsigned char *plaintext = malloc(ciphertext_len + 1);
    if (!plaintext)
    {
        log_error("Failed to allocate memory for plaintext");
        EVP_CIPHER_CTX_free(ctx);
        free(encrypted_data);
        return NULL;
    }

    if (EVP_DecryptUpdate(ctx, plaintext, &len, ciphertext, ciphertext_len) != 1)
    {
        log_error("Failed to decrypt");
        EVP_CIPHER_CTX_free(ctx);
        free(encrypted_data);
        free(plaintext);
        return NULL;
    }
    int plaintext_len = len;

    if (EVP_DecryptFinal_ex(ctx, plaintext + len, &len) != 1)
    {
        log_error("Failed to finalize decryption");
        EVP_CIPHER_CTX_free(ctx);
        free(encrypted_data);
        free(plaintext);
        return NULL;
    }
    plaintext_len += len;

    plaintext[plaintext_len] = '\0';

    EVP_CIPHER_CTX_free(ctx);
    free(encrypted_data);

    return (char *)plaintext;
}

void encryption_cleanup(void)
{
    if (encryption_initialized)
    {
        EVP_cleanup();
        ERR_free_strings();
        encryption_initialized = 0;
    }
}
