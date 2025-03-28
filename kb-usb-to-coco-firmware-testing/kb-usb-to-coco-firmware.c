#include "kb-usb-to-coco-firmware.h"

int main()
{
	// initialize stdio, the board itself, and tinyUSB host
    stdio_init_all();
	board_init();
	tuh_init(BOARD_TUH_RHPORT);
	if (board_init_after_tusb) {
		board_init_after_tusb();
	}
	// Our optoisolator inverts the data signal
	gpio_set_outover (0, GPIO_OVERRIDE_INVERT);

	// Initialize GPIOs
	printf ("Initializing GPIOs\n");
	gpio_init_mask (MT_RESET | MT_STROBE | MT_DATA | MT_ADDR);
	gpio_set_dir_out_masked (MT_RESET | MT_STROBE | MT_DATA | MT_ADDR);
	gpio_clr_mask (MT_RESET | MT_STROBE | MT_DATA | MT_ADDR);

	// Initialize the raw matrix row and column lookup tables
	int i, j;
	printf ("Initializing lookups…\n");
	for (i = 0; i < 256; i++) {
		raw_matrix_row_of[i] = 255;
		raw_matrix_col_of[i] = 255;
	}
	raw_matrix_row_of[HID_KEY_BACKSPACE] = 3;
	raw_matrix_col_of[HID_KEY_BACKSPACE] = 5;
	printf ("Populating lookups…\n");
	for (i = 0; i < 7; i++) {
		for (j = 0; j < 8; j++) {
			raw_matrix_row_of [raw_matrix[i][j]] = i;
			raw_matrix_col_of [raw_matrix[i][j]] = j;
		}
	}

	// Start up the serial display
	screen_is_dirty = true;
	printf ("Waiting for reports…\n");

	// Start the poll 
    while (true) {
		tuh_task();
		led_blinking_task();
    }
}

void led_blinking_task (void) {
	static bool led_state = false;
	// static uint32_t pins = MT_RESET;
	static uint32_t start_ms = 0;
	const uint32_t interval_ms = 250;

	if (board_millis() - start_ms < interval_ms) return;
	start_ms += interval_ms;
	board_led_write (led_state);
	led_state = 1 - led_state;
	// printf ("%03x\n", pins);
	// gpio_put_masked (MT_RESET|MT_STROBE|MT_DATA|MT_ADDR, pins);
	// pins = (pins << 1) & (MT_RESET|MT_STROBE|MT_DATA|MT_ADDR);
	// if (pins == 0) pins = MT_RESET;
}

void tuh_mount_cb (uint8_t dev_addr) {
	screen_is_dirty = true;
	printf("A device with address %d is mounted\r\n", dev_addr);
}
void tuh_umount_cb (uint8_t dev_addr) {
	screen_is_dirty = true;
	printf("A device with address %d is unmounted\r\n", dev_addr);
}

void tuh_hid_mount_cb (
	uint8_t dev_addr,
	uint8_t instance,
	uint8_t const* desc_report,
	uint16_t desc_len
) {
	screen_is_dirty = true;
	printf("HID device address = %d, instance = %d is mounted\r\n", dev_addr, instance);

	// Interface protocol (hid_interface_protocol_enum_t)
	const char* protocol_str[] = { "None", "Keyboard", "Mouse" };
	uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);

	printf("HID Interface Protocol = %s\r\n", protocol_str[itf_protocol]);

	// By default host stack will use activate boot protocol on supported interface.
	// Therefore for this simple example, we only need to parse generic report descriptor (with built-in parser)
	if ( itf_protocol == HID_ITF_PROTOCOL_NONE )
	{
		hid_info[instance].report_count = tuh_hid_parse_report_descriptor(hid_info[instance].report_info, MAX_REPORT, desc_report, desc_len);
		printf("HID has %u reports \r\n", hid_info[instance].report_count);
	}

	// request to receive report
	// tuh_hid_report_received_cb() will be invoked when report is available
	if ( !tuh_hid_receive_report(dev_addr, instance) )
	{
		printf("Error: cannot request to receive report\r\n");
	}
}

void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
	screen_is_dirty = true;
	printf (
		"HID device address = %d, instance = %d is unmounted\r\n",
		dev_addr, instance
	);
}

