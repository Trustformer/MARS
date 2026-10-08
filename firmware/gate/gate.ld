/* htif.ld from riscv libgloss-htif, with the MARS firmware (libmarsfw.a) in its own
   block, which gate_fw.c closes to user mode with PMP. */
OUTPUT_ARCH ("riscv")
ENTRY (_start)

SECTIONS
{
    . = 0x80000000;

    /* Keep under a power of 2 to reserve space for TCB */
    PROVIDE (__stack_size_min = 24K);

    .text : {
        *(.text.init)
        *(.text.unlikely .text.unlikely.*)
        *(.text.startup .text.startup.*)
        *(EXCLUDE_FILE (*libmarsfw.a:*) .text .text.*)
        *(.gnu.linkonce.t.*)
    }
    PROVIDE (_etext = .);

    /* Static Thread Local Storage template */
    .tdata : {
        PROVIDE_HIDDEN (__tdata_start = .);
        *(.tdata .tdata.*)
        *(.gnu.linkonce.td.*)
        PROVIDE_HIDDEN (__tdata_end = .);
    }
    PROVIDE (__tdata_size = SIZEOF (.tdata));

    .tbss (NOLOAD) : {
        PROVIDE_HIDDEN (__tbss_start = .);
        PROVIDE_HIDDEN (__tbss_offset = ABSOLUTE (__tbss_start - __tdata_start));
        *(.tbss .tbss.*)
        *(.gnu.linkonce.tb.*)
        *(.tcommon)
        PROVIDE_HIDDEN (__tbss_end = .);
    }
    PROVIDE (__tbss_size = SIZEOF (.tbss));

    .rodata : {
        *(EXCLUDE_FILE (*libmarsfw.a:*) .rodata .rodata.*)
        *(.gnu.linkonce.r.*)
    }

    .preinit_array : {
        PROVIDE_HIDDEN (__preinit_array_start = .);
        KEEP (*(.preinit_array))
        PROVIDE_HIDDEN (__preinit_array_end = .);
    }

    .init_array : {
        PROVIDE_HIDDEN (__init_array_start = .);
        KEEP (*(SORT_BY_INIT_PRIORITY(.init_array.*) SORT_BY_INIT_PRIORITY(.ctors.*)))
        KEEP (*(.init_array EXCLUDE_FILE (*crtbegin.o *crtbegin?.o *crtend.o *crtend?.o ) .ctors))
        PROVIDE_HIDDEN (__init_array_end = .);
    }

    .fini_array : {
        PROVIDE_HIDDEN (__fini_array_start = .);
        KEEP (*(SORT_BY_INIT_PRIORITY(.fini_array.*) SORT_BY_INIT_PRIORITY(.dtors.*)))
        KEEP (*(.fini_array EXCLUDE_FILE (*crtbegin.o *crtbegin?.o *crtend.o *crtend?.o ) .dtors))
        PROVIDE_HIDDEN (__fini_array_end = .);
    }

    .ctors : {
        KEEP (*crtbegin.o(.ctors))
        KEEP (*crtbegin?.o(.ctors))
        KEEP (*(EXCLUDE_FILE (*crtend.o *crtend?.o) .ctors))
        KEEP (*(SORT(.ctors.*)))
        KEEP (*(.ctors))
    }

    .dtors : {
        KEEP (*crtbegin.o(.dtors))
        KEEP (*crtbegin?.o(.dtors))
        KEEP (*(EXCLUDE_FILE (*crtend.o *crtend?.o) .dtors))
        KEEP (*(SORT(.dtors.*)))
        KEEP (*(.dtors))
    }

    /* Host-Target Interface */
    .htif ALIGN(0x40) : {
        *(.htif)
    }

    .data : ALIGN (8) {
        *(EXCLUDE_FILE (*libmarsfw.a:*) .data .data.*)
        *(.gnu.linkonce.d.*)
    }

    .sdata : {
        PROVIDE (__global_pointer$ = . + 0x800);
        *(EXCLUDE_FILE (*libmarsfw.a:*) .sdata .sdata.*)
        *(.gnu.linkonce.s.*)
        *(EXCLUDE_FILE (*libmarsfw.a:*) .srodata .srodata.*)
    }

    PROVIDE (_edata = .);

    .bss : ALIGN (8) {
        PROVIDE_HIDDEN (__bss_start = .);
        *(EXCLUDE_FILE (*libmarsfw.a:*) .sbss .sbss.*)
        *(.gnu.linkonce.sb.*)
        *(EXCLUDE_FILE (*libmarsfw.a:*) .bss .bss.*)
        *(.gnu.linkonce.b.*)
        *(EXCLUDE_FILE (*libmarsfw.a:*) COMMON)
        PROVIDE_HIDDEN (__bss_end = .);
    }

    /* MARS firmware: closed to user mode by PMP. */
    . = ALIGN (64);
    PROVIDE (__fw_start = .);
    .fw_text : {
        *libmarsfw.a:*(.text .text.* .rodata .rodata.* .srodata .srodata.*)
    }
    .fw_data : ALIGN (8) {
        *libmarsfw.a:*(.data .data.* .sdata .sdata.*)
    }
    .fw_bss (NOLOAD) : ALIGN (16) {
        PROVIDE (__fw_bss_start = .);
        *libmarsfw.a:*(.sbss .sbss.* .bss .bss.* COMMON)
        . = ALIGN (16);
        PROVIDE (__fw_heap = .);
        . += 4K;
        PROVIDE (__fw_heap_end = .);
        . += 8K;
        PROVIDE (__fw_stack_top = .);
        PROVIDE (__fw_bss_end = .);
    }
    . = ALIGN (64);
    PROVIDE (__fw_end = .);

    . = ALIGN (8);
    PROVIDE (_end = .);

    /* Reserve heap space */
    PROVIDE(__heap_size = 128K);
    . += __heap_size;
    . = ALIGN (4K);
    PROVIDE (__heap_end = .);

    /* Place Thread Control Block (TCB) at bottom of hart stack */
    PROVIDE (__tcb_size = __tbss_end - __tdata_start);
    PROVIDE (__stack_align = MAX (ALIGNOF (.tdata), 0x10));
    PROVIDE (__stack_start = ALIGN (__stack_align));
    PROVIDE (__stack_shift = LOG2CEIL( ALIGN (__stack_size_min + __tcb_size, __stack_align)));
    PROVIDE (__stack_size = 1 << __stack_shift);

    /* Default boot hart */
    PROVIDE (__boot_hart = 0);
}
