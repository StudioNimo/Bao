#pragma once

#include <stddef.h>
#include <stdint.h>

// Writes lowercase hex (64 chars) + NUL => requires out_hex_len >= 65
int bao_sha256_hex(const void *data, size_t len, char *out_hex, size_t out_hex_len);

// malloc'ed lowercase hex (64 chars + NUL). Caller frees. NULL on error.
char *generate_sha256(const void *data, size_t len);

// Copy first 7 hex chars from full_hex into out_short (NUL-terminated).
void bao_short_hash_from_hex(const char *full_hex, char out_short[8]);

// Streaming helper: sha256 of multiple chunks.
typedef struct bao_sha256_ctx bao_sha256_ctx;
bao_sha256_ctx *bao_sha256_new(void);
int bao_sha256_update(bao_sha256_ctx *ctx, const void *data, size_t len);
int bao_sha256_final_hex(bao_sha256_ctx *ctx, char *out_hex, size_t out_hex_len);
void bao_sha256_free(bao_sha256_ctx *ctx);

