/* Stage 3 differential driver: the same PcrExtend sequence the Verilator test
   bench runs, through the TCG C reference emulator.  Output format matches the
   bench's RegRead lines so the two can be diffed directly. */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "mars.h"

static void show(const char *label, int i)
{
    uint8_t dig[32];
    MARS_RC rc = MARS_RegRead((uint16_t)i, dig);
    printf("%s i =%-1d  rc=%0d dig=", label, i, rc);
    for (int j = 0; j < 32; j++) printf("%02x", dig[j]);
    printf("\n");
}

int main(void)
{
    uint8_t dig[32];
    MARS_RC rc;
    int i;

    show("RegRead     ", 0);
    show("RegRead     ", 1);

    for (i = 1; i <= 3; i++) {
        memset(dig, 0, sizeof(dig));
        dig[31] = (uint8_t)i;                 /* big-endian 256-bit value i */
        rc = MARS_PcrExtend(0, dig);
        printf("PcrExtend    i =0  rc=%0d\n", rc);
        show("RegRead     ", 0);
    }

    memset(dig, 0, sizeof(dig));
    dig[31] = 0xAA;
    rc = MARS_PcrExtend(1, dig);
    (void)rc;
    show("RegRead     ", 1);
    show("RegRead     ", 0);

    /* MARS_Quote over both PCRs.  The signature is HMAC(AK, snapshot) with AK
       derived from DP, so one value exercises the whole key hierarchy: the
       KDF framing, the snapshot field order, and both HMAC passes. */
    {
        uint8_t nonce[32], ctx[32], sig[32];
        int k;
        for (k = 0; k < 32; k++) nonce[k] = (uint8_t)(k + 1);
        for (k = 0; k < 32; k++) ctx[k]   = (uint8_t)(k + 0x21);
        rc = MARS_Quote(3, nonce, 32, ctx, 32, sig);
        printf("Quote        rsel=3 rc=%0d sig=", rc);
        for (k = 0; k < 32; k++) printf("%02x", sig[k]);
        printf("\n");
    }
    return 0;
}