void tuh_hid_report_received_cb(
	uint8_t dev_addr,
	uint8_t instance,
	uint8_t const* report,
	uint16_t len
) {
	uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);

	switch (itf_protocol)
	{
		case HID_ITF_PROTOCOL_KEYBOARD:
		TU_LOG2("HID receive boot keyboard report\r\n");
		process_kbd_report( (hid_keyboard_report_t const*) report );
		break;
	}

	// continue to request to receive report
	if ( !tuh_hid_receive_report(dev_addr, instance) )
	{
		printf("Error: cannot request to receive report\r\n");
	}
}

static void process_kbd_report (hid_keyboard_report_t const* report) {
	static uint16_t prev_report[16];
	static uint16_t cur_report[16];
	static uint16_t presses[16];
	static uint16_t releases[16];
	uint8_t i, j, keycode;

	// Rotate the report key data
	for (i = 0; i < 16; i++) {
		prev_report[i] = cur_report[i];	
		cur_report[i] = 0;
	}
	// Matrixify the modifier keys.  Blessedly, this maps directly to the
	//   HID keycodes, and they are the only keycodes at row 0xe
	if (!!(report->modifier)) {
		cur_report[0xe] = (uint16_t)report->modifier & 0x0f;
	}
	// Walk through the key reports and place them in the matrix.  The row is
	//   the high nybble of the keycode.
	for (i = 0; i < 6; i++) {
		if (!!(keycode = report->keycode[i])) {
			cur_report[(keycode & 0xf0) >> 4] |= 1 << (keycode & 0x0f);
		}
	}
	// Walk through the matrixed reports and calculate the presses and releases.
	for (i = 0; i < 15; i++) {
		releases[i] = prev_report[i] & (~cur_report[i]);
		presses[i] = (~prev_report[i]) & cur_report[i];
	}
	if (!!(presses[0xe] & 0x88)) {
		gui_is_pressed = true;
		printf ("GUI key is pressed\n");
	}
	if (!!(releases[0xe] & 0x88)) {
		printf ("GUI key is released\n");
		gui_is_pressed = false;
	}

	if (gui_is_pressed) {
		// check for GUI-Z, which toggles the mapped mode
		if (
			!!((presses[(HID_KEY_Z & 0xf0) >> 4]) & (1 << (HID_KEY_Z & 0X0f)))
		) {
			mapped_mode_active = !mapped_mode_active;
			// Clear the mapped mode key states buffer
			for (int i = 0; i < 256; i++) mapped_mode_key_states[i] = 0;
		}
		return;
	}
	if (recording_macro) {
		// call the macro key add here
	}
	// check for macro playback keypress here (macro should record raw/mapped
	//   state)
	if (mapped_mode_active) {
		mapped_mode (report);
	} else {
		raw_mode (report);
	}


	// visualize_bigm (cur_report, presses, releases);
}

static void raw_mode (hid_keyboard_report_t const* report) {
	printf (
		"Report received in raw mode: %02x %02x %02x %02x %02x %02x %02x [%02x]\n",
		report->keycode[0],
		report->keycode[1],
		report->keycode[2],
		report->keycode[3],
		report->keycode[4],
		report->keycode[5],
		report->keycode[6],
		report->modifier
	);
	// The previous keyboard poll values
	static uint8_t prev_poll[7] = {0,0,0,0,0,0,0};
	// The current keyboard poll values
	static uint8_t cur_poll[7] = {0,0,0,0,0,0,0};
	uint8_t i, j, press, release;
	uint8_t keycode;
	uint32_t pins;

	// Rotate the keypress data
	for (i = 0; i < 7; i++) {
		prev_poll[i] = cur_poll[i];	
		cur_poll[i] = 0;
	}
	// Handle the modifier keys
	if (!!(report->modifier & 0x44)) {
		// Alt
		cur_poll[6] |= 0x08;
	}
	if (!!(report->modifier & 0x22)) {
		// Shift
		cur_poll[6] |= 0x80;
	}
	if (!!(report->modifier & 0x11)) {
		// Ctrl
		cur_poll[6] |= 0x10;
	}
	// Handle all the other keys
	for (i = 0; i < 6; i++) {
		if ((keycode = report->keycode[i]) && raw_matrix_row_of[keycode] < 7) {
			cur_poll[raw_matrix_row_of[keycode]] |=
				(1 << raw_matrix_col_of[keycode]);
		}
	}
	// Calculate the key state changes
	for (i = 0; i < 7; i++) {
		release = prev_poll[i] & (~cur_poll[i]);
		press = (~prev_poll[i]) & cur_poll[i];
		if (!!release) for (j = 0; j < 8; j++) if (!!((release >> j) & 1)) {
			mt8808_send (i, j, 0);
		}
		if (!!press) for (j = 0; j < 8; j++) if (!!((press >> j) & 1)) {
			mt8808_send (i, j, 1);
		}
	}
	macro_record_pause();
}

