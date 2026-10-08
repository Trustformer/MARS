# Firmware MARS: the emulator's state machine (emulator/c) on mbedTLS, for the host
# and for bare-metal RISC-V.
#
#   make host-check    the OpenSSL and mbedTLS builds of the drivers agree
#   make spike-check   the bare-metal builds on spike agree with the host
#
# Needs gcc with libcrypto (host-check) and riscv64-unknown-elf-gcc with spike
# (spike-check); in the Trustformer testbed: scripts/cy 'make -C ../mars/firmware ...'.

EMU   := ../emulator/c
MBED  := ../third_party/mbedtls
B     := build
SEEDS ?= 1 2 3 4 5
CALLS ?= 5000

# This project's Profile: two PCRs, production key labels.
PROFILE  := -DPROFILE_COUNT_PCR=2 -DMARS_PROFILE_PRD
MBED_SRC := sha256 md platform_util
FW_INC   := -I. -I$(EMU) -I$(MBED)/include '-DMBEDTLS_CONFIG_FILE="mars_mbedtls_config.h"'
FW_OBJ    = $(addprefix $(1)/,mars.o hw_sha2_mbedtls.o $(addsuffix .o,$(MBED_SRC)))

HOST_CC := gcc -O2 -std=gnu99 -Wno-deprecated-declarations
# Bare-metal variants: the testbed's default flags, and everything the core supports.
RV_CC      := riscv64-unknown-elf-gcc -mcmodel=medany -O2 -std=gnu99 -DMARS_BAREMETAL -DMARS_QUIET
RV_testbed := -march=rv64imafd -mabi=lp64d
RV_core    := -march=rv64imafdc_zba_zbb_zbs -mabi=lp64d
LD_testbed := -specs=htif_nano.specs
LD_core    := -specs=htif.specs
RV_LD      := -static -T htif.ld
# spike models the core; libc and crt0 carry compressed instructions either way.
SPIKE_ISA  := rv64imafdc_zicntr_zba_zbb_zbs

.PHONY: host-check spike-check clean
.SECONDARY:

# -- the firmware objects, per build -------------------------------------------
# Only mars.c takes the Profile header pre-included; mbedTLS compiles as itself.
# FWCC_<build> compiles the firmware; behind the gate it also renames MARS_* (gate/).
$(B)/%/mars.o: $(EMU)/mars.c
	@mkdir -p $(@D)
	$(FWCC_$*) $(PROFILE) $(FW_INC) -include hw_sha2_mbedtls.h -c $< -o $@
$(B)/%/hw_sha2_mbedtls.o: $(EMU)/hw_sha2_mbedtls.c
	@mkdir -p $(@D)
	$(FWCC_$*) $(PROFILE) $(FW_INC) -c $< -o $@
$(B)/%/sha256.o: $(MBED)/library/sha256.c
	@mkdir -p $(@D)
	$(FWCC_$*) $(FW_INC) -c $< -o $@
$(B)/%/md.o: $(MBED)/library/md.c
	@mkdir -p $(@D)
	$(FWCC_$*) $(FW_INC) -c $< -o $@
$(B)/%/platform_util.o: $(MBED)/library/platform_util.c
	@mkdir -p $(@D)
	$(FWCC_$*) $(FW_INC) -c $< -o $@
$(B)/%/platform.o: $(MBED)/library/platform.c
	@mkdir -p $(@D)
	$(FWCC_$*) $(FW_INC) -c $< -o $@
$(B)/%/gate_fw.o: gate/gate_fw.c
	@mkdir -p $(@D)
	$(FWCC_$*) $(FW_INC) -c $< -o $@
$(B)/%/gate_trap.o: gate/gate_trap.S
	@mkdir -p $(@D)
	$(FWCC_$*) -c $< -o $@

CC_host       := $(HOST_CC)
CC_rv-testbed := $(RV_CC) $(RV_testbed)
CC_rv-core    := $(RV_CC) $(RV_core)
FWCC_host       = $(CC_host)
FWCC_rv-testbed = $(CC_rv-testbed)
FWCC_rv-core    = $(CC_rv-core)

# Behind the PMP gate: the firmware in machine mode, the caller in user mode.
CC_rv-gate   := $(RV_CC) $(RV_core) -DMARS_GATE -Igate
FWCC_rv-gate := $(CC_rv-gate) -DMARS_GATE_TEST_HOOKS -include fw_rename.h
GATE_OBJ      = $(call FW_OBJ,$(B)/rv-gate) $(addprefix $(B)/rv-gate/,platform.o gate_fw.o gate_trap.o)

$(B)/rv-gate/libmarsfw.a: $(GATE_OBJ)
	riscv64-unknown-elf-ar rcs $@ $^
$(B)/rv-gate/%.riscv: test/%.c gate/gate_user.c $(B)/rv-gate/libmarsfw.a
	$(CC_rv-gate) -I$(EMU) -DNCALLS=$(CALLS) -DSEED=$(word 1,$(SEEDS)) $(LD_core) -static -T gate/gate.ld -o $@ $^

