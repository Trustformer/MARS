/* User-mode stubs: the MARS API as ecalls.  Linked outside the protected block. */
#include "gate.h"
#include "mars.h"

long gate_call(long fn, long a0, long a1, long a2, long a3, long a4, long a5)
{
    register long r0 asm("a0") = a0;
    register long r1 asm("a1") = a1;
    register long r2 asm("a2") = a2;
    register long r3 asm("a3") = a3;
    register long r4 asm("a4") = a4;
    register long r5 asm("a5") = a5;
    register long r7 asm("a7") = fn;
    asm volatile ("ecall" : "+r"(r0) : "r"(r1), "r"(r2), "r"(r3), "r"(r4), "r"(r5), "r"(r7) : "memory");
    return r0;
}

#define C(fn, ...) (MARS_RC)gate_call(GATE_##fn, __VA_ARGS__)
#define P(x) ((long)(uintptr_t)(x))

MARS_RC MARS_SelfTest(bool fullTest) { return C(SelfTest, fullTest, 0, 0, 0, 0, 0); }
MARS_RC MARS_CapabilityGet(uint16_t pt, void *cap, uint16_t caplen) { return C(CapabilityGet, pt, P(cap), caplen, 0, 0, 0); }
MARS_RC MARS_SequenceHash() { return C(SequenceHash, 0, 0, 0, 0, 0, 0); }
MARS_RC MARS_SequenceUpdate(const void *in, size_t inSize, void *out, size_t *outlen)
    { return C(SequenceUpdate, P(in), (long)inSize, P(out), P(outlen), 0, 0); }
MARS_RC MARS_SequenceComplete(void *out, size_t *outlen) { return C(SequenceComplete, P(out), P(outlen), 0, 0, 0, 0); }
MARS_RC MARS_PcrExtend(uint16_t pcrIndex, const void *dig) { return C(PcrExtend, pcrIndex, P(dig), 0, 0, 0, 0); }
MARS_RC MARS_RegRead(uint16_t regIndex, void *dig) { return C(RegRead, regIndex, P(dig), 0, 0, 0, 0); }
MARS_RC MARS_Derive(uint32_t regSelect, const void *ctx, uint16_t ctxlen, void *out)
    { return C(Derive, regSelect, P(ctx), ctxlen, P(out), 0, 0); }
MARS_RC MARS_DpDerive(uint32_t regSelect, const void *ctx, uint16_t ctxlen)
    { return C(DpDerive, regSelect, P(ctx), ctxlen, 0, 0, 0); }
MARS_RC MARS_PublicRead(bool restricted, const void *ctx, uint16_t ctxlen, void *pub)
    { return C(PublicRead, restricted, P(ctx), ctxlen, P(pub), 0, 0); }
MARS_RC MARS_Quote(uint32_t regSelect, const void *nonce, uint16_t nlen, const void *ctx, uint16_t ctxlen, void *sig)
    { return C(Quote, regSelect, P(nonce), nlen, P(ctx), ctxlen, P(sig)); }
MARS_RC MARS_Sign(const void *ctx, uint16_t ctxlen, const void *dig, void *sig)
    { return C(Sign, P(ctx), ctxlen, P(dig), P(sig), 0, 0); }
MARS_RC MARS_SignatureVerify(bool restricted, const void *ctx, uint16_t ctxlen, const void *dig, const void *sig, bool *result)
    { return C(SignatureVerify, restricted, P(ctx), ctxlen, P(dig), P(sig), P(result)); }
