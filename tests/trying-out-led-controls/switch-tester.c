#include "switch-tester.h"

int main()
{
	// Startup incantation
    stdio_init_all();
	board_init();
	tuh_init (BOARD_TUH_RHPORT);
	if (board_init_after_tusb) board_init_after_tusb();

	// Reverse UART0 TX sense to account for optoisolator
	gpio_set_outover (0, GPIO_OVERRIDE_INVERT);

	the_kbd_addr = 0xff;
	the_kbd_inst = 0;

	// Initialize the GPIOs
	cls();
	printf ("Initializing GPIOs\n");
	gpio_init_mask (MT_ALL);
	gpio_set_dir_out_masked (MT_ALL);
	gpio_clr_mask (MT_ALL);

	printf ("Waiting for reports…\n");

    while (true) {
        // This incantation sets up our callbacks for USB host
		tuh_task();
		// Make sure we have a sign of life
        led_blinking_task();
    }

}

// Mount the HID and start looking for keypresses
void tuh_hid_mount_cb (
	uint8_t dev_addr,
	uint8_t inst,
	uint8_t const *desc_report,
	uint16_t desc_len
) {
	uint8_t const itf_protocol = tuh_hid_interface_protocol (dev_addr, inst);
	// I don't know what this incantation does or if we even need it
	if (itf_protocol == HID_ITF_PROTOCOL_NONE) {
		hid_info[inst].report_count = tuh_hid_parse_report_descriptor (
			hid_info[inst].report_info,
			MAX_REPORT,
			desc_report,
			desc_len
		);
	}
	_report_count[inst] = tuh_hid_parse_report_descriptor(
		_report_info_arr[inst], MAX_REPORT, desc_report, desc_len
	);
	for (int i=0; i < _report_count[inst]; i++) if (
		(_report_info_arr[inst][i].usage_page == HID_USAGE_PAGE_DESKTOP) && 
        (_report_info_arr[inst][i].usage == HID_USAGE_DESKTOP_KEYBOARD)
	) {
		the_kbd_addr = dev_addr;
		the_kbd_inst = inst;
	}
	// Start receiving reports
	cls();
	printf ("HID device attached.\n");
	if (!tuh_hid_receive_report (dev_addr, inst)) {
		cls();
		printf ("We can't receive HID reports.  There's something wrong.\n");
	}
}

void tuh_hid_umount_cb (uint8_t dev_addr, uint8_t inst) {
	the_kbd_addr = 0xff;
}

// The HID report callback
void tuh_hid_report_received_cb (
	uint8_t dev_addr,
	uint8_t inst,
	uint8_t const* report,
	uint16_t len
) {
	uint8_t const itf_protocol = tuh_hid_interface_protocol (dev_addr, inst);
	switch (itf_protocol) {
		// Keyboard reports
		case HID_ITF_PROTOCOL_KEYBOARD:
			process_kbd_report ((hid_keyboard_report_t const*) report);
		break;
	}

	// Continue to request reports
	if (!tuh_hid_receive_report (dev_addr, inst)) {
		printf ("We can't receive HID reports.  There's something wrong.\n");
	}
}

