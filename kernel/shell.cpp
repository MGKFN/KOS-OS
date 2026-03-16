#include "shell.h"
#include "keyboard.h"
#include "pmm.h"
#include "heap.h"
#include "rtc.h"
#include <stdint.h>

extern void vga_print(const char* str, unsigned char color);
extern void vga_print_hex(uint64_t val, unsigned char color);

static unsigned char current_text_color = 0x0F;
static unsigned char current_bg_color = 0x00;

static char cmd_buffer[256];
static int cmd_len = 0;

#define MAX_HISTORY 10
static char history[MAX_HISTORY][256];
static int history_count = 0;
static int history_pos = -1;

// CP17: Pipe output buffer
#define PIPE_BUFFER_SIZE 4096
static char pipe_buffer[PIPE_BUFFER_SIZE];
static int pipe_buffer_pos = 0;
static bool capturing_output = false;

// Available commands for tab completion
static const char* commands[] = {
    "HELP", "CLS", "VER", "ECHO", "MEM", "CPU",
    "DATE", "TIME", "COLOR", "PAUSE", "REBOOT", "SHUTDOWN"
};
static const int num_commands = 12;

static int str_cmp(const char* a, const char* b) {
    int i = 0;
    while (a[i] && b[i]) {
        char ca = a[i], cb = b[i];
        if (ca >= 'a' && ca <= 'z') ca -= 32;
        if (cb >= 'a' && cb <= 'z') cb -= 32;
        if (ca != cb) return ca - cb;
        i++;
    }
    return a[i] - b[i];
}

static bool str_starts_with(const char* str, const char* prefix) {
    int i = 0;
    while (prefix[i]) {
        char cs = str[i], cp = prefix[i];
        if (cs >= 'a' && cs <= 'z') cs -= 32;
        if (cp >= 'a' && cp <= 'z') cp -= 32;
        if (cs != cp) return false;
        i++;
    }
    return true;
}

static void str_copy(char* dst, const char* src) {
    int i = 0;
    while (src[i]) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

static int str_len(const char* s) {
    int i = 0;
    while (s[i]) i++;
    return i;
}

static int hex_to_int(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

static void print_num(uint64_t n, int width) {
    char buf[32];
    int i = 0;
    if (n == 0) buf[i++] = '0';
    else {
        while (n > 0) { buf[i++] = '0' + (n % 10); n /= 10; }
    }
    while (i < width) buf[i++] = '0';
    while (i > 0) {
        extern void vga_putchar(char ch, unsigned char color);
        unsigned char col = (current_bg_color << 4) | current_text_color;
        vga_putchar(buf[--i], col);
    }
}

static void format_size(uint64_t bytes, char* output) {
    uint64_t kb = bytes / 1024;
    uint64_t mb = kb / 1024;
    uint64_t gb = mb / 1024;
    
    int i = 0;
    if (gb > 0) {
        uint64_t val = gb;
        char buf[32];
        int j = 0;
        if (val == 0) buf[j++] = '0';
        else {
            while (val > 0) { buf[j++] = '0' + (val % 10); val /= 10; }
        }
        while (j > 0) {
            output[i++] = buf[--j];
        }
        output[i++] = ' ';
        output[i++] = 'G';
        output[i++] = 'B';
    } else if (mb > 0) {
        uint64_t val = mb;
        char buf[32];
        int j = 0;
        if (val == 0) buf[j++] = '0';
        else {
            while (val > 0) { buf[j++] = '0' + (val % 10); val /= 10; }
        }
        while (j > 0) {
            output[i++] = buf[--j];
        }
        output[i++] = ' ';
        output[i++] = 'M';
        output[i++] = 'B';
    } else {
        uint64_t val = kb;
        char buf[32];
        int j = 0;
        if (val == 0) buf[j++] = '0';
        else {
            while (val > 0) { buf[j++] = '0' + (val % 10); val /= 10; }
        }
        while (j > 0) {
            output[i++] = buf[--j];
        }
        output[i++] = ' ';
        output[i++] = 'K';
        output[i++] = 'B';
    }
    output[i] = '\0';
}

static void shell_print(const char* str, unsigned char color) {
    if (capturing_output) {
        int i = 0;
        while (str[i] && pipe_buffer_pos < PIPE_BUFFER_SIZE - 1) {
            pipe_buffer[pipe_buffer_pos++] = str[i++];
        }
        pipe_buffer[pipe_buffer_pos] = '\0';
    } else {
        vga_print(str, color);
    }
}

struct CPUInfo {
    char vendor[13];
    uint32_t max_function;
    uint32_t features_ecx;
    uint32_t features_edx;
};

static void get_cpu_info(CPUInfo* info) {
    uint32_t eax, ebx, ecx, edx;
    asm volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0));
    info->max_function = eax;
    *(uint32_t*)(info->vendor + 0) = ebx;
    *(uint32_t*)(info->vendor + 4) = edx;
    *(uint32_t*)(info->vendor + 8) = ecx;
    info->vendor[12] = '\0';
    if (info->max_function >= 1) {
        asm volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(1));
        info->features_ecx = ecx;
        info->features_edx = edx;
    }
}

