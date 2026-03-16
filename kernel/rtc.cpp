// ============================================================
//  KOS RTC Implementation — Read CMOS clock
// ============================================================
#include "rtc.h"

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1" :: "a"(val), "Nd"(port));
}

#define CMOS_ADDRESS 0x70
#define CMOS_DATA    0x71

static uint8_t cmos_read(uint8_t reg) {
    outb(CMOS_ADDRESS, reg);
    return inb(CMOS_DATA);
}

static uint8_t bcd_to_binary(uint8_t bcd) {
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

void rtc_init() {
    // Nothing to init — CMOS is always available
}

void rtc_read_datetime(DateTime* dt) {
    // Wait for update to complete
    while (cmos_read(0x0A) & 0x80);
    
    uint8_t sec   = cmos_read(0x00);
    uint8_t min   = cmos_read(0x02);
    uint8_t hour  = cmos_read(0x04);
    uint8_t day   = cmos_read(0x07);
    uint8_t month = cmos_read(0x08);
    uint8_t year  = cmos_read(0x09);
    
    uint8_t regB = cmos_read(0x0B);
    
    // Convert BCD to binary if needed
    if (!(regB & 0x04)) {
        sec   = bcd_to_binary(sec);
        min   = bcd_to_binary(min);
        hour  = bcd_to_binary(hour);
        day   = bcd_to_binary(day);
        month = bcd_to_binary(month);
        year  = bcd_to_binary(year);
    }
    
    // Convert 12-hour to 24-hour if needed
    if (!(regB & 0x02) && (hour & 0x80)) {
        hour = ((hour & 0x7F) + 12) % 24;
    }
    
    dt->second = sec;
    dt->minute = min;
    dt->hour   = hour;
    dt->day    = day;
    dt->month  = month;
    dt->year   = 2000 + year;
}
