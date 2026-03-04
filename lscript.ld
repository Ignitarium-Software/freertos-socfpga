
_STACK_SIZE = DEFINED(_STACK_SIZE) ? _STACK_SIZE : 0x2000;
_HEAP_SIZE = DEFINED(_HEAP_SIZE) ? _HEAP_SIZE : 0x2000;

_EL0_STACK_SIZE = DEFINED(_EL0_STACK_SIZE) ? _EL0_STACK_SIZE : 0x2000;
_EL1_STACK_SIZE = DEFINED(_EL1_STACK_SIZE) ? _EL1_STACK_SIZE : 0x2000;
_EL2_STACK_SIZE = DEFINED(_EL2_STACK_SIZE) ? _EL2_STACK_SIZE : 0x2000;

MEMORY
{
   SRAM_RO (rx): ORIGIN = 0x82000000, LENGTH = 0x100000
   SRAM    (rw): ORIGIN = 0x82100000, LENGTH = 0x7DF00000
}

/* Specify the default entry point to the program */

ENTRY(_vector_table)
PROVIDE(end = _heap_end); /*Symbol for stdlib heap*/
PROVIDE(__stack = __el1_stack); /*Symbol for stdlib stack*/
/* Define the sections, and where they are mapped in memory */

SECTIONS
{
    .text  (READONLY) : {
        KEEP (*(.vectors))
        *(.boot)
        *(.text)
        *(.text.*)
        *(.gnu.linkonce.t.*)
        *(.plt)
        *(.gnu_warning)
        *(.gcc_execpt_table)
        *(.glue_7)
        *(.glue_7t)
        *(.ARM.extab)
        *(.gnu.linkonce.armextab.*)
    } > SRAM_RO

    .init (ALIGN(64))  (READONLY) : {
        KEEP (*(.init))
    } > SRAM_RO

    .fini (ALIGN(64))  (READONLY) : {
        KEEP (*(.fini))
    } > SRAM_RO

    .interp  (READONLY) : {
        KEEP (*(.interp))
    } > SRAM_RO

    .note-ABI-tag  (READONLY) : {
        KEEP (*(.note-ABI-tag))
    } > SRAM_RO

    .rodata  (READONLY) : {
        . = ALIGN(64);
        __rodata_start = .;
        *(.rodata)
        *(.rodata.*)
        *(.gnu.linkonce.r.*)
        __rodata_end = .;
    } > SRAM_RO

    .rodata1  (READONLY) : {
        . = ALIGN(64);
        __rodata1_start = .;
        *(.rodata1)
        *(.rodata1.*)
        __rodata1_end = .;
    } > SRAM_RO

    .sdata2 : {
        . = ALIGN(64);
        __sdata2_start = .;
        *(.sdata2)
        *(.sdata2.*)
        *(.gnu.linkonce.s2.*)
        __sdata2_end = .;
    } > SRAM

    .sbss2 : {
        . = ALIGN(64);
        __sbss2_start = .;
        *(.sbss2)
        *(.sbss2.*)
        *(.gnu.linkonce.sb2.*)
        __sbss2_end = .;
    } > SRAM

    .data : {
        . = ALIGN(64);
        __data_start = .;
        *(.data)
        *(.data.*)
        *(.gnu.linkonce.d.*)
        *(.jcr)
        *(.got)
        *(.got.plt)
        __data_end = .;
    } > SRAM

    .data1 : {
        . = ALIGN(64);
        __data1_start = .;
        *(.data1)
        *(.data1.*)
        __data1_end = .;
    } > SRAM

    .got : {
        *(.got)
    } > SRAM

    .got1 : {
        *(.got1)
    } > SRAM

    .got2 : {
        *(.got2)
    } > SRAM

    .ctors : {
        . = ALIGN(64);
        __CTOR_LIST__ = .;
        ___CTORS_LIST___ = .;
        KEEP (*crtbegin.o(.ctors))
        KEEP (*(EXCLUDE_FILE(*crtend.o) .ctors))
        KEEP (*(SORT(.ctors.*)))
        KEEP (*(.ctors))
        __CTOR_END__ = .;
        ___CTORS_END___ = .;
    } > SRAM

    .dtors : {
        . = ALIGN(64);
        __DTOR_LIST__ = .;
        ___DTORS_LIST___ = .;
        KEEP (*crtbegin.o(.dtors))
        KEEP (*(EXCLUDE_FILE(*crtend.o) .dtors))
        KEEP (*(SORT(.dtors.*)))
        KEEP (*(.dtors))
        __DTOR_END__ = .;
        ___DTORS_END___ = .;
    } > SRAM

    .fixup : {
        __fixup_start = .;
        *(.fixup)
        __fixup_end = .;
    } > SRAM

    .eh_frame : {
        *(.eh_frame)
    } > SRAM

    .eh_framehdr : {
        __eh_framehdr_start = .;
        *(.eh_framehdr)
        __eh_framehdr_end = .;
    } > SRAM

    .gcc_except_table : {
        *(.gcc_except_table)
    } > SRAM

    .ARM.exidx : {
        __exidx_start = .;
        *(.ARM.exidx*)
        *(.gnu.linkonce.armexidix.*.*)
        __exidx_end = .;
    } > SRAM

    .preinit_array : {
        . = ALIGN(64);
        __preinit_array_start = .;
        KEEP (*(SORT(.preinit_array.*)))
        KEEP (*(.preinit_array))
        __preinit_array_end = .;
    } > SRAM

    .init_array : {
        . = ALIGN(64);
        __init_array_start = .;
        KEEP (*(SORT(.init_array.*)))
        KEEP (*(.init_array))
        __init_array_end = .;
    } > SRAM

    .fini_array : {
        . = ALIGN(64);
        __fini_array_start = .;
        KEEP (*(SORT(.fini_array.*)))
        KEEP (*(.fini_array))
        __fini_array_end = .;
    } > SRAM

    .ARM.attributes : {
        __ARM.attributes_start = .;
        *(.ARM.attributes)
        __ARM.attributes_end = .;
    } > SRAM

    .sbss (NOLOAD) : {
        . = ALIGN(64);
        __sbss_start = .;
        *(.sbss)
        *(.sbss.*)
        *(.gnu.linkonce.sb.*)
        . = ALIGN(64);
        __sbss_end = .;
    } > SRAM

    .tdata : {
        . = ALIGN(64);
        __tdata_start = .;
        *(.tdata)
        *(.tdata.*)
        *(.gnu.linkonce.td.*)
        __tdata_end = .;
    } > SRAM

    .tbss : {
        . = ALIGN(64);
        __tbss_start = .;
        *(.tbss)
        *(.tbss.*)
        *(.gnu.linkonce.tb.*)
        __tbss_end = .;
    } > SRAM

    .bss (NOLOAD) : {
        . = ALIGN(64);
        __bss_start__ = .;
        *(.bss)
        *(.bss.*)
        *(.gnu.linkonce.b.*)
        *(COMMON)
        . = ALIGN(64);
        __bss_end__ = .;
    } > SRAM

    /*Leave 4K space for saftey (if any input section are to be put in .section)*/
    .stack (NOLOAD) : {
        . = ALIGN(64);
        _el2_stack_end = .;
        . += 0x40000;
        . = ALIGN(64);
        __el2_stack = .;
        _el21_stack_end = .;
        . += 0x40000;
        . = ALIGN(64);
        __el21_stack = .;
        _el1_stack_end = .;
        . += 0x40000;
        . = ALIGN(64);
        __el1_stack = .;
        _el11_stack_end = .;
        . += 0x40000;
        . = ALIGN(64);
        __el11_stack = .;
        _el0_stack_end = .;
        . += 0x40000;
        . = ALIGN(64);
        __el0_stack = .;
        _el01_stack_end = .;
        . += 0x40000;
        . = ALIGN(64);
        __el01_stack = .;
    } > SRAM

    _end = .; PROVIDE (end = .);
    _heap_end = 0xFFEFEFF0;

}