static inline void outb(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1" :: "a"(val), "Nd"(port));
}

static void cmd_help() {
    unsigned char col = (current_bg_color << 4) | current_text_color;
    shell_print("\n  KOS Commands:\n", 0x0B);
    shell_print("  ============================================\n", col);
    shell_print("  HELP       - Show this help\n", col);
    shell_print("  CLS        - Clear screen\n", col);
    shell_print("  VER        - Show KOS version\n", col);
    shell_print("  ECHO text  - Print text\n", col);
    shell_print("  MEM        - Memory statistics\n", col);
    shell_print("  CPU        - CPU information\n", col);
    shell_print("  DATE       - Show current date\n", col);
    shell_print("  TIME       - Show current time\n", col);
    shell_print("  COLOR XY   - Set color (X=bg, Y=fg, 0-F)\n", col);
    shell_print("  PAUSE      - Wait for keypress\n", col);
    shell_print("  REBOOT     - Restart computer\n", col);
    shell_print("  SHUTDOWN   - Power off (ACPI)\n", col);
    shell_print("  ============================================\n", col);
    shell_print("  TIP: Use UP/DOWN arrows for command history\n", 0x08);
    shell_print("       Use TAB for command completion\n", 0x08);
    shell_print("       Use | for pipes (e.g. MEM | PAUSE)\n", 0x08);
}

extern void vga_clear();
extern void draw_title_bar();

static void cmd_cls() {
    if (!capturing_output) {
        vga_clear();
        draw_title_bar();
    }
    unsigned char col = (current_bg_color << 4) | current_text_color;
    shell_print("\n", col);
}

static void cmd_ver() {
    unsigned char col = (current_bg_color << 4) | current_text_color;
    shell_print("\n", col);
    shell_print("            _  __ ___   ___\n", 0x0B);
    shell_print("           | |/ // _ \\ / __|\n", 0x0B);
    shell_print("           | ' <| (_) |\\__ \\\n", 0x0B);
    shell_print("           |_|\\_\\\\___/ |___/\n", 0x0B);
    shell_print("\n", col);
    shell_print("  ============================================\n", 0x09);
    shell_print("   Kyan Operating System\n", 0x0F);
    shell_print("  ============================================\n", 0x09);
    shell_print("\n", col);
    shell_print("  Version:        1.0 Beta (Build 17)\n", col);
    shell_print("  Architecture:   x86_64 (64-bit)\n", col);
    shell_print("  Toolchain:      GCC 13.2.0 + NASM 2.16\n", col);
    shell_print("  Boot Protocol:  Multiboot2 Specification\n", col);
    shell_print("\n", col);
    shell_print("  Copyright (C) 2026 Kyan. All rights reserved.\n", 0x08);
    shell_print("  KOS(TM) is a trademark of Kyan.\n", 0x08);
    shell_print("\n", col);
    shell_print("  This software is proprietary and confidential.\n", 0x08);
    shell_print("  Unauthorized use, copying or distribution is\n", 0x08);
    shell_print("  strictly prohibited by law.\n", 0x08);
    shell_print("\n", col);
}

