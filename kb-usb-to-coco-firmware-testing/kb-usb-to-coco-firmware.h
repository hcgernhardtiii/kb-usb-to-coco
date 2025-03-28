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

#define MAX_REPORT 4

#define MATRIX_HAS(mtx, key) (!!(((mtx)[((key) & 0xf0) >> 4]) & (1 << ((key) & 0X0f))))
#define MATRIX_CLEAR(mtx, key) ((mtx)[((key) & 0xf0) >> 4] &= (~(1 << ((key) & 0x0f))))

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

// For mapped mode, we need to keep track of the shift state of a key when it
//   was pressed.  This needs to be global so we can clear it upon entering
//   mapped mode.
uint8_t mapped_mode_key_states[256];

void led_blinking_task (void);
static void process_kbd_report(hid_keyboard_report_t const *report);

// process the matrix in raw mode
static void raw_mode (hid_keyboard_report_t const *report);
// process the matrix in mapped mode
static void mapped_mode (
	hid_keyboard_report_t const* report,
	uint16_t cur_report[16],
	uint16_t presses[16],
	uint16_t releases[16]
);
// map a keypress.  We clear the key from the presses matrix to allow for
//   later processing of unmapped keys.
#define MAP_PRESS(key, row, col, state) \
	if (MATRIX_HAS (presses, (key))) { \
		mt8808_send ((row), (col), 1); \
		mapped_mode_key_states[(key)] = (state); \
		MATRIX_CLEAR (presses, (key)); \
	}
// resolve a mapped key conflict.  We check to see if the conflicting key
//   is currently pressed.  If it is, we send a key release and pause before
//   continuing on with the press.
#define MAP_CONFLICT(key, row, col, state) \
	if (mapped_mode_key_states[(key)] == (state)) { \
		mapped_mode_key_states[(key)] = 0; \
		mt8808_send ((row), (col), 0); \
		mt8808_pause(); \
	}

// Send a single keyswitch matrix operation to the mt8808
static inline void mt8808_send (int row, int col, int data);
// Insert a pause to allow for shift state toggling
static inline void mt8808_pause();
// Record a matrix switch operation
static inline void macro_record_key (int row, int col, int data);
// Record a keystroke pause
static inline void macro_record_pause();

// Visualize the “big” matrix—the raw keyboard scan itself
static void visualize_bigm (
	uint16_t cur[16],
	uint16_t presses[16],
	uint16_t releases[16]
);

#endif