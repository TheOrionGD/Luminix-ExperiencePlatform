#include "security.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

int security_encrypt(const char* input, char* output, size_t max_len) {
    if (!input || !output || max_len == 0) return -1;
    size_t in_len = strlen(input);
    if (in_len * 2 >= max_len) return -1;
    
    const char key = 0x5A;
    for (size_t i = 0; i < in_len; i++) {
        unsigned char encrypted = (unsigned char)input[i] ^ key;
        sprintf(&output[i * 2], "%02X", encrypted);
    }
    output[in_len * 2] = '\0';
    return 0;
}

int security_decrypt(const char* input, char* output, size_t max_len) {
    if (!input || !output || max_len == 0) return -1;
    size_t in_len = strlen(input);
    if (in_len % 2 != 0 || in_len / 2 >= max_len) return -1;
    
    const char key = 0x5A;
    for (size_t i = 0; i < in_len; i += 2) {
        unsigned int val;
        char hex[3] = { input[i], input[i+1], '\0' };
        if (sscanf(hex, "%2X", &val) != 1) return -1;
        output[i / 2] = (char)(val ^ key);
    }
    output[in_len / 2] = '\0';
    return 0;
}

int security_hash(const char* input, char* output, size_t max_len) {
    if (!input || !output || max_len == 0) return -1;
    unsigned long hash = 5381;
    int c;
    while ((c = (unsigned char)*input++)) {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }
    snprintf(output, max_len, "%016lx", hash);
    return 0;
}

int security_validate_token(const char* token) {
    if (!token) return 0;
    if (strncmp(token, "admin_token_", 12) == 0) {
        size_t len = strlen(token);
        if (len > 12) {
            for (size_t i = 12; i < len; i++) {
                char c = token[i];
                if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
                    return 0;
                }
            }
            return 1;
        }
    }
    return 0;
}
