/* The PMP gate from user mode: MARS calls work, loads and stores into the firmware
   block fault, and a buffer pointing into the block is refused with MARS_RC_BUFFER. */
#include <stdio.h>
#include <stdint.h>
#include "gate.h"
#include "mars.h"

extern char __fw_start[], __fw_bss_start[], __fw_end[];
static int fails;

static void check(const char *what, long got, long want)
{
    if (got != want) { fails++; printf("FAIL %s: got %ld, expected %ld\n", what, got, want); }
    else printf("ok   %s = %ld\n", what, got);
}

static int user_main(int argc, char **argv)
{
    (void)argc; (void)argv;
    uint8_t dig[32];
    check("RegRead from user mode", MARS_RegRead(0, dig), MARS_RC_SUCCESS);
    check("no fault so far", gate_last_fault(), 0);

    volatile uint32_t *fw = (volatile uint32_t *)__fw_bss_start;
    uint32_t leak = *fw;
    check("load from the firmware block faults (mcause 5)", gate_last_fault(), 5);
    *fw = 0xdeadbeef;
    check("store into the firmware block faults (mcause 7)", gate_last_fault(), 7);
    (void)leak;

    check("RegRead into the block refused", MARS_RegRead(0, __fw_bss_start), MARS_RC_BUFFER);
    check("Sign reading the block refused", MARS_Sign(__fw_start, 32, dig, dig), MARS_RC_BUFFER);
    check("buffer straddling the block start refused",
          MARS_RegRead(0, __fw_start - 16), MARS_RC_BUFFER);
    check("RegRead from user mode still works", MARS_RegRead(0, dig), MARS_RC_SUCCESS);

    printf("%s gate_pmp (%d failed)\n", fails ? "FAIL" : "PASS", fails);
    return fails != 0;
}

int main(int argc, char **argv) { gate_run_user(user_main, argc, argv); }
