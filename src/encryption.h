#ifndef HOSTMAN_ENCRYPTION_H
#define HOSTMAN_ENCRYPTION_H

#include <stdbool.h>

char *
encryption_encrypt_api_key(const char *api_key);
char *
encryption_decrypt_api_key(const char *encrypted_key);
bool
encryption_init(void);
void
encryption_cleanup(void);

#endif