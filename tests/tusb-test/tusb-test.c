#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "bsp/board_api.h"
#include "tusb.h"

#define USE_ANSI_ESCAPE   0
#define MAX_REPORT  4

// Call counters
static uint8_t pcount=0;
static uint8_t ccount=0;
// The previous keyboard poll values
static uint8_t prev_poll[7] = {0,0,0,0,0,0,0};
// The current keyboard poll values
static uint8_t cur_poll[7] = {0,0,0,0,0,0,0};

static uint8_t raw_matrix [7][8] = {{
	HID_KEY_BRACKET_LEFT,
	HID_KEY_A,
	HID_KEY_B,
	HID_KEY_C,
	HID_KEY_D,
	HID_KEY_E,
	HID_KEY_F,
	HID_KEY_G
},{
	HID_KEY_H,
	HID_KEY_I,
	HID_KEY_J,
	HID_KEY_K,
	HID_KEY_L,
	HID_KEY_M,
	HID_KEY_N,
	HID_KEY_O
},{
	HID_KEY_P,
	HID_KEY_Q,
	HID_KEY_R,
	HID_KEY_S,
	HID_KEY_T,
	HID_KEY_U,
	HID_KEY_V,
	HID_KEY_W
},{
	HID_KEY_X,
	HID_KEY_Y,
	HID_KEY_Z,
	HID_KEY_ARROW_UP,
	HID_KEY_ARROW_DOWN,
	HID_KEY_ARROW_LEFT,
	HID_KEY_ARROW_RIGHT,
	HID_KEY_SPACE
},{
	HID_KEY_0,
	HID_KEY_1,
	HID_KEY_2,
	HID_KEY_3,
	HID_KEY_4,
	HID_KEY_5,
	HID_KEY_6,
	HID_KEY_7
},{
	HID_KEY_8,
	HID_KEY_9,
	HID_KEY_MINUS,
	HID_KEY_SEMICOLON,
	HID_KEY_COMMA,
	HID_KEY_EQUAL,
	HID_KEY_PERIOD,
	HID_KEY_SLASH
},{
	HID_KEY_ENTER,
	HID_KEY_HOME,
	HID_KEY_ESCAPE,
	HID_KEY_NONE,	// Will be alt
	HID_KEY_NONE,	// Will be ctrl
	HID_KEY_F1,
	HID_KEY_F2,
	HID_KEY_NONE	// Will be shift (either)
}};
static uint8_t matrix_row_of[256];
static uint8_t matrix_col_of[256];

static struct {
  uint8_t report_count;
  tuh_hid_report_info_t report_info[MAX_REPORT];
} hid_info[CFG_TUH_HID];

static bool screen_is_dirty = false;

void led_blinking_task (void);
static void process_kbd_report(hid_keyboard_report_t const *report);
static void cls();
static void putblock (int row, int col);
static void clrblock (int row, int col);

#define MT_RESET_GPIO 2
#define MT_STROBE_GPIO 3

#define MT_RESET 0x004
#define MT_STROBE 0x008
#define MT_DATA 0x010
#define MT_ADDR 0x7E0

#define MT_RESET_SHIFT 2
#define MT_STROBE_SHIFT 3
#define MT_DATA_SHIFT 4
#define MT_ROW_SHIFT 5
#define MT_COL_SHIFT 8

#define MT_HOLD_T() asm volatile ("nop\n\tnop");
#define MT_STROBE_T() MT_HOLD_T() MT_HOLD_T()
#define MT_RESET_T() MT_STROBE_T() MT_STROBE_T()
#define MT_WAIT_T() MT_RESET_T() MT_RESET_T()

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
		matrix_row_of[i] = 255;
		matrix_col_of[i] = 255;
	}
	matrix_row_of[HID_KEY_BACKSPACE] = 3;
	matrix_col_of[HID_KEY_BACKSPACE] = 5;
	printf ("Populating lookups…\n");
	for (i = 0; i < 7; i++) {
		for (j = 0; j < 8; j++) {
			matrix_row_of [raw_matrix[i][j]] = i;
			matrix_col_of [raw_matrix[i][j]] = j;
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
