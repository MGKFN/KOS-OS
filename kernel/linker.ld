/* ============================================================
   K-DOS-1.0-Beta  —  linker.ld
   Linker Script: tells the linker WHERE to place each section
   of our kernel in memory, and in what order.

   WHY THIS MATTERS:
   When GRUB loads our kernel ELF binary, it places it in
   physical memory. We tell the linker the binary should be
   laid out assuming it starts at 1MB (0x100000). This is
   the standard "higher" load address that avoids conflicts
   with BIOS/real-mode memory below 1MB.

   SECTION ORDER (critical for Multiboot2):
   The Multiboot2 header MUST appear in the first 32KB of
   the binary. We put it first so GRUB can find it.
   ============================================================ */

/* Entry point symbol — the linker needs to know where execution begins.
   This matches the 'global _start' label in boot.asm              */
ENTRY(_start)

SECTIONS
{
    /* --------------------------------------------------------
       Load address: 1MB (0x100000)
       This is where GRUB will place our kernel in RAM.
       The dot (.) is the "location counter" — it tracks
       where we currently are in the output binary.
       -------------------------------------------------------- */
    . = 1M;

    /* --------------------------------------------------------
       .multiboot_header — MUST be first
       Contains the magic bytes GRUB scans for.
       ALIGN(8) ensures 8-byte alignment as required by spec.
       -------------------------------------------------------- */
    .multiboot_header :
    {
        KEEP(*(.multiboot_header))   /* KEEP = don't discard even if unreferenced */
    }

    /* --------------------------------------------------------
       .text — Executable code
       All compiled C++ functions and Assembly code land here.
       ALIGN(4096) aligns to a 4KB page boundary (good practice).
       -------------------------------------------------------- */
    . = ALIGN(4096);
    .text :
    {
        *(.text)        /* All .text sections from all object files */
        *(.text.*)      /* Also catches .text.unlikely, .text.hot, etc. */
    }

    /* --------------------------------------------------------
       .rodata — Read-only data
       String literals, const globals, the GDT table, etc.
       -------------------------------------------------------- */
    . = ALIGN(4096);
    .rodata :
    {
        *(.rodata)
        *(.rodata.*)
    }

    /* --------------------------------------------------------
       .data — Initialized read-write data
       Global and static variables that have initial values.
       -------------------------------------------------------- */
    . = ALIGN(4096);
    .data :
    {
        *(.data)
        *(.data.*)
    }

    /* --------------------------------------------------------
       .bss — Uninitialized data (zero-filled at load)
       Our stack and page tables live here (declared in boot.asm
       with 'resb'). The loader/GRUB zeros this region for us.

       We export __bss_start and __bss_end so that if we ever
       need to zero BSS manually, we know the bounds.
       -------------------------------------------------------- */
    . = ALIGN(4096);
    __bss_start = .;
    .bss :
    {
        *(COMMON)       /* Common symbols (some compilers emit these) */
        *(.bss)
        *(.bss.*)
    }
    __bss_end = .;

    /* --------------------------------------------------------
       Discard debug/metadata sections we don't need.
       This keeps the kernel binary smaller.
       -------------------------------------------------------- */
    /DISCARD/ :
    {
        *(.comment)         /* Compiler version strings */
        *(.note.*)          /* ELF notes */
        *(.eh_frame)        /* C++ exception frame info (not used yet) */
        *(.eh_frame_hdr)
    }
}
