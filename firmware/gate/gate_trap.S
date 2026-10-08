/* Machine-mode trap entry for the MARS gate: switches to the firmware stack,
   saves the caller-saved registers as gate_fw.c's frame, and dispatches. */
#if __riscv_xlen == 64
#define STORE sd
#define LOAD  ld
#define REGBYTES 8
#else
#define STORE sw
#define LOAD  lw
#define REGBYTES 4
#endif

    .section .text.gate_trap, "ax"
    .globl gate_trap
    .align 4
gate_trap:
    csrrw sp, mscratch, sp
    addi  sp, sp, -16*REGBYTES
    STORE ra,  0*REGBYTES(sp)
    STORE t0,  1*REGBYTES(sp)
    STORE t1,  2*REGBYTES(sp)
    STORE t2,  3*REGBYTES(sp)
    STORE t3,  4*REGBYTES(sp)
    STORE t4,  5*REGBYTES(sp)
    STORE t5,  6*REGBYTES(sp)
    STORE t6,  7*REGBYTES(sp)
    STORE a0,  8*REGBYTES(sp)
    STORE a1,  9*REGBYTES(sp)
    STORE a2, 10*REGBYTES(sp)
    STORE a3, 11*REGBYTES(sp)
    STORE a4, 12*REGBYTES(sp)
    STORE a5, 13*REGBYTES(sp)
    STORE a6, 14*REGBYTES(sp)
    STORE a7, 15*REGBYTES(sp)
    mv    a0, sp
    call  gate_dispatch
    LOAD  ra,  0*REGBYTES(sp)
    LOAD  t0,  1*REGBYTES(sp)
    LOAD  t1,  2*REGBYTES(sp)
    LOAD  t2,  3*REGBYTES(sp)
    LOAD  t3,  4*REGBYTES(sp)
    LOAD  t4,  5*REGBYTES(sp)
    LOAD  t5,  6*REGBYTES(sp)
    LOAD  t6,  7*REGBYTES(sp)
    LOAD  a0,  8*REGBYTES(sp)
    LOAD  a1,  9*REGBYTES(sp)
    LOAD  a2, 10*REGBYTES(sp)
    LOAD  a3, 11*REGBYTES(sp)
    LOAD  a4, 12*REGBYTES(sp)
    LOAD  a5, 13*REGBYTES(sp)
    LOAD  a6, 14*REGBYTES(sp)
    LOAD  a7, 15*REGBYTES(sp)
    addi  sp, sp, 16*REGBYTES
    csrrw sp, mscratch, sp
    mret
