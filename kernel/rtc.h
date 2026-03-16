// ============================================================
//  KOS RTC — CMOS Real-Time Clock Driver
// ============================================================
#pragma once
#include <stdint.h>

struct DateTime {
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint16_t year;
};

void rtc_init();
void rtc_read_datetime(DateTime* dt);
