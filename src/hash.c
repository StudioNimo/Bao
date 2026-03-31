#include "hash.h"

#include <stdlib.h>
#include <string.h>

static void hex_encode_lower(const unsigned char *in, size_t in_len, char *out) {
  static const char *hex = "0123456789abcdef";
  for (size_t i = 0; i < in_len; i++) {
    out[i * 2 + 0] = hex[(in[i] >> 4) & 0xF];
    out[i * 2 + 1] = hex[in[i] & 0xF];
  }
  out[in_len * 2] = 0;
}

#if defined(BAO_USE_OPENSSL)
#include <openssl/evp.h>

struct bao_sha256_ctx {
  EVP_MD_CTX *md;
};

int bao_sha256_hex(const void *data, size_t len, char *out_hex, size_t out_hex_len) {
  if (!out_hex || out_hex_len < 65) return -1;
  unsigned char digest[EVP_MAX_MD_SIZE];
  unsigned int dlen = 0;

  EVP_MD_CTX *ctx = EVP_MD_CTX_new();
  if (!ctx) return -1;
  if (EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) != 1) {
    EVP_MD_CTX_free(ctx);
    return -1;
  }
  if (EVP_DigestUpdate(ctx, data, len) != 1) {
    EVP_MD_CTX_free(ctx);
    return -1;
  }
  if (EVP_DigestFinal_ex(ctx, digest, &dlen) != 1) {
    EVP_MD_CTX_free(ctx);
    return -1;
  }
  EVP_MD_CTX_free(ctx);

  if (dlen != 32) return -1;
  hex_encode_lower(digest, 32, out_hex);
  return 0;
}

bao_sha256_ctx *bao_sha256_new(void) {
  bao_sha256_ctx *c = (bao_sha256_ctx *)calloc(1, sizeof(*c));
  if (!c) return NULL;
  c->md = EVP_MD_CTX_new();
  if (!c->md) {
    free(c);
    return NULL;
  }
  if (EVP_DigestInit_ex(c->md, EVP_sha256(), NULL) != 1) {
    EVP_MD_CTX_free(c->md);
    free(c);
    return NULL;
  }
  return c;
}

int bao_sha256_update(bao_sha256_ctx *ctx, const void *data, size_t len) {
  if (!ctx || !ctx->md) return -1;
  return (EVP_DigestUpdate(ctx->md, data, len) == 1) ? 0 : -1;
}

int bao_sha256_final_hex(bao_sha256_ctx *ctx, char *out_hex, size_t out_hex_len) {
  if (!ctx || !ctx->md || !out_hex || out_hex_len < 65) return -1;
  unsigned char digest[EVP_MAX_MD_SIZE];
  unsigned int dlen = 0;
  if (EVP_DigestFinal_ex(ctx->md, digest, &dlen) != 1) return -1;
  if (dlen != 32) return -1;
  hex_encode_lower(digest, 32, out_hex);
  return 0;
}

void bao_sha256_free(bao_sha256_ctx *ctx) {
  if (!ctx) return;
  if (ctx->md) EVP_MD_CTX_free(ctx->md);
  free(ctx);
}

#else

// Minimal SHA-256 (public domain style, compact)
// Based on FIPS 180-4.

typedef struct {
  uint32_t h[8];
  uint64_t len;
  unsigned char buf[64];
  size_t buf_len;
} sha256_state;

static uint32_t rotr32(uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); }
static uint32_t ch(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (~x & z); }
static uint32_t maj(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (x & z) ^ (y & z); }
static uint32_t bsig0(uint32_t x) { return rotr32(x, 2) ^ rotr32(x, 13) ^ rotr32(x, 22); }
static uint32_t bsig1(uint32_t x) { return rotr32(x, 6) ^ rotr32(x, 11) ^ rotr32(x, 25); }
static uint32_t ssig0(uint32_t x) { return rotr32(x, 7) ^ rotr32(x, 18) ^ (x >> 3); }
static uint32_t ssig1(uint32_t x) { return rotr32(x, 17) ^ rotr32(x, 19) ^ (x >> 10); }

static uint32_t load_be32(const unsigned char *p) {
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}
static void store_be32(unsigned char *p, uint32_t v) {
  p[0] = (unsigned char)(v >> 24);
  p[1] = (unsigned char)(v >> 16);
  p[2] = (unsigned char)(v >> 8);
  p[3] = (unsigned char)(v);
}
static void store_be64(unsigned char *p, uint64_t v) {
  p[0] = (unsigned char)(v >> 56);
  p[1] = (unsigned char)(v >> 48);
  p[2] = (unsigned char)(v >> 40);
  p[3] = (unsigned char)(v >> 32);
  p[4] = (unsigned char)(v >> 24);
  p[5] = (unsigned char)(v >> 16);
  p[6] = (unsigned char)(v >> 8);
  p[7] = (unsigned char)(v);
}

