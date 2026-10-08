/* User-mode view of the MARS firmware behind the PMP gate: every MARS_* call is an
   ecall into machine mode (gate_fw.c).  Include instead of calling mars.c directly. */
#ifndef MARS_GATE_H
#define MARS_GATE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum gate_fn {
    GATE_SelfTest, GATE_CapabilityGet, GATE_SequenceHash, GATE_SequenceUpdate,
    GATE_SequenceComplete, GATE_PcrExtend, GATE_RegRead, GATE_Derive, GATE_DpDerive,
    GATE_PublicRead, GATE_Quote, GATE_Sign, GATE_SignatureVerify,
    GATE_TestSetFailure, GATE_TestReset, GATE_LastFault,
};

long gate_call(long fn, long a0, long a1, long a2, long a3, long a4, long a5);

/* Machine-mode entry: protects the firmware, then runs entry(argc, argv) in user mode. */
void gate_run_user(int (*entry)(int, char **), int argc, char **argv) __attribute__((noreturn));

/* Test-only hooks, present when the firmware is built with MARS_GATE_TEST_HOOKS. */
static inline void gate_test_set_failure(void) { gate_call(GATE_TestSetFailure, 0, 0, 0, 0, 0, 0); }
static inline void gate_test_reset(void)       { gate_call(GATE_TestReset, 0, 0, 0, 0, 0, 0); }
/* mcause of the last user-mode access fault the gate absorbed (0 = none), then cleared. */
static inline long gate_last_fault(void)       { return gate_call(GATE_LastFault, 0, 0, 0, 0, 0, 0); }
#endif
