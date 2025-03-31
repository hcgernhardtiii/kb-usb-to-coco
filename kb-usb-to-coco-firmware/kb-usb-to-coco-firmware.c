#include "kb-usb-to-coco-firmware.h"

int main()
{
    stdio_init_all();
	board_init();
	tuh_init(BOARD_TUH_RHPORT);
	if (board_init_after_tusb) {
		board_init_after_tusb();
	}
	gpio_set_outover (0, GPIO_OVERRIDE_INVERT);


	// Initialize GPIOs
	printf ("Initializing GPIOs\n");
	gpio_init_mask (MT_RESET | MT_STROBE | MT_DATA | MT_ADDR);
	gpio_set_dir_out_masked (MT_RESET | MT_STROBE | MT_DATA | MT_ADDR);
	gpio_clr_mask (MT_RESET | MT_STROBE | MT_DATA | MT_ADDR);

	// Initialize the matrix row and column lookup tables
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
	screen_is_dirty = true;
	printf ("Waiting for reports…\n");

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
void tun_umount_cb (uint8_t dev_addr) {
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
	for (i = 0; i < 6; i++) {
		if (!!(keycode = report->keycode[i])) {
			cur_report[(keycode & 0xf0) >> 4] |= 1 << (keycode & 0x0f);
		}
	}
	// Walk through the matrixed reports and calculate the presses and releases.
	for (i = 0; i < 16; i++) {
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
		printf (
			"Checking for GUI key sequences %02x %02x %02x %02x %02x %02x [%02x]\n",
			report->keycode[0],
			report->keycode[1],
			report->keycode[2],
			report->keycode[3],
			report->keycode[4],
			report->keycode[5],
			report->modifier
		);
		if (
			!!(presses[(HID_KEY_Z & 0xf0) >> 4] & (1 << (HID_KEY_Z & 0x0f)))
		) {
			mapped_mode_active = !mapped_mode_active;
		}
		return;
	}
	if (mapped_mode_active) {
		mapped_mode (report, presses);
	} else {
		raw_mode (report);
	}
}

static void mapped_mode (
	hid_keyboard_report_t const* report,
	uint16_t presses[16]
) {
	printf (
		"Report recieved in mapped mode: %02x %02x %02x %02x %02x %02x [%02x]\n",
		report->keycode[0],
		report->keycode[1],
		report->keycode[2],
		report->keycode[3],
		report->keycode[4],
		report->keycode[5],
		report->modifier
	);
	// We'll have a maximum of six new presses in any given report
	int keypresses[6] = {0, 0, 0, 0, 0, 0};
	// We'll need to know the *current* status of our modifier bits.  We'll map
	//   this to an integer so we've got easy subscripting into our map array.
	int modstate = !!(report->modifier & 0x11) |
		!!(report->modifier & 0x22) << 1 |
		!!(report->modifier & 0x44) << 2;
	// Let's get the keypresses out
	int press = 0;
	for (int i = 0; i < 16; i++) if (!!presses[i]) {
		uint16_t the_presses = presses[i];
		for (int j = 0; j < 16; j++) {
			if (!!(the_presses & 1)) keypresses[press++] = (i << 4) | j;
			the_presses = the_presses >> 1;
		}
	}
	// Let's see what we have:
	for (int i = 0; i < 6; i++) {
		if (!keypresses[i]) continue;
		// Send the presses
		for (int j = 0; j < 4; j++) {
			if (ecb_map[keypresses[i]][modstate][j] & 0100) continue;
			mt8808_send (
				(ecb_map[keypresses[i]][modstate][j] >> 3) & 07,
				ecb_map[keypresses[i]][modstate][j] & 07,
				1
			);
		}
		// Wait for it…
		mt8808_pause();
		// Now send the release
		for (int j = 0; j < 4; j++) {
			if (ecb_map[keypresses[i]][modstate][j] & 0100) continue;
			mt8808_send (
				(ecb_map[keypresses[i]][modstate][j] >> 3) & 07,
				ecb_map[keypresses[i]][modstate][j] & 07,
				0
			);
		}
	}
}

static void raw_mode (hid_keyboard_report_t const* report) {
	printf (
		"Report recieved in raw mode: %02x %02x %02x %02x %02x %02x [%02x]\n",
		report->keycode[0],
		report->keycode[1],
		report->keycode[2],
		report->keycode[3],
		report->keycode[4],
		report->keycode[5],
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
			cur_poll[raw_matrix_row_of[keycode]] |= (1 << raw_matrix_col_of[keycode]);
		}
	}
	// Calculate the key state changes
	for (i = 0; i < 7; i++) {
		release = prev_poll[i] & (~cur_poll[i]);
		press = (~prev_poll[i]) & cur_poll[i];
		if (!!release) for (j = 0; j < 8; j++) {
			if (!!((release >> j) & 1)) mt8808_send (i, j, 0);
		}
		if (!!press) for (j = 0; j < 8; j++) {
			if (!!((press >> j) & 1)) mt8808_send (i, j, 1);
		}
	}
}

static inline void mt8808_send (int row, int col, int data) {
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

static inline void mt8808_pause() {
	sleep_ms (25);
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
		// The current report
		printf (
			"\e[%d;%df%s",
			row+3, col+2,
			!!(cur[row] & (1 << col)) ? "▒" : " "
		);
		printf (
			"\e[%d;%df%s",
			row+3, col+2+17,
			!!(presses[row] & (1 << col)) ? "▒" : " "
		);
		printf (
			"\e[%d;%df%s",
			row+3, col+2+17+17,
			!!(releases[row] & (1 << col)) ? "▒" : " "
		);
	}
}