static void cmd_echo(const char* args) {
    unsigned char col = (current_bg_color << 4) | current_text_color;
    shell_print("\n  ", col);
    if (*args == '\0') shell_print("ECHO is on.\n", col);
    else { shell_print(args, col); shell_print("\n", col); }
}

static void cmd_mem() {
    unsigned char col = (current_bg_color << 4) | current_text_color;
    shell_print("\n  Memory Statistics:\n", 0x0B);
    shell_print("  ============================================\n", col);
    
    uint64_t total = pmm_get_total_memory();
    uint64_t free = pmm_get_free_memory();
    uint64_t used = total - free;
    
    if (capturing_output) {
        char buf[64];
        shell_print("  Total RAM:  ", col);
        format_size(total, buf);
        shell_print(buf, col);
        shell_print("\n  Free RAM:   ", col);
        format_size(free, buf);
        shell_print(buf, col);
        shell_print("\n  Used RAM:   ", col);
        format_size(used, buf);
        shell_print(buf, col);
        shell_print("\n  Heap Used:  ", col);
        format_size(heap_get_used(), buf);
        shell_print(buf, col);
        shell_print("\n  Heap Free:  ", col);
        format_size(heap_get_free(), buf);
        shell_print(buf, col);
        shell_print("\n", col);
    } else {
        shell_print("  Total RAM:  ", col);
        vga_print_hex(total / 1024, 0x0A);
        shell_print(" KB\n", col);
        shell_print("  Free RAM:   ", col);
        vga_print_hex(free / 1024, 0x0A);
        shell_print(" KB\n", col);
        shell_print("  Used RAM:   ", col);
        vga_print_hex(used / 1024, 0x0E);
        shell_print(" KB\n", col);
        shell_print("\n", col);
        shell_print("  Heap Used:  ", col);
        vga_print_hex(heap_get_used(), 0x0E);
        shell_print(" bytes\n", col);
        shell_print("  Heap Free:  ", col);
        vga_print_hex(heap_get_free(), 0x0A);
        shell_print(" bytes\n", col);
        shell_print("  ============================================\n", col);
    }
}

static void cmd_cpu() {
    unsigned char col = (current_bg_color << 4) | current_text_color;
    CPUInfo cpu;
    get_cpu_info(&cpu);
    shell_print("\n  CPU Information:\n", 0x0B);
    shell_print("  ============================================\n", col);
    shell_print("  Vendor:     ", col);
    shell_print(cpu.vendor, 0x0A);
    shell_print("\n", col);
    shell_print("  Mode:       64-bit Long Mode\n", col);
    shell_print("  Features:   ", col);
    if (cpu.features_edx & (1 << 0))  shell_print("FPU ", 0x0A);
    if (cpu.features_edx & (1 << 23)) shell_print("MMX ", 0x0A);
    if (cpu.features_edx & (1 << 25)) shell_print("SSE ", 0x0A);
    if (cpu.features_edx & (1 << 26)) shell_print("SSE2 ", 0x0A);
    if (cpu.features_ecx & (1 << 0))  shell_print("SSE3 ", 0x0A);
    shell_print("\n", col);
    shell_print("  ============================================\n", col);
}

static void cmd_date() {
    unsigned char col = (current_bg_color << 4) | current_text_color;
    DateTime dt;
    rtc_read_datetime(&dt);
    shell_print("\n  Current date is: ", col);
    print_num(dt.day, 2);
    shell_print("-", col);
    print_num(dt.month, 2);
    shell_print("-", col);
    print_num(dt.year, 4);
    shell_print("\n", col);
}

static void cmd_time() {
    unsigned char col = (current_bg_color << 4) | current_text_color;
    DateTime dt;
    rtc_read_datetime(&dt);
    shell_print("\n  Current time is: ", col);
    print_num(dt.hour, 2);
    shell_print(":", col);
    print_num(dt.minute, 2);
    shell_print(":", col);
    print_num(dt.second, 2);
    shell_print("\n", col);
}

