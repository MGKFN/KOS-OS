#pragma once
#include <stdint.h>

#define KEY_UP    0x01
#define KEY_DOWN  0x02
#define KEY_LEFT  0x03
#define KEY_RIGHT 0x04

void keyboard_init();
char keyboard_getchar();

extern "C" void keyboard_handler();
