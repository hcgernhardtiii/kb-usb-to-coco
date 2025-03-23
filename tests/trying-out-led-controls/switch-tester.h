#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "bsp/board_api.h"
#include "tusb.h"

#define USE_ANSI_ESCAPE   0
#define MAX_REPORT  4

// GPIO pins for MT8808 signals
#define MT_RESET_GPIO 2
#define MT_STROBE_GPIO 3

// GPIO masks for the MT8808
#define MT_RESET 0x004
#define MT_STROBE 0x008
#define MT_DATA 0x010
#define MT_ADDR 0x7E0
#define MT_ALL (MT_RESET|MT_STROBE|MT_DATA|MT_ADDR)
#define MT_DADDR (MT_DATA|MTADDR)

// Bit shift values for GPIO pin starts
#define MT_RESET_SHIFT 2
#define MT_STROBE_SHIFT 3
#define MT_DATA_SHIFT 4
#define MT_ROW_SHIFT 5
#define MT_COL_SHIFT 8

// Delay times for MT8808
#define MT_HOLD_T() asm volatile ("nop\n\tnop");
#define MT_STROBE_T() MT_HOLD_T() MT_HOLD_T()
#define MT_RESET_T() MT_STROBE_T() MT_STROBE_T()
#define MT_WAIT_T() MT_RESET_T() MT_RESET_T()

#define cls() printf ("\e[H\e[J")

static struct {
  uint8_t report_count;
  tuh_hid_report_info_t report_info[MAX_REPORT];
} hid_info[CFG_TUH_HID];

static uint8_t _report_count[CFG_TUH_HID];
static tuh_hid_report_info_t _report_info_arr[CFG_TUH_HID][MAX_REPORT];
static uint8_t the_kbd_addr;
static uint8_t the_kbd_inst;

// Sign of life
void led_blinking_task (void);

// Handle a keyboard report
void process_kbd_report (hid_keyboard_report_t const *report);