static void mapped_mode (
	hid_keyboard_report_t const* report,
	uint16_t cur_report[16],
	uint16_t presses[16],
	uint16_t releases[16]
) {
	bool is_shifted = !!(report->modifier & 0x22);
	if (is_shifted && (
		MATRIX_HAS (presses, HID_KEY_2) ||
		!!(presses[0x2] & 0x60f8) ||
		!!(presses[0x3] & 0x0018) ||
		!!MATRIX_HAS (presses, HID_KEY_ARROW_RIGHT) ||
		!!(presses[0x5] & 0xfff6) ||
		!!(presses[0x6] & 0x000f)
	)) {
		// First let's check for our shift toggle specials
		if (
			MATRIX_HAS (presses, HID_KEY_2) ||
			MATRIX_HAS (presses, HID_KEY_6) ||
			MATRIX_HAS (presses, HID_KEY_SEMICOLON)
		) {
			// These specials need to be unshifted
			mt8808_pause();
			mt8808_send (6, 7, 0);
			// - `HID_KEY_2` for `@`
			MAP_PRESS (HID_KEY_2, 4, 2, 0x3);
			// - `HID_KEY_6` for `^`
			MAP_PRESS (HID_KEY_6, 4, 6, 0x3);
			// - `HID_KEY_SEMICOLON` for `:`
			MAP_PRESS (HID_KEY_SEMICOLON, 5, 2, 0x3);
			mt8808_pause();
			// Restore the shift state
			mt8808_send (6, 7, 1);
		}
		// Now we go through our other specials:
		if (!!(presses[0x2] && 0x60f0)) {
			// These are the keypresses we translate
			// - `HID_KEY_7` for `&` 
			MAP_PRESS (HID_KEY_7, 4, 6, 0x3);
			// - `HID_KEY_8` for `*` (conflicts with HID_KEY_SEMICOLON)
			MAP_CONFLICT (HID_KEY_SEMICOLON, 5, 2, 0x3);
			MAP_PRESS (HID_KEY_8, 5, 2, 0x3);
			// - `HID_KEY_9` for `(` 
			MAP_PRESS (HID_KEY_9, 5, 1, 0x3);
			// - `HID_KEY_0` for `)` 
			MAP_PRESS (HID_KEY_0, 4, 0, 0x3);
			// - `HID_KEY_MINUS` for `_` (renders as a left arrow)
			MAP_PRESS (HID_KEY_MINUS, 5, 5, 0x3);
			// - `HID_KEY_EQUAL` for `+`
			MAP_PRESS (HID_KEY_EQUAL, 5, 3, 0x3);
		}
		if (MATRIX_HAS (presses, HID_KEY_APOSTROPHE)) {
			// - `HID_KEY_APOSTROPHE` for `"`
			MAP_PRESS (HID_KEY_APOSTROPHE, 4, 7, 0x3);
		}
		// And now for the ones we ignore:
		MAP_IGNORE (HID_KEY_ARROW_RIGHT, 0x03);
		if (!!(presses[0x5] & 0xfff6)) {
			MAP_IGNORE (HID_KEY_ARROW_DOWN, 0x03);
			MAP_IGNORE (HID_KEY_ARROW_UP, 0x03);
			MAP_IGNORE (HID_KEY_KEYPAD_DIVIDE, 0x03);
			MAP_IGNORE (HID_KEY_KEYPAD_MULTIPLY, 0x03);
			MAP_IGNORE (HID_KEY_KEYPAD_SUBTRACT, 0x03);
			MAP_IGNORE (HID_KEY_KEYPAD_ADD, 0x03);
			MAP_IGNORE (HID_KEY_KEYPAD_ENTER, 0x03);
			MAP_IGNORE (HID_KEY_KEYPAD_1, 0x03);
			MAP_IGNORE (HID_KEY_KEYPAD_2, 0x03);
			MAP_IGNORE (HID_KEY_KEYPAD_3, 0x03);
			MAP_IGNORE (HID_KEY_KEYPAD_4, 0x03);
			MAP_IGNORE (HID_KEY_KEYPAD_5, 0x03);
			MAP_IGNORE (HID_KEY_KEYPAD_6, 0x03);
			MAP_IGNORE (HID_KEY_KEYPAD_7, 0x03);
		}
		if (!!(presses[0x6] & 0x000f)) {
			MAP_IGNORE (HID_KEY_KEYPAD_8, 0X03)
			MAP_IGNORE (HID_KEY_KEYPAD_9, 0X03)
			MAP_IGNORE (HID_KEY_KEYPAD_0, 0X03)
			MAP_IGNORE (HID_KEY_KEYPAD_DECIMAL, 0X03)
		}
	}
	if (!is_shifted && (
		!!(presses[0x2] & 0xe000) ||
		!!(presses[0x3] & 0x0011) ||
		!!(presses[0x5] & 0xfff0) ||
		!!(presses[0x6] & 0x000f)
	)) {
		// First let's check for our shift toggle specials
		if (
			!!(presses[0x2] & 0xc000) ||
			!!(presses[0x3] & 0x0011) ||
			!!(presses[0x5] & 0x00a0)
		) {
			// These specials need to be unshifted
			mt8808_pause();
			mt8808_send (6, 7, 1);
			// - `HID_KEY_EQUAL` for `=` (shifted)
			MAP_PRESS (HID_KEY_EQUAL, 5, 5, 0x1);
			// - `HID_KEY_BRACKET_LEFT` for `[` (shifted)(down arrow)
			MAP_PRESS (HID_KEY_BRACKET_LEFT, 3, 4, 0x1);
			// - `HID_KEY_BRACKET_RIGHT` for `]` (shifted)(right arrow)
			MAP_PRESS (HID_KEY_BRACKET_RIGHT, 3, 6, 0x1);
			// - `HID_KEY_APOSTROPHE` for `'` (shifted)
			MAP_PRESS (HID_KEY_APOSTROPHE, 4, 7, 0x1);
			// - `HID_KEY_KEYPAD_MULTIPLY` for `*` (shifted)
			MAP_PRESS (HID_KEY_KEYPAD_MULTIPLY, 5, 2, 0x1);
			// - `HID_KEY_KEYPAD_ADD` for `+` (shifted)
			MAP_PRESS (HID_KEY_KEYPAD_ADD, 5, 3, 0x1);
			mt8808_pause();
			// Restore the shift state
			mt8808_send (6, 7, 0);
		}
		// Now we go through our other specials:
		if (MATRIX_HAS (presses, HID_KEY_MINUS)) {
			// - `HID_KEY_MINUS` for `-`
			MAP_CONFLICT (HID_KEY_EQUAL, 5, 5, 0x1);
			MAP_PRESS (HID_KEY_MINUS, 5, 5, 0x1);
		}
		if (!!(presses[0x5] & 0xff50)) {
			// - `HID_KEY_KEYPAD_DIVIDE` for `/`
			MAP_PRESS (HID_KEY_KEYPAD_DIVIDE, 5, 7, 0x1)
			// - `HID_KEY_KEYPAD_SUBTRACT` for `-`
			MAP_CONFLICT (HID_KEY_EQUAL, 5, 5, 0x1);
			MAP_PRESS (HID_KEY_KEYPAD_SUBTRACT, 5, 5, 0x1);
			// - `HID_KEY_KEYPAD_ENTER` for <kbd>enter</kbd>
			MAP_PRESS (HID_KEY_KEYPAD_ENTER, 6, 0, 0x1);
			// - `HID_KEY_KEYPAD_1` for `1`
			MAP_PRESS (HID_KEY_KEYPAD_1, 4, 1, 0x1);
			// - `HID_KEY_KEYPAD_2` for `2`
			MAP_PRESS (HID_KEY_KEYPAD_2, 4, 2, 0x1);
			// - `HID_KEY_KEYPAD_3` for `3`
			MAP_PRESS (HID_KEY_KEYPAD_3, 4, 3, 0x1);
			// - `HID_KEY_KEYPAD_4` for `4`
			MAP_PRESS (HID_KEY_KEYPAD_4, 4, 4, 0x1);
			// - `HID_KEY_KEYPAD_5` for `5`
			MAP_PRESS (HID_KEY_KEYPAD_5, 4, 5, 0x1);
			// - `HID_KEY_KEYPAD_6` for `6`
			MAP_PRESS (HID_KEY_KEYPAD_6, 4, 6, 0x1);
			// - `HID_KEY_KEYPAD_7` for `7`
			MAP_PRESS (HID_KEY_KEYPAD_7, 4, 7, 0x1);
		}
		if (!!(presses[0x6] & 0x000f)) {
			// - `HID_KEY_KEYPAD_8` for `8`
			MAP_PRESS (HID_KEY_KEYPAD_8, 5, 0, 0x1);
			// - `HID_KEY_KEYPAD_9` for `9`
			MAP_PRESS (HID_KEY_KEYPAD_9, 5, 1, 0x1);
			// - `HID_KEY_KEYPAD_0` for `0`
			MAP_PRESS (HID_KEY_KEYPAD_0, 4, 0, 0x1);
			// - `HID_KEY_KEYPAD_DECIMAL` for `.`
			MAP_PRESS (HID_KEY_KEYPAD_DECIMAL, 5, 6, 0x1);
		}
	}
	macro_record_pause();
}

