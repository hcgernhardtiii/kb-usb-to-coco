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
	uint8_t i, j, press, release;
	uint8_t keycode;
	uint32_t pins;
	if (screen_is_dirty) cls();

	// Rotate the keypress data
	for (i = 0; i < 7; i++) {
		prev_poll[i] = cur_poll[i];	
		cur_poll[i] = 0;
	}
	// If we have a GUI key pressed, we'll be doing other processing later on
	if (!!(report->modifier & 0x88)) {
		// Do GUI key processing here
		return;
	}
	cls();
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
		if ((keycode = report->keycode[i]) && matrix_row_of[keycode] < 7) {
			cur_poll[matrix_row_of[keycode]] |= (1 << matrix_col_of[keycode]);
		}
	}
	// Calculate the key state changes
	for (i = 0; i < 7; i++) {
		release = prev_poll[i] & (~cur_poll[i]);
		press = (~prev_poll[i]) & cur_poll[i];
		printf (
			"\e[%d;1f%d v:%02x c:%02x p:%02x r:%02x",
			i+10, i, prev_poll[i], cur_poll[i], press, release
		);
		if (!!release) for (j = 0; j < 8; j++) {
			if (!!((release >> j) & 1)) clrblock (i, j);
		}
		if (!!press) for (j = 0; j < 8; j++) {
			if (!!((press >> j) & 1)) putblock (i, j);
		}
	}
}

static void putblock (int row, int col) {
	ccount = 0;
	pcount++;
	uint32_t pins =
		(uint32_t)row << MT_ROW_SHIFT |
		(uint32_t)col << MT_COL_SHIFT |
		1 << MT_DATA_SHIFT;
	row += 2; col += 2;
	printf ("\e[%d;%df▒", row, col);

	// Set up the data and address
	gpio_put_masked (
		MT_DATA | MT_ADDR,
		pins
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
	printf ("\e[24;1fp:%d c:%d", pcount, ccount);
}
static void clrblock (int row, int col) {
	pcount=0;
	ccount++;
	uint32_t pins =
		(uint32_t)row << MT_ROW_SHIFT |
		(uint32_t)col << MT_COL_SHIFT ;
	row += 2; col += 2;
	printf ("\e[%d;%df ", row, col);

	// Set up the data and address
	gpio_put_masked (
		MT_DATA | MT_ADDR,
		pins
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
	printf ("\e[24;1fp:%d c:%d", pcount, ccount);
}

static void cls() {
	// // Run the reset
	// gpio_put (MT_RESET_GPIO, 1);
	// MT_RESET_T();
	// gpio_put (MT_RESET_GPIO, 0);
	// MT_WAIT_T();

	printf ("\e[H\e[J 01234567\n0\n1\n2\n3\n4\n5\n6\e[24;1f");
	screen_is_dirty = false;
}
