/*
 * SPDX-License-Identifier: CC0-1.0
 *
 * SPDX-FileContributor: Antonio Niño Díaz, 2022
 * SPDX-FileContributor: David Guillen Fandos, 2024
 */

OUTPUT_FORMAT("elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(dldi_startup_80)

/* Used for the DLDI driver. Linked at the fake 0xBF800000 address. It is */
/* meant to be PIC but there's some support for constant fixing. */

MEMORY {
    ROM     : ORIGIN = 0xBF800000, LENGTH = 8K
}

SECTIONS
{
    .dldi_header : ALIGN(4)
    {
        __BINARY_START__ = .;
        KEEP (*(.dldi_header))
    } > ROM

    /* Actual Code */

    .text : ALIGN(4)
    {
        *(.text)
        *(.text*)
        *(.iwram)
        *(.iwram*)
        *iwram.*(.text*)
        *(.gnu.linkonce.t.*) /* Used for vague linking */
    } > ROM

    /* Read-only Data */

    .rodata : ALIGN(4)
    {
        *(.rodata)
        *(.rodata*)
    } > ROM

    /* Funny way to generate flags directly in the linker! Thanks asie! */
    PROVIDE (__fix_flags = (
                    ((ABSOLUTE(__got_end) - ABSOLUTE(__got_start)) > 0 ? 4 : 0) +
                    ((ABSOLUTE(__bss_end) - ABSOLUTE(__bss_start)) > 0 ? 8 : 0)));
    PROVIDE (__log_payload_size = LOG2CEIL(ABSOLUTE(__BINARY_END__) - ABSOLUTE(__BINARY_START__)));

    /* We should not generate any GOT data, since is is linked with LTO */
    __got_start = .;
    .got :
    {
        *(.got.plt)
        *(.got)
        *(.rel.got)
    } > ROM
    __got_end = .;

    /* Data and bss */
    .data : ALIGN(4)
    {
        *iwram.*(.data*)
        *(.data)
        *(.data*)
        . = ALIGN(4);
    } > ROM

    /* BSS should also be empty when using LTO */
    __bss_start = .;
    .bss : ALIGN(4)
    {
        *(.bss)
        *(.bss*)
        . = ALIGN(4);
    } > ROM
    __bss_end = .;

    __BINARY_END__ = .;
    __BINARY_SIZE__ = __BINARY_END__ - __BINARY_START__;
}