inline static __attribute__((always_inline)) void mt8808_send (
	int row, int col, int data
) {
	// Save the switch operation if we're recording a macro
	if (recording_macro) macro_record_key (row, col, data);
	// Set up the data and address
	gpio_put_masked (
		MT_DATA | MT_ADDR,
		(uint32_t)row << MT_ROW_SHIFT |
		(uint32_t)col << MT_COL_SHIFT |
		(uint32_t)data << MT_DATA_SHIFT
	);
	MT_HOLD_T();
	// Strobe the data and address
	gpio_put (MT_STROBE_GPIO, 1);
	MT_STROBE_T();
	gpio_put (MT_STROBE_GPIO, 0);
	MT_HOLD_T();
	// Release the data and address pins
	gpio_clr_mask (MT_DATA | MT_ADDR);
	MT_WAIT_T();
}

inline static __attribute__((always_inline)) void mt8808_pause() {
	if (recording_macro) macro_record_pause();
	sleep_ms (17);
}

inline static __attribute__((always_inline)) void macro_record_key (
	int row, int col, int data
) {

}

inline static __attribute__((always_inline)) void macro_record_pause() {

}

static void visualize_bigm (
	uint16_t cur[16],
	uint16_t presses[16],
	uint16_t releases[16]
) {
	static bool do_setup = true;
	if (do_setup) {
		printf (
			"\e[H\e[J     CURRENT          PRESSED          RELEASED\n 0123456789ABCDEF 0123456789ABCDEF 0123456789ABCDEF\n0\n1\n2\n3\n4\n5\n6\n7\n8\n9\nA\nB\nC\nD\nE\nF\e[24;1f"
		);
		do_setup = false;
	}
	for (int row = 0; row < 16; row++) for (int col = 0; col < 16; col++) {
		// the current report
		printf (
			"\e[%d;%df%s",
			row+3, col+2,
			!!(cur[row] & (1 << col)) ? "▒" : " "
		);
		// the presses for this report
		printf (
			"\e[%d;%df%s",
			row+3, col+2+17,
			!!(presses[row] & (1 << col)) ? "▒" : " "
		);
		// the releases for this report
		printf (
			"\e[%d;%df%s",
			row+3, col+2+17+17,
			!!(releases[row] & (1 << col)) ? "▒" : " "
		);
	}
}

