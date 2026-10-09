/* mbedTLS for the MARS firmware: SHA-256 and HMAC (the md layer), nothing else. */
#define MBEDTLS_MD_C
#define MBEDTLS_SHA256_C

#ifdef MARS_BAREMETAL
/* On bare metal, mbedTLS allocates from fw_alloc.c's pool. */
#include <stddef.h>
#define MBEDTLS_PLATFORM_C
#define MBEDTLS_PLATFORM_MEMORY
#define MBEDTLS_PLATFORM_CALLOC_MACRO fw_calloc
#define MBEDTLS_PLATFORM_FREE_MACRO   fw_free
void *fw_calloc(size_t n, size_t size);
void fw_free(void *p);
#endif
