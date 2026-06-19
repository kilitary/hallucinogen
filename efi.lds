OUTPUT_FORMAT(elf64-x86-64)
ENTRY(efi_main)

SECTIONS
{
  . = 0;
  ImageBase = .;

  .hash : { *(.hash) }
  .text : { *(.text .text.*) }
  .rodata : { *(.rodata .rodata.*) }
  .data : { *(.data .data.*) }
  .bss : { *(COMMON .bss .bss.*) }
  .rela : { *(.rela*) }

  /DISCARD/ : { *(.note.* .comment .interp .dynstr .dynsym) }
}
