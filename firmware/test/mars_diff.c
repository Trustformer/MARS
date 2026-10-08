/* Randomized differential driver over every MARS_* entry point of mars.c.
   A seeded splitmix64 stream picks calls and arguments (including invalid
   ones), so two builds of the emulator given the same SEED/NCALLS must print
   the same '@' lines; only the driver's lines start with '@'.
   Host:        ./mars_diff [ncalls] [seed]
   Bare metal:  -DNCALLS=... -DSEED=...  (argv is read when argc >= 2) */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include "mars.h"

extern bool failure;     /* mars.c:48 -- the driver's way into failure mode */
void _MARS_Init(void);   /* mars.c:360 -- the reset signal, run at boot as a constructor */

#ifndef NCALLS
#define NCALLS 5000
#endif
#ifndef SEED
#define SEED 1
#endif

#define DL 32            /* PROFILE_LEN_DIGEST = SIGN = KSYM for the SHA2 profile */
#define NREG 2           /* PROFILE_COUNT_PCR + PROFILE_COUNT_TSR */
#define GUARD 8          /* bytes past every output buffer that must stay untouched */
#define FILL 0xee

static uint64_t s;
static unsigned long n;  /* call index */
static unsigned long hits[16], fails[16];

static uint64_t rnd(void)
{
    uint64_t z = (s += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}
static uint32_t below(uint32_t k) { return (uint32_t)(rnd() % k); }
static bool chance(uint32_t pct) { return below(100) < pct; }
static void fill(uint8_t *b, size_t k) { for (size_t i = 0; i < k; i++) b[i] = (uint8_t)rnd(); }

static void hex(const void *b, size_t k)
{
    for (size_t i = 0; i < k; i++) printf("%02x", ((const uint8_t *)b)[i]);
}
static uint32_t fnv(const void *b, size_t k)   /* fingerprint of an input, so lines are self-describing */
{
    uint32_t h = 0x811c9dc5;
    if (!b) return 0;
    for (size_t i = 0; i < k; i++) h = (h ^ ((const uint8_t *)b)[i]) * 0x01000193;
    return h;
}
/* output buffer of len bytes, then GUARD bytes that must survive */
static uint8_t obuf[2][DL * 2 + GUARD];
static uint8_t *out_prep(int i) { memset(obuf[i], FILL, sizeof obuf[i]); return obuf[i]; }
static void out_show(const char *name, int i, size_t len)
{
    printf(" %s=", name);
    hex(obuf[i], len);
    for (size_t k = len; k < sizeof obuf[i]; k++)
        if (obuf[i][k] != FILL) { printf(" OVERRUN@%lu", (unsigned long)k); break; }
}
/* a call line is assembled before the call and printed after it, so the
   emulator's debug prints land on lines of their own */
static char line[512];
static int lp;
#define L(...) (lp += snprintf(line + lp, sizeof line - (size_t)lp, __VA_ARGS__))
static void tag(const char *name) { lp = 0; L("@%lu %s", n++, name); }
static void flush(void) { printf("%s", line); lp = 0; }
static void rc_show(int cc, MARS_RC rc) { hits[cc]++; if (rc) fails[cc]++; flush(); printf(" rc=%u", rc); }

/* SequenceHash opens the shared hash context; SequenceComplete and PcrExtend
   finalize it.  The emulator leaves Complete on a closed context unspecified, so
   that output depends on the crypto backend and prints outside the '@' stream. */
static bool seq_open;

/* inputs: a big pool; a call takes a random slice */
static uint8_t pool[4096];
static const void *in_ptr(size_t len, bool allow_null)
{
    if (allow_null && chance(8)) return NULL;
    return pool + below((uint32_t)(sizeof pool - len + 1));
}
static size_t in_len(uint32_t typical, uint32_t max)
{
    if (chance(85)) return below(typical + 1);
    return below(max + 1);
}
static uint16_t reg_index(void) { return chance(85) ? (uint16_t)below(NREG) : (uint16_t)rnd(); }
static uint32_t reg_select(void)
{
    if (chance(85)) return below(1u << NREG);
    return chance(50) ? (uint32_t)rnd() : (1u << below(32));
}

/* ---- one call per entry point ------------------------------------------- */

static void c_selftest(void)
{
    bool full = below(2);
    tag("SelfTest"); L(" full=%d", full);
    rc_show(MARS_CC_SelfTest, MARS_SelfTest(full)); printf("\n");
}

static void c_capget(void)
{
    uint16_t pt = chance(85) ? (uint16_t)(1 + below(11)) : (uint16_t)below(20);
    uint16_t caplen = chance(90) ? 2 : (uint16_t)below(5);
    bool null = chance(5);
    uint8_t *cap = out_prep(0);
    tag("CapabilityGet"); L(" pt=%u caplen=%u cap=%s", pt, caplen, null ? "NULL" : "buf");
    rc_show(MARS_CC_CapabilityGet, MARS_CapabilityGet(pt, null ? NULL : cap, caplen));
    out_show("cap", 0, 2); printf("\n");
}

static void c_seqhash(void)
{
    tag("SequenceHash");
    MARS_RC rc = MARS_SequenceHash();
    rc_show(MARS_CC_SequenceHash, rc); printf("\n");
    if (rc == 0) seq_open = true;
}

static void c_sequpdate(const void *in, size_t inlen, bool null_outlen)
{
    size_t outlen = 0xeeee;
    tag("SequenceUpdate"); L(" in=%08x inlen=%lu outlen=%s", (unsigned)fnv(in, inlen), (unsigned long)inlen,
                                  null_outlen ? "NULL" : "ptr");
    rc_show(MARS_CC_SequenceUpdate,
            MARS_SequenceUpdate(in, inlen, out_prep(1), null_outlen ? NULL : &outlen));
    printf(" outlen=%lu", (unsigned long)outlen); out_show("out", 1, 4); printf("\n");
}
static void c_sequpdate_rand(void)
{
    size_t len = in_len(100, 2048);
    const void *in = in_ptr(len, true);
    c_sequpdate(in, len, chance(4));
}

static MARS_RC c_seqcomplete(size_t cap, bool null_out, bool null_outlen)
{
    size_t outlen = cap;
    uint8_t *out = out_prep(0);
    MARS_RC rc;
    tag("SequenceComplete"); L(" out=%s outlen_in=%lu%s", null_out ? "NULL" : "buf", (unsigned long)cap,
                                    null_outlen ? " outlen=NULL" : "");
    rc = MARS_SequenceComplete(null_out ? NULL : out, null_outlen ? NULL : &outlen);
    if (rc == 0 && !seq_open) printf("UB ");
    if (rc == 0) seq_open = false;
    rc_show(MARS_CC_SequenceComplete, rc);
    printf(" outlen=%lu", (unsigned long)outlen); out_show("out", 0, DL); printf("\n");
    return rc;
}
static void c_seqcomplete_rand(void)
{
    size_t cap = chance(80) ? DL : (chance(50) ? DL * 2 : below(DL));
    bool null_out = chance(5), null_outlen = chance(5);
    c_seqcomplete(cap, null_out, null_outlen);
}

static void c_pcrextend(void)
{
    uint16_t idx = reg_index();
    bool null = chance(5);
    const void *dig = in_ptr(DL, false);
    tag("PcrExtend"); L(" idx=%u dig=%08x", idx, null ? 0u : (unsigned)fnv(dig, DL));
    MARS_RC rc = MARS_PcrExtend(idx, null ? NULL : dig);
    rc_show(MARS_CC_PcrExtend, rc); printf("\n");
    if (rc == 0) seq_open = false;
}

static MARS_RC c_regread(uint16_t idx, bool null)
{
    MARS_RC rc;
    tag("RegRead"); L(" idx=%u%s", idx, null ? " dig=NULL" : "");
    rc = MARS_RegRead(idx, null ? NULL : out_prep(0));
    rc_show(MARS_CC_RegRead, rc);
    if (!null) out_show("dig", 0, DL);
    printf("\n");
    return rc;
}

static void c_derive(void)
{
    uint32_t rsel = reg_select();
    uint16_t len = (uint16_t)in_len(64, 1024);
    const void *ctx = in_ptr(len, true);
    bool null = chance(5);
    tag("Derive"); L(" rsel=%08x ctx=%08x ctxlen=%u%s", (unsigned)rsel, (unsigned)fnv(ctx, len), len,
                          null ? " out=NULL" : "");
    rc_show(MARS_CC_Derive, MARS_Derive(rsel, ctx, len, null ? NULL : out_prep(0)));
    if (!null) out_show("out", 0, DL);
    printf("\n");
}

static void c_dpderive(void)
{
    uint32_t rsel = reg_select();
    uint16_t len = (uint16_t)in_len(64, 1024);
    const void *ctx = chance(15) ? NULL : in_ptr(len, false);
    if (!ctx && chance(70)) len = 0;           /* (NULL, 0) re-derives DP from PS */
    tag("DpDerive"); L(" rsel=%08x ctx=%s%08x ctxlen=%u", (unsigned)rsel, ctx ? "" : "NULL:",
                            (unsigned)fnv(ctx, len), len);
    rc_show(MARS_CC_DpDerive, MARS_DpDerive(rsel, ctx, len)); printf("\n");
}

static void c_publicread(void)
{
    bool restricted = below(2);
    uint16_t len = (uint16_t)in_len(32, 256);
    const void *ctx = in_ptr(len, true);
    tag("PublicRead"); L(" restricted=%d ctx=%08x ctxlen=%u", restricted, (unsigned)fnv(ctx, len), len);
    rc_show(MARS_CC_PublicRead, MARS_PublicRead(restricted, ctx, len, out_prep(0)));
    out_show("pub", 0, DL); printf("\n");
}

static MARS_RC c_quote(uint32_t rsel, const void *nonce, uint16_t nlen,
                       const void *ctx, uint16_t ctxlen, bool null_sig, uint8_t *sig_copy)
{
    MARS_RC rc;
    tag("Quote"); L(" rsel=%08x nonce=%08x nlen=%u ctx=%08x ctxlen=%u%s", (unsigned)rsel,
                         (unsigned)fnv(nonce, nlen), nlen, (unsigned)fnv(ctx, ctxlen), ctxlen,
                         null_sig ? " sig=NULL" : "");
    rc = MARS_Quote(rsel, nonce, nlen, ctx, ctxlen, null_sig ? NULL : out_prep(0));
    rc_show(MARS_CC_Quote, rc);
    if (!null_sig) { out_show("sig", 0, DL); if (sig_copy) memcpy(sig_copy, obuf[0], DL); }
    printf("\n");
    return rc;
}
static void c_quote_rand(void)
{
    uint16_t nlen = (uint16_t)in_len(32, 512), clen = (uint16_t)in_len(32, 512);
    const void *nonce = in_ptr(nlen, true), *ctx = in_ptr(clen, true);
    uint32_t rsel = reg_select();
    bool null_sig = chance(5);
    c_quote(rsel, nonce, nlen, ctx, clen, null_sig, NULL);
}

static MARS_RC c_sign(const void *ctx, uint16_t ctxlen, const void *dig, bool null_sig, uint8_t *sig_copy)
{
    MARS_RC rc;
    tag("Sign"); L(" ctx=%08x ctxlen=%u dig=%s%08x%s", (unsigned)fnv(ctx, ctxlen), ctxlen,
                        dig ? "" : "NULL:", (unsigned)fnv(dig, DL), null_sig ? " sig=NULL" : "");
    rc = MARS_Sign(ctx, ctxlen, dig, null_sig ? NULL : out_prep(0));
    rc_show(MARS_CC_Sign, rc);
    if (!null_sig) { out_show("sig", 0, DL); if (sig_copy) memcpy(sig_copy, obuf[0], DL); }
    printf("\n");
    return rc;
}
static void c_sign_rand(void)
{
    uint16_t len = (uint16_t)in_len(32, 512);
    const void *ctx = in_ptr(len, true), *dig = chance(5) ? NULL : in_ptr(DL, false);
    c_sign(ctx, len, dig, chance(5), NULL);
}

static void c_verify(bool restricted, const void *ctx, uint16_t ctxlen, const void *dig,
                     const void *sig, bool null_result)
{
    uint8_t res = FILL;      /* bool storage, prefilled */
    tag("SignatureVerify"); L(" restricted=%d ctx=%08x ctxlen=%u dig=%s%08x sig=%s%08x%s", restricted,
                                   (unsigned)fnv(ctx, ctxlen), ctxlen, dig ? "" : "NULL:", (unsigned)fnv(dig, DL),
                                   sig ? "" : "NULL:", (unsigned)fnv(sig, DL), null_result ? " result=NULL" : "");
    rc_show(MARS_CC_SignatureVerify,
            MARS_SignatureVerify(restricted, ctx, ctxlen, dig, sig, null_result ? NULL : (bool *)&res));
    printf(" result=%02x\n", res);
}
static void c_verify_rand(void)
{
    uint16_t len = (uint16_t)in_len(32, 512);
    const void *ctx = in_ptr(len, true);
    const void *dig = chance(4) ? NULL : in_ptr(DL, false);
    const void *sig = chance(4) ? NULL : in_ptr(DL, false);
    bool restricted = below(2), null_result = chance(4);
    c_verify(restricted, ctx, len, dig, sig, null_result);
}

/* ---- compound scenarios: the positive paths -------------------------------- */

static void s_sign_verify(void)          /* Sign, then verify it right / wrong */
{
    uint8_t sig[DL];
    uint16_t len = (uint16_t)in_len(32, 256);
    const void *ctx = in_ptr(len, false), *dig = in_ptr(DL, false);
    if (c_sign(ctx, len, dig, false, sig)) return;
    c_verify(false, ctx, len, dig, sig, false);      /* true */
    c_verify(true, ctx, len, dig, sig, false);       /* restricted key differs: false */
    { uint32_t at = below(DL); sig[at] ^= (uint8_t)(1u << below(8)); }
    c_verify(false, ctx, len, dig, sig, false);      /* tampered: false */
}

static void s_quote_verify(void)         /* Quote, rebuild the snapshot with the Sequence API, verify */
{
    uint8_t sig[DL], snap[DL], be[4];
    uint32_t rsel = below(1u << NREG);
    uint16_t nlen = (uint16_t)in_len(32, 256), clen = (uint16_t)in_len(32, 256);
    const void *nonce = in_ptr(nlen, false), *ctx = in_ptr(clen, false);
    if (c_quote(rsel, nonce, nlen, ctx, clen, false, sig)) return;
    be[0] = (uint8_t)(rsel >> 24); be[1] = (uint8_t)(rsel >> 16); be[2] = (uint8_t)(rsel >> 8); be[3] = (uint8_t)rsel;
    c_seqhash();
    c_sequpdate(be, 4, false);
    for (uint16_t i = 0; i < NREG; i++)
        if (rsel >> i & 1) {
            uint8_t r[DL];
            c_regread(i, false);
            memcpy(r, obuf[0], DL);
            c_sequpdate(r, DL, false);
        }
    c_sequpdate(nonce, nlen, false);
    if (c_seqcomplete(DL, false, false)) return;
    memcpy(snap, obuf[0], DL);
    c_verify(true, ctx, clen, snap, sig, false);     /* true */
    c_verify(false, ctx, clen, snap, sig, false);    /* unrestricted key: false */
}

static void s_sequence(void)             /* a whole hash sequence, chunked */
{
    int k = (int)below(6);
    c_seqhash();
    while (k--) c_sequpdate_rand();
    c_seqcomplete(DL, false, false);
    if (chance(30)) c_seqcomplete(DL, false, false);   /* complete again: use after final */
}

static void s_reset(void)
{
    tag("Reset(_MARS_Init)"); flush(); printf("\n");
    _MARS_Init();
}

static void one_call(uint32_t r);

/* a short burst of calls in failure mode, then the reset that clears it */
static void s_inject_failure(void)
{
    int k = 3 + (int)below(8);
    tag("InjectFailure"); flush(); printf("\n");
    failure = true;
    while (k--) one_call(below(800));
    s_reset();
}

int main(int argc, char **argv)
{
    unsigned long ncalls = NCALLS;
    uint64_t seed = SEED;
    if (argc > 1) ncalls = strtoul(argv[1], 0, 0);
    if (argc > 2) seed = strtoull(argv[2], 0, 0);
    s = seed;
    fill(pool, sizeof pool);
    printf("@ mars_diff seed=%lu ncalls=%lu\n", (unsigned long)seed, ncalls);

    /* A Sequence completed before any SequenceHash/PcrExtend since power-on: the
       hash context is still all-zero static storage.  Tagged UB, outside the '@' stream. */
    {
        size_t ol = 0;
        uint8_t *o = out_prep(0);
        MARS_RC r1 = MARS_SequenceUpdate("abc", 3, NULL, &ol);
        ol = DL;
        MARS_RC r2 = MARS_SequenceComplete(o, &ol);
        printf("UB uninitialised-sequence update rc=%u complete rc=%u outlen=%lu out=", r1, r2, (unsigned long)ol);
        hex(o, DL); printf("\n");
        MARS_SequenceHash();
        seq_open = true;
    }

    while (n < ncalls)
        one_call(below(1000));

    printf("@ summary calls=%lu\n", n);
    {
        static const char *nm[13] = { "SelfTest", "CapabilityGet", "SequenceHash", "SequenceUpdate",
            "SequenceComplete", "PcrExtend", "RegRead", "Derive", "DpDerive", "PublicRead", "Quote",
            "Sign", "SignatureVerify" };
        for (int i = 0; i <= MARS_CC_LAST; i++)
            printf("@ summary %-16s calls=%lu rc!=0:%lu\n", nm[i], hits[i], fails[i]);
    }
    return 0;
}

static void one_call(uint32_t r)
{
    if      (r <  40) c_selftest();
    else if (r <  90) c_capget();
    else if (r < 130) c_seqhash();
    else if (r < 210) c_sequpdate_rand();
    else if (r < 260) c_seqcomplete_rand();
    else if (r < 370) c_pcrextend();
    else if (r < 470) { uint16_t i = reg_index(); c_regread(i, chance(5)); }
    else if (r < 540) c_derive();
    else if (r < 590) c_dpderive();
    else if (r < 620) c_publicread();
    else if (r < 690) c_quote_rand();
    else if (r < 750) c_sign_rand();
    else if (r < 800) c_verify_rand();
    else if (r < 860) s_sign_verify();
    else if (r < 920) s_quote_verify();
    else if (r < 970) s_sequence();
    else if (r < 980) s_inject_failure();
    else              s_reset();
}
