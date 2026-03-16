#include "idt.h"
#include "pic.h"
#include "keyboard.h"
#include "pmm.h"
#include "heap.h"
#include "shell.h"
#include <stdint.h>

static const int VGA_COLS = 80;
static const int VGA_ROWS = 25;
static const int VGA_BASE = 0xB8000;

static const unsigned char COLOR_DEFAULT = 0x0F;
static const unsigned char COLOR_TITLE   = 0x0B;
static const unsigned char COLOR_SUCCESS = 0x0A;
static const unsigned char COLOR_WARNING = 0x0E;
static const unsigned char COLOR_ERROR   = 0x0C;
static const unsigned char COLOR_HEADER  = 0x1F;

static int cursor_col = 0;
static int cursor_row = 0;

static void vga_write_cell(int col, int row, char ch, unsigned char color)
{
    volatile unsigned short* vga = (volatile unsigned short*)VGA_BASE;
    vga[(row * VGA_COLS) + col] = (unsigned short)((color << 8) | (unsigned char)ch);
}

static void vga_scroll()
{
    volatile unsigned short* vga = (volatile unsigned short*)VGA_BASE;
    for (int row = 2; row < VGA_ROWS; row++)
        for (int col = 0; col < VGA_COLS; col++)
            vga[(row-1)*VGA_COLS+col] = vga[row*VGA_COLS+col];
    for (int col = 0; col < VGA_COLS; col++)
        vga_write_cell(col, VGA_ROWS-1, ' ', COLOR_DEFAULT);
    cursor_row--;
}

void vga_clear()
{
    for (int row = 0; row < VGA_ROWS; row++) {
        unsigned char color = (row == 0) ? COLOR_HEADER : COLOR_DEFAULT;
        for (int col = 0; col < VGA_COLS; col++)
            vga_write_cell(col, row, ' ', color);
    }
    cursor_col = 0;
    cursor_row = 1;
}

void vga_putchar(char ch, unsigned char color = COLOR_DEFAULT)
{
    if (ch == '\n') { cursor_col = 0; cursor_row++; }
    else if (ch == '\b') {
        if (cursor_col > 0) { cursor_col--; vga_write_cell(cursor_col, cursor_row, ' ', COLOR_DEFAULT); }
    } else {
        vga_write_cell(cursor_col, cursor_row, ch, color);
        if (++cursor_col >= VGA_COLS) { cursor_col = 0; cursor_row++; }
    }
    if (cursor_row >= VGA_ROWS) vga_scroll();
}

void vga_print(const char* str, unsigned char color = COLOR_DEFAULT)
{
    for (int i = 0; str[i]; i++) vga_putchar(str[i], color);
}

void vga_print_hex(uint64_t value, unsigned char color)
{
    const char* hex = "0123456789ABCDEF";
    char buf[17]; buf[16] = '\0';
    for (int i = 15; i >= 0; i--) { buf[i] = hex[value & 0xF]; value >>= 4; }
    vga_print(buf, color);
}

static void vga_print_at(int col, int row, const char* str, unsigned char color)
{
    for (int i = 0; str[i] && (col+i) < VGA_COLS; i++)
        vga_write_cell(col+i, row, str[i], color);
}

void draw_title_bar()
{
    for (int col = 0; col < VGA_COLS; col++) vga_write_cell(col, 0, ' ', COLOR_HEADER);
    vga_print_at(1, 0, "KOS - Kyan Operating System", COLOR_HEADER);
    const char* build = "Checkpoint 11: Shell";
    int len = 0; while (build[len]) len++;
    vga_print_at(VGA_COLS - len - 1, 0, build, COLOR_HEADER);
}

static void draw_separator(unsigned char color = COLOR_DEFAULT)
{
    for (int col = 0; col < VGA_COLS; col++) vga_write_cell(col, cursor_row, '-', color);
    cursor_row++; cursor_col = 0;
}

extern "C" void kernel_main(unsigned int multiboot_info_ptr)
{
    vga_clear();
    draw_title_bar();

    vga_print("\n", COLOR_DEFAULT);
    draw_separator(COLOR_TITLE);
    vga_print("  KOS v1.0 Beta  |  Checkpoint 11  |  Boot Sequence\n", COLOR_TITLE);
    draw_separator(COLOR_TITLE);

    vga_print("\n  [1/6] Remapping PIC...", COLOR_WARNING);
    pic_init();
    vga_print(" OK\n", COLOR_SUCCESS);

    vga_print("  [2/6] Loading IDT...", COLOR_WARNING);
    idt_init();
    vga_print(" OK\n", COLOR_SUCCESS);

    vga_print("  [3/6] PS/2 Keyboard...", COLOR_WARNING);
    keyboard_init();
    vga_print(" OK\n", COLOR_SUCCESS);

    vga_print("  [4/6] Physical Memory...", COLOR_WARNING);
    pmm_init(multiboot_info_ptr);
    vga_print(" OK\n", COLOR_SUCCESS);

    vga_print("  [5/6] Kernel Heap...", COLOR_WARNING);
    heap_init();
    vga_print(" OK\n", COLOR_SUCCESS);

    vga_print("  [6/6] Enabling interrupts...", COLOR_WARNING);
    asm volatile ("sti");
    vga_print(" OK\n", COLOR_SUCCESS);

    vga_print("\n  Boot complete!\n", COLOR_SUCCESS);
    draw_separator(COLOR_SUCCESS);

    shell_init();
    shell_run();
}
