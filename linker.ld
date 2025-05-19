ENTRY(_start)

SECTIONS
{
  . = 1M;                 /* load the kernel at 1 MiB */

  .multiboot : { *(.multiboot*) }

  .text  ALIGN(4K) : { *(.text*) }
  .rodata ALIGN(4K) : { *(.rodata*) }
  .data  ALIGN(4K) : { *(.data*)  }
  .bss   ALIGN(4K) : { *(.bss*) *(COMMON) }
}