static void cmd_color(const char* args) {
    unsigned char col = (current_bg_color << 4) | current_text_color;
    if (*args == '\0') {
        shell_print("\n  Current: BG=", col);
        if (!capturing_output) vga_print_hex(current_bg_color, col);
        shell_print(" FG=", col);
        if (!capturing_output) vga_print_hex(current_text_color, col);
        shell_print("\n", col);
        return;
    }
    int bg = hex_to_int(args[0]);
    int fg = args[1] ? hex_to_int(args[1]) : current_text_color;
    if (bg < 0 || fg < 0) {
        shell_print("\n  Usage: COLOR XY (X=bg 0-F, Y=fg 0-F)\n", 0x0C);
        return;
    }
    current_bg_color = (unsigned char)bg;
    current_text_color = (unsigned char)fg;
    shell_print("\n  Color changed.\n", (current_bg_color << 4) | current_text_color);
}

static void cmd_pause() {
    unsigned char col = (current_bg_color << 4) | current_text_color;
    shell_print("\n  Press any key to continue . . . ", col);
    if (!capturing_output) {
        while (true) {
            char c = keyboard_getchar();
            if (c) break;
            asm volatile("hlt");
        }
        shell_print("\n", col);
    }
}

static void cmd_reboot() {
    shell_print("\n  Rebooting system...\n", 0x0E);
    if (!capturing_output) {
        asm volatile("cli");
        outb(0x64, 0xFE);
        while(1) asm volatile("hlt");
    }
}

static void cmd_shutdown() {
    shell_print("\n  Shutting down...\n", 0x0E);
    if (!capturing_output) {
        outb(0xB004, 0x2000);
        outb(0x604, 0x2000);
        outb(0x4004, 0x3400);
        shell_print("  ACPI shutdown failed. System halted.\n", 0x0C);
        while(1) asm volatile("cli; hlt");
    }
}

static void add_to_history(const char* cmd) {
    if (cmd[0] == '\0') return;
    if (history_count < MAX_HISTORY) {
        str_copy(history[history_count++], cmd);
    } else {
        for (int i = 0; i < MAX_HISTORY - 1; i++) {
            str_copy(history[i], history[i + 1]);
        }
        str_copy(history[MAX_HISTORY - 1], cmd);
    }
}

static void clear_line() {
    extern void vga_putchar(char ch, unsigned char color);
    unsigned char col = (current_bg_color << 4) | current_text_color;
    for (int i = 0; i < cmd_len; i++) {
        vga_putchar('\b', col);
    }
}

static void load_history(int index) {
    if (index < 0 || index >= history_count) return;
    clear_line();
    str_copy(cmd_buffer, history[index]);
    cmd_len = str_len(cmd_buffer);
    extern void vga_putchar(char ch, unsigned char color);
    unsigned char col = (current_bg_color << 4) | current_text_color;
    for (int i = 0; i < cmd_len; i++) {
        vga_putchar(cmd_buffer[i], col);
    }
}

static void tab_complete() {
    if (cmd_len == 0) return;
    char prefix[64];
    int i;
    for (i = 0; i < cmd_len && i < 63; i++) {
        prefix[i] = cmd_buffer[i];
        if (prefix[i] >= 'a' && prefix[i] <= 'z') prefix[i] -= 32;
    }
    prefix[i] = '\0';
    const char* match = nullptr;
    int match_count = 0;
    for (int j = 0; j < num_commands; j++) {
        if (str_starts_with(commands[j], prefix)) {
            match = commands[j];
            match_count++;
        }
    }
    if (match_count == 1) {
        clear_line();
        str_copy(cmd_buffer, match);
        cmd_len = str_len(cmd_buffer);
        extern void vga_putchar(char ch, unsigned char color);
        unsigned char col = (current_bg_color << 4) | current_text_color;
        for (int j = 0; j < cmd_len; j++) {
            vga_putchar(cmd_buffer[j], col);
        }
    }
}

