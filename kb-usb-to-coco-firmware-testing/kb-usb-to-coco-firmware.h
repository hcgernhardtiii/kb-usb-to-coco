#ifndef ___KB_USB_TO_COCO_FIRMWARE_H
#define ___KB_USB_TO_COCO_FIRMWARE_H

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "bsp/board_api.h"
#include "tusb.h"
#include "mt8808.h"
#include "raw_matrix.h"

#define MAX_REPORT  4

// Call counters
static uint8_t pcount=0;
static uint8_t ccount=0;
// The previous keyboard poll values
static uint8_t prev_poll[7] = {0,0,0,0,0,0,0};
// The current keyboard poll values
static uint8_t cur_poll[7] = {0,0,0,0,0,0,0};

static struct {
  uint8_t report_count;
  tuh_hid_report_info_t report_info[MAX_REPORT];
} hid_info[CFG_TUH_HID];

static bool screen_is_dirty = false;

// Some mode flags we'll need:
// - <kbd>GUI</kbd> is our special function key.  If it's pressed we're talking
//   to the interface, not to the CoCo.
static bool gui_is_pressed = false;
// - Whether or not we're in mapped mode (we start in raw mode)
static bool mapped_mode_active = false;
// - Whether or not we're recording a macro (macro keys will be dealt with in
//   `process_kbd_report()`)
static bool recording_macro = false;

void led_blinking_task (void);
static void process_kbd_report(hid_keyboard_report_t const *report);
static void cls();
static void putblock (int row, int col);
static void clrblock (int row, int col);

#endif