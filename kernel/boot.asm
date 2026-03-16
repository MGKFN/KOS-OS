[BITS 32]

section .multiboot_header
align 8

mb2_header_start:
    dd  0xE85250D6
    dd  0
    dd  mb2_header_end - mb2_header_start
    dd  -(0xE85250D6 + 0 + (mb2_header_end - mb2_header_start))
    dw  0
    dw  0
    dd  8
mb2_header_end:

section .bss
align 4096

pml4_table:
    resb 4096
pdpt:
    resb 4096
pd:
    resb 4096
    
; Extra PD entries for more identity mapping
pd2:
    resb 4096
pd3:
    resb 4096
pd4:
    resb 4096
pd5:
    resb 4096
pd6:
    resb 4096
pd7:
    resb 4096
pd8:
    resb 4096

stack_bottom:
    resb 16384
stack_top:

section .data
align 8
saved_multiboot_info: dq 0

section .text
global _start
extern kernel_main

_start:
    mov esp, stack_top
    mov dword [saved_multiboot_info], ebx
    
    cmp eax, 0x36d76289
    jne no_multiboot
    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000001
    jb  no_long_mode
    mov eax, 0x80000001
    cpuid
    test edx, (1 << 29)
    jz  no_long_mode
    call setup_paging
    call enable_long_mode
    lgdt [gdt64.pointer]
    jmp 0x08:long_mode_entry
    hlt

setup_paging:
    ; Clear all page tables
    mov edi, pml4_table
    mov ecx, (4096 * 10) / 4  ; PML4 + PDPT + 8 PDs
    xor eax, eax
    rep stosd
    
    ; PML4[0] → PDPT
    mov eax, pdpt
    or  eax, 0x03
    mov [pml4_table], eax
    
    ; PDPT[0] → PD (first 1GB)
    mov eax, pd
    or  eax, 0x03
    mov [pdpt], eax
    
    ; Map first 16MB using 2MB huge pages (8 entries)
    mov dword [pd + 0*8], 0x00000083  ; 0-2MB
    mov dword [pd + 1*8], 0x00200083  ; 2-4MB
    mov dword [pd + 2*8], 0x00400083  ; 4-6MB
    mov dword [pd + 3*8], 0x00600083  ; 6-8MB
    mov dword [pd + 4*8], 0x00800083  ; 8-10MB
    mov dword [pd + 5*8], 0x00A00083  ; 10-12MB
    mov dword [pd + 6*8], 0x00C00083  ; 12-14MB
    mov dword [pd + 7*8], 0x00E00083  ; 14-16MB
    
    ret

enable_long_mode:
    mov eax, pml4_table
    mov cr3, eax
    mov eax, cr4
    or  eax, (1 << 5)
    mov cr4, eax
    mov ecx, 0xC0000080
    rdmsr
    or  eax, (1 << 8)
    wrmsr
    mov eax, cr0
    or  eax, (1 << 31)
    mov cr0, eax
    ret

no_multiboot:
    mov eax, 0xDEAD0001
    hlt

no_long_mode:
    mov eax, 0xDEAD0002
    hlt

[BITS 64]

long_mode_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov rsp, stack_top
    
    xor rdi, rdi
    mov edi, dword [saved_multiboot_info]
    
    call kernel_main

halt_loop:
    cli
    hlt
    jmp halt_loop

section .rodata
gdt64:
    dq 0
    .code_seg: equ $ - gdt64
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10011010b
    db 10101111b
    db 0x00
    .data_seg: equ $ - gdt64
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10010010b
    db 11001111b
    db 0x00
    .pointer:
        dw $ - gdt64 - 1
        dq gdt64
