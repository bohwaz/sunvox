OUTPUT_FORMAT("elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_Z15ARM_PalmOS_mainPKvPvPFmS0_mS0_mE)
SEARCH_DIR(/usr/local/arm-elf/lib);
SECTIONS
{
.text 0x00000000 : { *(arm_startup) *(text) *(.text.*) *(.gnu*) *(sec1) *(sec2) *(sec3) *(sec4) }
.alldata : { *(.rodata) *(.rodata.*) *(.data) *(.bss) }
.rel : { *(.data.rel.local) *(.data.rel) }
.got : { *(.got) *(.got.plt) }
}

