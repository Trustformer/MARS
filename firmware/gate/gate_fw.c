/* Machine-mode side of the MARS gate: PMP closes the firmware block to user mode,
   ecalls dispatch to MARS_* after checking that every buffer lies outside the block,
   and mbedTLS allocates from a pool inside it.  Linked into the protected block. */
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "mars.h"
#include "gate.h"

extern char __fw_start[], __fw_end[], __fw_bss_start[], __fw_bss_end[];
extern char __fw_heap[], __fw_heap_end[], __fw_stack_top[];
extern bool failure;
void _MARS_Init(void);
void gate_trap(void);

#define PMP_R   0x01
#define PMP_W   0x02
#define PMP_X   0x04
#define PMP_TOR 0x08

#define csr_read(r)     ({ uintptr_t v_; asm volatile ("csrr %0, " #r : "=r"(v_)); v_; })
#define csr_write(r, v) asm volatile ("csrw " #r ", %0" :: "r"((uintptr_t)(v)))

/* The block's .bss lies outside crt0's; zero it before _MARS_Init runs. */
static void __attribute__((constructor(101))) fw_zero_bss(void)
{
    memset(__fw_bss_start, 0, (size_t)(__fw_bss_end - __fw_bss_start));
}

/* mbedTLS allocates only inside one call, so the pool empties (and is wiped) when
   the last allocation is freed. */
static size_t heap_used;
static unsigned heap_live;

void *fw_calloc(size_t n, size_t size)
{
    if (size && n > SIZE_MAX / size) return NULL;
    size_t sz = (n * size + 15) & ~(size_t)15;
    if (sz > (size_t)(__fw_heap_end - __fw_heap) - heap_used) return NULL;
    void *p = __fw_heap + heap_used;
    heap_used += sz;
    heap_live++;
    memset(p, 0, sz);
    return p;
}

void fw_free(void *p)
{
    if (p && heap_live && --heap_live == 0) {
        memset(__fw_heap, 0, heap_used);
        heap_used = 0;
    }
}

static bool in_user(uintptr_t a, size_t len)
{
    if (a + len < a) return false;
    return a + len <= (uintptr_t)__fw_start || a >= (uintptr_t)__fw_end;
}

/* Trap frame from gate_trap.S: ra, t0-t6, a0-a7. */
#define A(i) f[8 + (i)]
#define P(i) ((void *)A(i))
#define USER(i, len) do { if (!in_user(A(i), (len))) return MARS_RC_BUFFER; } while (0)
#define DL 32   /* PROFILE_LEN_DIGEST = SIGN = KSYM */

static long last_fault;

static long call(long fn, uintptr_t *f)
{
    switch (fn) {
    case GATE_SelfTest:
        return MARS_SelfTest(A(0) != 0);
    case GATE_CapabilityGet:
        USER(1, (uint16_t)A(2) == 2 ? 2 : 0);
        return MARS_CapabilityGet((uint16_t)A(0), P(1), (uint16_t)A(2));
    case GATE_SequenceHash:
        return MARS_SequenceHash();
    case GATE_SequenceUpdate:
        USER(0, A(1)); USER(3, sizeof(size_t));
        return MARS_SequenceUpdate(P(0), A(1), P(2), P(3));
    case GATE_SequenceComplete:
        USER(1, sizeof(size_t));
        USER(0, A(1) && *(size_t *)A(1) >= DL ? DL : 0);
        return MARS_SequenceComplete(P(0), P(1));
    case GATE_PcrExtend:
        USER(1, DL);
        return MARS_PcrExtend((uint16_t)A(0), P(1));
    case GATE_RegRead:
        USER(1, DL);
        return MARS_RegRead((uint16_t)A(0), P(1));
    case GATE_Derive:
        USER(1, (uint16_t)A(2)); USER(3, DL);
        return MARS_Derive((uint32_t)A(0), P(1), (uint16_t)A(2), P(3));
    case GATE_DpDerive:
        USER(1, (uint16_t)A(2));
        return MARS_DpDerive((uint32_t)A(0), P(1), (uint16_t)A(2));
    case GATE_PublicRead:
        USER(1, (uint16_t)A(2));
        return MARS_PublicRead(A(0) != 0, P(1), (uint16_t)A(2), P(3));
    case GATE_Quote:
        USER(1, (uint16_t)A(2)); USER(3, (uint16_t)A(4)); USER(5, DL);
        return MARS_Quote((uint32_t)A(0), P(1), (uint16_t)A(2), P(3), (uint16_t)A(4), P(5));
    case GATE_Sign:
        USER(0, (uint16_t)A(1)); USER(2, DL); USER(3, DL);
        return MARS_Sign(P(0), (uint16_t)A(1), P(2), P(3));
    case GATE_SignatureVerify:
        USER(1, (uint16_t)A(2)); USER(3, DL); USER(4, DL); USER(5, sizeof(bool));
        return MARS_SignatureVerify(A(0) != 0, P(1), (uint16_t)A(2), P(3), P(4), P(5));
#ifdef MARS_GATE_TEST_HOOKS
    case GATE_TestSetFailure:
        failure = true;
        return 0;
    case GATE_TestReset:
        _MARS_Init();
        return 0;
#endif
    case GATE_LastFault: {
        long c = last_fault;
        last_fault = 0;
        return c;
    }
    default:
        return MARS_RC_COMMAND;
    }
}

void gate_dispatch(uintptr_t *f)
{
    uintptr_t cause = csr_read(mcause), epc = csr_read(mepc);
    bool from_user = (csr_read(mstatus) & (3u << 11)) == 0;

    if (cause == 8) {                                   /* ecall from user mode */
        A(0) = (uintptr_t)call((long)A(7), f);
        csr_write(mepc, epc + 4);
        return;
    }
    if (from_user && (cause == 5 || cause == 7)) {      /* user load/store into the block */
        last_fault = (long)cause;
        csr_write(mepc, epc + ((*(uint16_t *)epc & 3) == 3 ? 4 : 2));
        return;
    }
    exit(1000 + (int)cause);
}

void gate_run_user(int (*entry)(int, char **), int argc, char **argv)
{
    uintptr_t r = PMP_R | PMP_W | PMP_X;
    /* TOR: [0, fw_start) user RWX, [fw_start, fw_end) closed, [fw_end, top) user RWX */
    csr_write(pmpaddr0, (uintptr_t)__fw_start >> 2);
    csr_write(pmpaddr1, (uintptr_t)__fw_end >> 2);
    csr_write(pmpaddr2, (uintptr_t)-1);
    csr_write(pmpcfg0, (PMP_TOR | r) | (PMP_TOR << 8) | ((PMP_TOR | r) << 16));
    csr_write(mscratch, __fw_stack_top);
    csr_write(mtvec, gate_trap);
    csr_write(mcounteren, 7);                           /* cycle, time, instret */
    csr_write(mepc, entry);
    csr_write(mstatus, csr_read(mstatus) & ~(uintptr_t)(3u << 11));   /* MPP = user */
    register uintptr_t a0 asm("a0") = (uintptr_t)argc;
    register uintptr_t a1 asm("a1") = (uintptr_t)argv;
    register uintptr_t ra asm("ra") = (uintptr_t)exit;   /* entry's return value is the exit code */
    asm volatile ("mret" :: "r"(a0), "r"(a1), "r"(ra));
    __builtin_unreachable();
}