static const uint32_t K[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u,
    0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu,
    0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu,
    0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau, 0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
    0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu,
    0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
    0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u, 0x19a4c116u,
    0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u,
    0xc67178f2u};

static void sha256_init(sha256_state *s) {
  s->h[0] = 0x6a09e667u;
  s->h[1] = 0xbb67ae85u;
  s->h[2] = 0x3c6ef372u;
  s->h[3] = 0xa54ff53au;
  s->h[4] = 0x510e527fu;
  s->h[5] = 0x9b05688cu;
  s->h[6] = 0x1f83d9abu;
  s->h[7] = 0x5be0cd19u;
  s->len = 0;
  s->buf_len = 0;
}

static void sha256_compress(sha256_state *s, const unsigned char block[64]) {
  uint32_t w[64];
  for (int i = 0; i < 16; i++) w[i] = load_be32(block + (size_t)i * 4);
  for (int i = 16; i < 64; i++) w[i] = ssig1(w[i - 2]) + w[i - 7] + ssig0(w[i - 15]) + w[i - 16];

  uint32_t a = s->h[0], b = s->h[1], c = s->h[2], d = s->h[3];
  uint32_t e = s->h[4], f = s->h[5], g = s->h[6], h = s->h[7];

  for (int i = 0; i < 64; i++) {
    uint32_t t1 = h + bsig1(e) + ch(e, f, g) + K[i] + w[i];
    uint32_t t2 = bsig0(a) + maj(a, b, c);
    h = g;
    g = f;
    f = e;
    e = d + t1;
    d = c;
    c = b;
    b = a;
    a = t1 + t2;
  }

  s->h[0] += a;
  s->h[1] += b;
  s->h[2] += c;
  s->h[3] += d;
  s->h[4] += e;
  s->h[5] += f;
  s->h[6] += g;
  s->h[7] += h;
}

static void sha256_update(sha256_state *s, const void *data, size_t len) {
  const unsigned char *p = (const unsigned char *)data;
  s->len += (uint64_t)len;

  while (len > 0) {
    size_t space = 64 - s->buf_len;
    size_t take = (len < space) ? len : space;
    memcpy(s->buf + s->buf_len, p, take);
    s->buf_len += take;
    p += take;
    len -= take;
    if (s->buf_len == 64) {
      sha256_compress(s, s->buf);
      s->buf_len = 0;
    }
  }
}

static void sha256_final(sha256_state *s, unsigned char out[32]) {
  uint64_t bit_len = s->len * 8;

  // append 0x80 then pad with zeros
  s->buf[s->buf_len++] = 0x80;
  if (s->buf_len > 56) {
    while (s->buf_len < 64) s->buf[s->buf_len++] = 0;
    sha256_compress(s, s->buf);
    s->buf_len = 0;
  }
  while (s->buf_len < 56) s->buf[s->buf_len++] = 0;
  store_be64(s->buf + 56, bit_len);
  sha256_compress(s, s->buf);

  for (int i = 0; i < 8; i++) store_be32(out + (size_t)i * 4, s->h[i]);
}

struct bao_sha256_ctx {
  sha256_state st;
};

int bao_sha256_hex(const void *data, size_t len, char *out_hex, size_t out_hex_len) {
  if (!out_hex || out_hex_len < 65) return -1;
  sha256_state st;
  sha256_init(&st);
  sha256_update(&st, data, len);
  unsigned char digest[32];
  sha256_final(&st, digest);
  hex_encode_lower(digest, 32, out_hex);
  return 0;
}

bao_sha256_ctx *bao_sha256_new(void) {
  bao_sha256_ctx *c = (bao_sha256_ctx *)calloc(1, sizeof(*c));
  if (!c) return NULL;
  sha256_init(&c->st);
  return c;
}

int bao_sha256_update(bao_sha256_ctx *ctx, const void *data, size_t len) {
  if (!ctx) return -1;
  sha256_update(&ctx->st, data, len);
  return 0;
}

int bao_sha256_final_hex(bao_sha256_ctx *ctx, char *out_hex, size_t out_hex_len) {
  if (!ctx || !out_hex || out_hex_len < 65) return -1;
  unsigned char digest[32];
  sha256_final(&ctx->st, digest);
  hex_encode_lower(digest, 32, out_hex);
  return 0;
}

void bao_sha256_free(bao_sha256_ctx *ctx) { free(ctx); }

#endif

char *generate_sha256(const void *data, size_t len) {
  char *out = (char *)malloc(65);
  if (!out) return NULL;
  if (bao_sha256_hex(data, len, out, 65) != 0) {
    free(out);
    return NULL;
  }
  return out;
}

void bao_short_hash_from_hex(const char *full_hex, char out_short[8]) {
  if (!full_hex) {
    out_short[0] = 0;
    return;
  }
  for (int i = 0; i < 7; i++) {
    out_short[i] = full_hex[i] ? full_hex[i] : 0;
  }
  out_short[7] = 0;
}
