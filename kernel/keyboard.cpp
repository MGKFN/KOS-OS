#include "keyboard.h"

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(uint16_t port, uint8_t val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

// Special keys
#define KEY_UP    0x01
#define KEY_DOWN  0x02
#define KEY_LEFT  0x03
#define KEY_RIGHT 0x04

static const char scancode_to_char[128] = {
    0,  0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*',
    0, ' '
};

static const char scancode_to_char_shift[128] = {
    0,  0, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*',
    0, ' '
};

static bool shift = false;
static bool caps = false;
static char last_key = 0;

void keyboard_init() {
    uint8_t mask = inb(0x21);
    mask &= ~(1 << 1);
    outb(0x21, mask);
}

extern "C" void keyboard_handler() {
    uint8_t sc = inb(0x60);
    
    if (sc == 0x2A || sc == 0x36) { shift = true; return; }
    if (sc == 0xAA || sc == 0xB6) { shift = false; return; }
    if (sc == 0x3A) { caps = !caps; return; }
    if (sc & 0x80) return;
    
    // Arrow keys
    if (sc == 0x48) { last_key = KEY_UP; return; }
    if (sc == 0x50) { last_key = KEY_DOWN; return; }
    if (sc == 0x4B) { last_key = KEY_LEFT; return; }
    if (sc == 0x4D) { last_key = KEY_RIGHT; return; }
    
    char c;
    if (shift) {
        c = scancode_to_char_shift[sc];
    } else {
        c = scancode_to_char[sc];
        if (caps && c >= 'a' && c <= 'z') {
            c -= 32;
        }
    }
    
    last_key = c;
}

char keyboard_getchar() {
    char c = last_key;
    last_key = 0;
    return c;
}
