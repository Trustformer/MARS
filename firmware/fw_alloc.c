/* mbedTLS's allocator on bare metal: a bump pool in the firmware's own .bss (inside
   the protected block behind the gate).  mbedTLS allocates only inside one call, so
   the pool empties, and is wiped, when the last allocation is freed. */
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static uint8_t pool[4096] __attribute__((aligned(16)));
static size_t used;
static unsigned live;

void *fw_calloc(size_t n, size_t size)
{
    if (size && n > SIZE_MAX / size) return NULL;
    size_t sz = (n * size + 15) & ~(size_t)15;
    if (sz > sizeof pool - used) return NULL;
    void *p = pool + used;
    used += sz;
    live++;
    memset(p, 0, sz);
    return p;
}

void fw_free(void *p)
{
    if (p && live && --live == 0) {
        memset(pool, 0, used);
        used = 0;
    }
}
