/*
 * This linker script is used to drive the link step of the kernel, by e.g.
 * forcing the linker to use specific addresses when allocating space for
 * sections and symbols.
 *
 * It assumes flat physical memory (RAM) starting at 0, of size 64MB,
 * of which only "upper memory", starting at 1MB, is used.
 *
 * On x86, the first 1MB of physical memory is where legacy BIOS mappings
 * are mapped. Completely skip that region for convenience.
 *
 * For a map of lower memory, see http://wiki.osdev.org/Memory_Map_(x86).
 */

/*
 * Override the default entry point. This sets the address of the first
 * instruction run when the boot loader passes control to the kernel.
 */
ENTRY(boot_start)

/*
 * The memory command is used to describe regions of memory. Here, a single
 * region of RAM is defined. Adding another region, such as the video RAM
 * at 0xa0000, would allow other commands in this script to allocate symbols
 * out of that region.
 *
 * Describing memory regions is optional. It is best used when building for
 * known devices with a specific memory layout.
 */
MEMORY
{
    FLASH_ALIAS : ORIGIN = 0x00000000, LENGTH = 0x100000
    FLASH       : ORIGIN = 0x08000000, LENGTH = 0x100000
    RAM         : ORIGIN = 0x20000000, LENGTH = 0x1C000
}

/*
 * The program headers define segments in the ELF image. they are used to
 * fix properties on sections mapped to segments. Here, the PT_LOAD flag
 * tells the boot loader that a segment must actually be loaded from the
 * ELF image to memory. Some sections, such as most debugging sections, are
 * normally not loaded to memory. FLAGS are used to set Unix-like
 * permissions to a segment, so that a value of 4 means the segment may
 * only contain read-only non-executable data, 5 (4 + 1) means read-only
 * executable data (normally instructions), and 6 (4 + 2) means read-write
 * non-executable data.
 *
 * The hdr segment is meant to contain the multiboot header. The name "text"
 * is the historical name used to refer to instructions.
 *
 * See https://sourceware.org/binutils/docs-2.29/ld/index.html.
 */
PHDRS
{
    vectors PT_LOAD FLAGS(5);
    text    PT_LOAD FLAGS(5);
    data    PT_LOAD FLAGS(6);

    /*
     * XXX Currently required to avoid warning, probably because of
     * different alignments of .data and .bss.
     */
    bss     PT_LOAD FLAGS(6);
}

/*
 * Sections define how the image data are partitioned.
 *
 * Common sections include :
 *
 *  - .text
 *      The code section.
 *  - .data
 *      The section for initialized data (e.g. static int var = 123;).
 *  - .bss
 *      The section for uninitialized data (e.g. static int var;). Its name
 *      is historical and means "Block Started by Symbol". The .bss section
 *      is special in that it takes no space in the kernel image, because
 *      it's filled with bytes of value 0. Its size in memory is stored
 *      in the ELF file, and in this case, the boot loader initializes the
 *      memory for the .bss section.
 *
 * Here, an additional section is used to store the multiboot header, and
 * any section for read-only data produced by the compiler is forced into
 * the .data section.
 *
 * Sections are allocated out of the RAM memory region, and mapped to heir
 * corresponding program headers segment.
 *
 * See https://sourceware.org/binutils/docs-2.29/ld/Input-Section-Basics.html#Input-Section-Basics
 * for more information about the syntax used below.
 */
SECTIONS
{
    .vectors : {
        *(.vectors)
    } > FLASH_ALIAS AT > FLASH : vectors

    .text : {
        *(.text*)
        *(.rodata*)
    } > FLASH_ALIAS AT > FLASH : text

    _lma_data_addr = .;

    .data : {
        _data_start = .;
        *(.data*)
        _data_end = .;
    } > RAM AT > FLASH : data

    .bss : {
        _bss_start = .;
        *(.bss)
        _bss_end = .;
    } > RAM AT > FLASH : bss

    /*
     * The .eh_frame section is used by DWARF tools to unwind the stack,
     * allowing software to dump stack traces. Although this section could
     * safely be left in the kernel image, it may confuse people who
     * disassemble it.
     */
    /DISCARD/ : {
        *(.eh_frame)
        *(.ARM*)
    }
}
