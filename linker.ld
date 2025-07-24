ENTRY(_start)

SECTIONS
{
    /* Start loading the kernel at physical address 1 MiB */
    . = 1M;

    /* Multiboot header section */
    .multiboot : { *(.multiboot*) }

    /* Code and read-only data */
    .text      ALIGN(4K) : { *(.text*) }
    .rodata    ALIGN(4K) : { *(.rodata*) }

    /* Initialized data */
    .data      ALIGN(4K) : { *(.data*) }

    /* Uninitialized data (BSS) */
    .bss       ALIGN(4K) : { *(.bss*) *(COMMON) }

    /* End-of-kernel symbol: first byte just past .bss */
    _kernel_end = .;

    /*
     * Place the heap *after* _kernel_end so it does *not* count
     * as part of the “kernel” for pmm_init.
     */
    .heap      ALIGN(4K) : { *(.heap*) }
}
