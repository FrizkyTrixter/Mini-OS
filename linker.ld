ENTRY(_start)

SECTIONS
{
    /* Load the kernel starting at physical address 1 MiB */
    . = 1M;

    /* --- Multiboot header --- */
    .multiboot ALIGN(4K) :
    {
        KEEP(*(.multiboot))
        KEEP(*(.multiboot_header))
    }

    /* --- Code and read-only data --- */
    .text ALIGN(4K) :
    {
        *(.text*)
    }

    .rodata ALIGN(4K) :
    {
        *(.rodata*)
    }

    /* --- Initialized data --- */
    .data ALIGN(4K) :
    {
        *(.data*)
    }

    /* --- Uninitialized data (BSS) --- */
    .bss ALIGN(4K) :
    {
        *(.bss*)
        *(COMMON)
    }

    /* Mark end of kernel image */
    _kernel_end = .;
    PROVIDE(kernel_end = .);

    /* --- Optional heap region (reserved space after kernel) --- */
    .heap ALIGN(4K) :
    {
        *(.heap*)
    }

    /* Provide symbols for C code to locate heap start/end */
    PROVIDE(end = .);
    PROVIDE(_end = .);

    /* --- Debugging / symbol table --- */
    /DISCARD/ :
    {
        *(.comment)
        *(.note*)
    }
}