# -- drivers -------------------------------------------------------------------
$(B)/host/ref-%: test/%.c
	@mkdir -p $(@D)
	$(HOST_CC) $(PROFILE) -I$(EMU) -o $@ $< -include hw_sha2.h $(EMU)/mars.c $(EMU)/hw_sha2.c -lcrypto
$(B)/host/fw-%: test/%.c $(call FW_OBJ,$(B)/host)
	$(HOST_CC) -I$(EMU) -o $@ $^
$(B)/rv-testbed/%.riscv: test/%.c $(call FW_OBJ,$(B)/rv-testbed)
	$(CC_rv-testbed) -I$(EMU) -DNCALLS=$(CALLS) -DSEED=$(word 1,$(SEEDS)) $(LD_testbed) $(RV_LD) -o $@ $^
$(B)/rv-core/%.riscv: test/%.c $(call FW_OBJ,$(B)/rv-core)
	$(CC_rv-core) -I$(EMU) -DNCALLS=$(CALLS) -DSEED=$(word 1,$(SEEDS)) $(LD_core) $(RV_LD) -o $@ $^

# -- checks --------------------------------------------------------------------
host-check: $(B)/host/ref-mars_diff $(B)/host/fw-mars_diff $(B)/host/ref-stage3_vectors $(B)/host/fw-stage3_vectors
	@$(B)/host/ref-stage3_vectors | grep -v '^PS:\|^DP:\|^Skdf:' > $(B)/stage3.ref
	@$(B)/host/fw-stage3_vectors  | grep -v '^PS:\|^DP:\|^Skdf:' > $(B)/stage3.fw
	@cmp -s $(B)/stage3.ref test/stage3.expected || { echo "FAIL: the OpenSSL build no longer gives test/stage3.expected"; exit 1; }
	@cmp -s $(B)/stage3.fw  test/stage3.expected || { echo "FAIL: stage-3 vectors differ (mbedTLS)"; exit 1; }
	@echo "stage-3 vectors: OpenSSL == mbedTLS == test/stage3.expected"
	@for s in $(SEEDS); do \
	  $(B)/host/ref-mars_diff $(CALLS) $$s | grep -v '^UB' > $(B)/diff.ref.$$s; \
	  $(B)/host/fw-mars_diff  $(CALLS) $$s | grep -v '^UB' > $(B)/diff.fw.$$s; \
	  cmp -s $(B)/diff.ref.$$s $(B)/diff.fw.$$s || { echo "FAIL: seed $$s differs"; exit 1; }; \
	  echo "seed $$s: $$(grep -c '^@[0-9]' $(B)/diff.fw.$$s) calls, OpenSSL == mbedTLS (derived keys included)"; \
	done
	@echo "PASS host-check"

spike-check: $(B)/host/ref-mars_diff $(B)/rv-testbed/mars_diff.riscv $(B)/rv-core/mars_diff.riscv \
             $(B)/rv-gate/mars_diff.riscv $(B)/rv-gate/gate_pmp.riscv
	@$(B)/host/ref-mars_diff $(CALLS) $(word 1,$(SEEDS)) | grep '^@' > $(B)/spike.ref
	@spike --isa=$(SPIKE_ISA) $(B)/rv-testbed/mars_diff.riscv | grep '^@' > $(B)/spike.testbed
	@spike --isa=$(SPIKE_ISA) $(B)/rv-core/mars_diff.riscv | grep '^@' > $(B)/spike.core
	@spike --isa=$(SPIKE_ISA) $(B)/rv-gate/mars_diff.riscv | grep '^@' > $(B)/spike.gate
	@for v in testbed core gate; do \
	  cmp -s $(B)/spike.ref $(B)/spike.$$v || { echo "FAIL: spike ($$v) differs from the host"; exit 1; }; \
	  echo "$$v: $$(grep -c '^@[0-9]' $(B)/spike.$$v) calls on spike == host"; \
	done
	@spike --isa=$(SPIKE_ISA) $(B)/rv-gate/gate_pmp.riscv > $(B)/spike.pmp; \
	  grep -q '^PASS gate_pmp' $(B)/spike.pmp || { cat $(B)/spike.pmp; echo "FAIL: gate_pmp"; exit 1; }
	@echo "gate: user-mode loads and stores into the firmware fault; pointers into it are refused"
	@echo "PASS spike-check"

clean:
	rm -rf $(B)

# -- libmarsfw.a for another build ---------------------------------------------
#   make lib NAME=<dir> LIBCC='<compiler and flags>' LIBAR=<ar> [GATE=1]
# The Trustformer testbed builds its SoC programs against this.
ifdef NAME
FWCC_$(NAME) = $(LIBCC) $(if $(GATE),-DMARS_GATE -DMARS_GATE_TEST_HOOKS -Igate -include fw_rename.h)
LIB_OBJ      = $(call FW_OBJ,$(B)/$(NAME)) $(if $(GATE),$(addprefix $(B)/$(NAME)/,platform.o gate_fw.o gate_trap.o))
.PHONY: lib
lib: $(B)/$(NAME)/libmarsfw.a
$(B)/$(NAME)/libmarsfw.a: $(LIB_OBJ)
	$(LIBAR) rcs $@ $^
endif