static void execute_single_command(const char* input) {
    int i = 0;
    while (input[i] == ' ') i++;
    if (input[i] == '\0') return;
    char cmd[64]; 
    int ci = 0;
    while (input[i] && input[i] != ' ' && ci < 63) cmd[ci++] = input[i++];
    cmd[ci] = '\0';
    while (input[i] == ' ') i++;
    const char* args = &input[i];
    unsigned char col = (current_bg_color << 4) | current_text_color;
    if (str_cmp(cmd, "HELP") == 0)       cmd_help();
    else if (str_cmp(cmd, "CLS") == 0)   cmd_cls();
    else if (str_cmp(cmd, "VER") == 0)   cmd_ver();
    else if (str_cmp(cmd, "ECHO") == 0)  cmd_echo(args);
    else if (str_cmp(cmd, "MEM") == 0)   cmd_mem();
    else if (str_cmp(cmd, "CPU") == 0)   cmd_cpu();
    else if (str_cmp(cmd, "DATE") == 0)  cmd_date();
    else if (str_cmp(cmd, "TIME") == 0)  cmd_time();
    else if (str_cmp(cmd, "COLOR") == 0) cmd_color(args);
    else if (str_cmp(cmd, "PAUSE") == 0) cmd_pause();
    else if (str_cmp(cmd, "REBOOT") == 0) cmd_reboot();
    else if (str_cmp(cmd, "SHUTDOWN") == 0) cmd_shutdown();
    else {
        shell_print("\n  Bad command or file name: ", 0x0C);
        shell_print(cmd, 0x0C);
        shell_print("\n  Type HELP for available commands.\n", col);
    }
}

static void execute_command() {
    int pipe_pos = -1;
    for (int i = 0; i < cmd_len; i++) {
        if (cmd_buffer[i] == '|') {
            pipe_pos = i;
            break;
        }
    }
    if (pipe_pos == -1) {
        execute_single_command(cmd_buffer);
    } else {
        char cmd1[256], cmd2[256];
        int i = 0;
        while (i < pipe_pos) {
            cmd1[i] = cmd_buffer[i];
            i++;
        }
        cmd1[i] = '\0';
        i = pipe_pos + 1;
        while (i < cmd_len && cmd_buffer[i] == ' ') i++;
        int j = 0;
        while (i < cmd_len) {
            cmd2[j++] = cmd_buffer[i++];
        }
        cmd2[j] = '\0';
        pipe_buffer_pos = 0;
        pipe_buffer[0] = '\0';
        capturing_output = true;
        execute_single_command(cmd1);
        capturing_output = false;
        unsigned char col = (current_bg_color << 4) | current_text_color;
        vga_print(pipe_buffer, col);
        execute_single_command(cmd2);
    }
}

void shell_init() {
    cmd_len = 0;
    history_count = 0;
    history_pos = -1;
    pipe_buffer_pos = 0;
    capturing_output = false;
    rtc_init();
}

void shell_run() {
    unsigned char col = (current_bg_color << 4) | current_text_color;
    vga_print("\n  Type HELP for available commands.\n", 0x0B);
    vga_print("\n  KOS> ", 0x0B);
    while (true) {
        char c = keyboard_getchar();
        if (c) {
            col = (current_bg_color << 4) | current_text_color;
            if (c == '\n') {
                cmd_buffer[cmd_len] = '\0';
                add_to_history(cmd_buffer);
                execute_command();
                cmd_len = 0;
                history_pos = -1;
                col = (current_bg_color << 4) | current_text_color;
                vga_print("\n  KOS> ", 0x0B);
            } else if (c == '\b') {
                if (cmd_len > 0) {
                    cmd_len--;
                    extern void vga_putchar(char ch, unsigned char color);
                    vga_putchar('\b', col);
                }
            } else if (c == '\t') {
                tab_complete();
            } else if (c == KEY_UP) {
                if (history_count > 0) {
                    if (history_pos == -1) {
                        history_pos = history_count - 1;
                    } else if (history_pos > 0) {
                        history_pos--;
                    }
                    load_history(history_pos);
                }
            } else if (c == KEY_DOWN) {
                if (history_pos != -1) {
                    if (history_pos < history_count - 1) {
                        history_pos++;
                        load_history(history_pos);
                    } else {
                        history_pos = -1;
                        clear_line();
                        cmd_len = 0;
                    }
                }
            } else {
                if (cmd_len < 255) {
                    cmd_buffer[cmd_len++] = c;
                    extern void vga_putchar(char ch, unsigned char color);
                    vga_putchar(c, col);
                }
            }
        }
        asm volatile("hlt");
    }
}
