#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "mbedtls/sha256.h"
#include "mbedtls/md.h"

#include "hw_sha2_mbedtls.h"
#include "mars_internal.h"

#ifdef MARS_QUIET
#define hexout(msg, buf, len) ((void)0)
#else
static void hexout(const char *msg, const void *buf, uint16_t len)
{
typeof(len) i;
    if (msg)
        printf("%s: ", msg);
    for (i=0; i<len; i++)
        printf("%02x", ((uint8_t *)buf)[i]);
    printf("\n");
}
#endif


bool CryptVerify(const void *key, const void *dig, const void *sig)
{
uint8_t mac[PROFILE_LEN_SIGN];
    CryptSign(mac, key, dig);
    return memcmp(mac, sig, PROFILE_LEN_SIGN) == 0;
}

// TPM version of 800-108 is in CryptHash.c, CryptKDFa()
// according to part 1 of the TPM Specification.
// Both counter "i" and bit length "L" are encoded in 4 big endian bytes.
// TPM uses 8192 (0x2000) for L.
// Here, i is only 1.

// from https://nvlpubs.nist.gov/nistpubs/SpecialPublications/NIST.SP.800-108r1-draft.pdf

// OpenSSL's KBKDF supports 800-108 Counter mode.
// However, the value for L is taken from the parent key size.
// No way to specify a different L (8192).

void CryptSkdf(void * child, const void * parent, char label, const void * ctx, uint16_t ctxlen)
{
    mbedtls_md_context_t hctx;
    mbedtls_md_init(&hctx);
    mbedtls_md_setup(&hctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
    //  HMAC (key, [i]2 || Label || 0x00 || Context || [L]2)
    mbedtls_md_hmac_starts(&hctx, parent, PROFILE_LEN_KSYM);
    mbedtls_md_hmac_update(&hctx, (const unsigned char *)"\x00\x00\x00\x01", 4);  // i = 1
    mbedtls_md_hmac_update(&hctx, (const unsigned char *)&label, 1);              // Label
    mbedtls_md_hmac_update(&hctx, (const unsigned char *)"", 1);                  // 0x00
    mbedtls_md_hmac_update(&hctx, ctx, ctxlen);            // Context
    mbedtls_md_hmac_update(&hctx, (const unsigned char *)"\x00\x00\x20\x00", 4);  // L = 0x2000 = 8192
    mbedtls_md_hmac_finish(&hctx, child);
    mbedtls_md_free(&hctx);
    hexout("Skdf", child, PROFILE_LEN_KSYM);
}

void CryptXkdf(void *key, const void *parent, char label, const void *ctx, uint16_t ctxlen)
{
    CryptSkdf(key, parent, label, ctx, ctxlen);
}

void CryptHashInit(profile_shc_t *hctx)
{
    mbedtls_sha256_init(hctx);
    mbedtls_sha256_starts(hctx, 0);
}

void CryptHashUpdate(profile_shc_t *hctx, const void *msg, size_t n)
{
    mbedtls_sha256_update(hctx, msg, n);
}

void CryptHashFinal(profile_shc_t *hctx, void *dig)
{
    mbedtls_sha256_finish(hctx, dig);
}

void CryptSign(void *out, const void *key, const void *digest)
{
    mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), key, PROFILE_LEN_KSYM, digest, PROFILE_LEN_DIGEST, out);
}

bool CryptSelfTest(bool fullTest)
{
profile_shc_t hctx;
uint8_t dig[PROFILE_LEN_DIGEST];
uint8_t exp[PROFILE_LEN_DIGEST] = "\x32\x9b\x3d\xcf\x79\x8a\x73\xe8\xb8\x7f\x48\x6b\xcd\xaa\x8e\x20\x70\xf6\x43\x7f\x1d\x47\x0f\xec\x6e\x17\x4e\xf8\xec\x7b\x15\x54";
    CryptHashInit(&hctx);
    CryptHashUpdate(&hctx, "PYTHON", 6);
    CryptHashFinal(&hctx, dig);
    return memcmp(dig, exp, PROFILE_LEN_DIGEST) == 0;
}
