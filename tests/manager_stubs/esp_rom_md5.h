#pragma once
#include <openssl/evp.h>
#include <assert.h>
typedef struct {EVP_MD_CTX *ctx;} md5_context_t;
static void esp_rom_md5_init(md5_context_t *m){m->ctx=EVP_MD_CTX_new();assert(m->ctx);assert(EVP_DigestInit_ex(m->ctx,EVP_md5(),NULL)==1);}
static void esp_rom_md5_update(md5_context_t *m,const void *bytes,unsigned n){assert(EVP_DigestUpdate(m->ctx,bytes,n)==1);}
static void esp_rom_md5_final(unsigned char *out,md5_context_t *m){unsigned n;assert(EVP_DigestFinal_ex(m->ctx,out,&n)==1&&n==16);EVP_MD_CTX_free(m->ctx);}
