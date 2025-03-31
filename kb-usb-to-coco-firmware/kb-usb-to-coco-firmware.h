#ifndef ___KB_USB_TO_COCO_FIRMWARE_H
#define ___KB_USB_TO_COCO_FIRMWARE_H

#include <stdio.h>
#include "time.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "bsp/board_api.h"
#include "tusb.h"
#include "mt8808.h"
#include "raw_matrix.h"
#include "mapped_matrix.h"

#define MAX_REPORT 4

static struct {
<<<<<<< HEAD
  uint8_t report_count;
  tuh_hid_report_info_t report_info[MAX_REPORT];
=======
	uint8_t report_count;
	tuh_hid_report_info_t report_info[MAX_REPORT];
>>>>>>> next-video-take-2
} hid_info[CFG_TUH_HID];

static bool screen_is_dirty = false;

<<<<<<< HEAD
// Some mode flags we'll need:
// - <kbd>GUI</kbd> is our special function key.  If it's pressed we're talking
//   to the interface, not to the CoCo.
=======
// Some mode flags we'll need
// - <kbd>GUI</kbd> is our special function key.  If it's pressed we're talking
//   to the interface, not to the CoCo
>>>>>>> next-video-take-2
static bool gui_is_pressed = false;
// - Whether or not we're in mapped mode (we start in raw mode)
static bool mapped_mode_active = false;
// - Whether or not we're recording a macro (macro keys will be dealt with in
//   `process_kbd_report()`)
static bool recording_macro = false;

<<<<<<< HEAD
// Whether or not a keycode exists within a given matrix
#define MATRIX_HAS(mtx, key) ( \
	!!(((mtx)[((key) & 0xf0) >> 4]) & (1 << ((key) & 0x0f))) \
)

void led_blinking_task (void);
static void process_kbd_report(hid_keyboard_report_t const *report);
static void cls();
static void putblock (int row, int col);
static void clrblock (int row, int col);
static inline void mt8808_send (int row, int col, int data);
static inline void macro_record_send (int row, int col, int data) {}
static inline void mt8808_pause();
static inline void macro_record_pause() {}

// process the matrix in raw mode
static void raw_mode (hid_keyboard_report_t const *report);
// process the matrix in mapped mode
static void mapped_mode (
	hid_keyboard_report_t const *report,
	uint16_t presses[16]
);

// Visualize the “big” matrix—the raw keyboard scan itself
static void visualize_bigm (
=======
void led_blinking_task (void);
static void process_kbd_report(hid_keyboard_report_t const *report);
static void cls();
static void raw_mode (hid_keyboard_report_t const* report);
static void mapped_mode (
	hid_keyboard_report_t const* report,
	uint16_t presses[16]
);
static inline void mt8808_send (int row, int col, int data);
static inline void mt8808_pause();

static void visualize_bigm(
>>>>>>> next-video-take-2
	uint16_t cur[16],
	uint16_t presses[16],
	uint16_t releases[16]
);

#endif