// Process a keypress.  We're being quite restrictive in this regard.
void process_kbd_report (hid_keyboard_report_t const* report) {
	uint8_t i;
	uint8_t keycode;
	uint32_t the_num;
	bool have_num = false;
	static bool recording_macro = false;
	static uint32_t gpio_state = 0;
	// The row and column values we're entering
	static uint32_t row = 0, col = 0;
	// What we're going to be entering
	static enum {
		DOING_NOTHING,
		ENTERING_ROW,
		ENTERING_COL,
		CLEARING_ADDR,
		TOGGLING_STROBE,
		TOGGLING_DATA,
		TOGGLING_RESET,
	} the_state = DOING_NOTHING;

	// We only pay attention to the zeroth keypress.  If it's not there, we
	// ignore everything else.
	if (!(keycode = report->keycode[0])) return;

	// If we're here, we've got a key to process.
	cls();
	printf ("\e[H\e[1B\e[KCode: %d", report->keycode[0]);

	// Only a few of the keys have meaning.  Set up what we have and what we
	// need to do.
	switch (keycode) {
		case HID_KEY_0:
			keycode = HID_KEY_1 - 1;
		case HID_KEY_1:
		case HID_KEY_2:
		case HID_KEY_3:
		case HID_KEY_4:
		case HID_KEY_5:
		case HID_KEY_6:
		case HID_KEY_7:
			the_num = keycode - (HID_KEY_1 - 1);
			have_num = true;
			break;
		case HID_KEY_R:
			the_state = ENTERING_ROW;
			have_num = false;
			break;
		case HID_KEY_C:
			the_state = ENTERING_COL;
			have_num = false;
			break;
		case HID_KEY_ESCAPE:
			the_state = CLEARING_ADDR;
			break;
		case HID_KEY_D:
			the_state = TOGGLING_DATA;
			break;
		case HID_KEY_S:
			the_state = TOGGLING_STROBE;
			break;
		case HID_KEY_DELETE:
			the_state = TOGGLING_RESET;
			break;
	}

	// Now let's handle the states.
	switch (the_state) {
		case DOING_NOTHING:
			break;
		case ENTERING_ROW:
			printf ("\e[H\e[2B\e[KROW: ");
			if (have_num) {
				printf ("%d", the_num);
				gpio_state &= ~(0x07 << MT_ROW_SHIFT);
				gpio_state |= the_num << MT_ROW_SHIFT;
				the_state = DOING_NOTHING;
			}
			break;
		case ENTERING_COL:
			printf ("\e[H\e[2B\e[KCOL: ");
			if (have_num) {
				printf ("%d", the_num);
				gpio_state &= ~(0x07 << MT_COL_SHIFT);
				gpio_state |= the_num << MT_COL_SHIFT;
				the_state = DOING_NOTHING;
			}
			break;
		case CLEARING_ADDR:
			gpio_state &= (MT_RESET|MT_STROBE|MT_DATA);
			the_state = DOING_NOTHING;
			break;
		case TOGGLING_DATA:
			gpio_state ^= MT_DATA;
			the_state = DOING_NOTHING;
			break;
		case TOGGLING_STROBE:
			gpio_state ^= MT_STROBE;
			the_state = DOING_NOTHING;
			break;
		case TOGGLING_RESET:
			gpio_state &= MT_RESET;
			gpio_state ^= MT_RESET;
			the_state = DOING_NOTHING;
			break;
		default:
			break;
	}

	// Display the GPIO state
	printf (
		"\e[4;1f%s %s %s\e[5;1f\e[K%09b",
		(gpio_state & MT_DATA) ? "DATA" : "    ",
		(gpio_state & MT_STROBE) ? "STROBE" : "      ",
		(gpio_state & MT_RESET) ? "RESET" : "     ",
		(gpio_state >> 2)
	);

	// Ship the GPIO state
	gpio_put_masked (MT_ALL, gpio_state);
}

// The sign of life
void led_blinking_task (void) {
	static bool led_state = false;
	static uint8_t kb_leds = 0x00;
	static uint32_t start_ms = 0;
	const uint32_t interval_ms = 500;

	if (board_millis() - start_ms < interval_ms) return;
	start_ms += interval_ms;
	board_led_write (led_state);
	printf ("\e[20;1f\e[K%02x %02x", the_kbd_addr, the_kbd_inst);
	if (the_kbd_addr != 0xff) {
		printf ("\e[21;1f\e[K%02x", kb_leds);
		tuh_hid_set_report (
			the_kbd_addr,
			the_kbd_inst,
			0,
			HID_REPORT_TYPE_OUTPUT,
			&kb_leds,
			sizeof(kb_leds)
		);
		kb_leds = (kb_leds + 1) % 8;
	}

	led_state = 1 - led_state;
}
