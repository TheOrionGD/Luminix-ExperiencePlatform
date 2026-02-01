/**
 * Security Header File
 */

#ifndef SECURITY_H
#define SECURITY_H

#include <stddef.h>

int security_encrypt(const char* input, char* output, size_t max_len);
int security_decrypt(const char* input, char* output, size_t max_len);
int security_hash(const char* input, char* output, size_t max_len);
int security_validate_token(const char* token);

#endif
EOF;