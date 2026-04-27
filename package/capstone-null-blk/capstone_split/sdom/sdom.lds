SECTIONS
{
    /* Start at 0x0 */
    . = 0x0;

    /* Group all code and read-only data together */
    .init.text : {
        *(*.init.text)
        *(.text.my_start_symbol) /* Ensure your entry point is first */
        *(.text*)
        *(.rodata*)
    }

    /* Discard sections we don't need for a standalone blob */
    /DISCARD/ : {
        *(.hash)
        *(.gnu.hash)
        *(.dynsym)
        *(.dynstr)
        *(.rela*)
        *(.note*)
        *(.comment)
    }
}
