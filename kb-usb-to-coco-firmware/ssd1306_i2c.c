#include "pico/stdlib.h"
#include "ssd1306_i2c.h"

void ssd1306_send_cmd (uint8_t cmd) {
	uint8_t buf[2] = {0x80, cmd};
	i2c_write_blocking(OLED_I2C, SSD1306_I2C_ADDR, buf, 2, false);
}

// Send a buffer of pixel data to the SSD1306
void ssd1306_send_buf (uint8_t buf[], int buflen) {
	// Note for later:  Maybe use the nostop flag and see if we can send the
	//   control byte separate from the buffer?
	uint8_t *b = malloc (buflen + 1);
	b[0] = 0x40;
	memcpy (b+1, buf, buflen);
	i2c_write_blocking (
		OLED_I2C, SSD1306_I2C_ADDR, b, buflen + 1, false
	);
	free (b);
}

// Initialize the ssd1306
void ssd1306_init() {
    uint8_t cmds[] = {
        SSD1306_SET_DISP,					// set display off
        /* memory mapping */
        SSD1306_SET_MEM_MODE,				// set memory address mode:
											//   0 = horizontal, 1 = vertical,
											//   2 = page
        0x00,								// horizontal addressing mode
        /* resolution and layout */
        SSD1306_SET_DISP_START_LINE,		// set display start line to 0
        SSD1306_SET_SEG_REMAP | 0x01,		// set segment re-map, column
											//   address 127 is mapped to SEG0
        SSD1306_SET_MUX_RATIO,				// set multiplex ratio
        SSD1306_HEIGHT - 1,					// Display height - 1
        SSD1306_SET_COM_OUT_DIR | 0x08,		// set COM (common) output scan
											//   direction. Scan from bottom up,
											//   COM[N-1] to COM0
        SSD1306_SET_DISP_OFFSET,			// set display offset
        0x00,								// no offset
        SSD1306_SET_COM_PIN_CFG,			// set COM (common) pins hardware
											//   configuration. Board specific
											//   magic number.  0x02 Works for
											//   128x32, 0x12 Possibly works
											//   for 128x64. Other options 0x22,
											//   0x32
		SSD1306_WIDTH == 128 ? (
			SSD1306_HEIGHT == 64 ? 0x12 : 0x02
		) : 0x02,
        /* timing and driving scheme */
        SSD1306_SET_DISP_CLK_DIV,			// set display clock divide ratio
        0x80,								// div ratio of 1, standard freq
        SSD1306_SET_PRECHARGE,				// set pre-charge period
        0xF1,								// Vcc internally generated on our
											//   board
        SSD1306_SET_VCOM_DESEL,				// set VCOMH deselect level
        0x30,								// 0.83xVcc
        /* display */
        SSD1306_SET_CONTRAST,				// set contrast control
        0xFF,
        SSD1306_SET_ENTIRE_ON,				// set entire display on to follow
											//   RAM content
        SSD1306_SET_NORM_DISP,				// set normal (not inverted) display
        SSD1306_SET_CHARGE_PUMP,			// set charge pump
        0x14,								// Vcc internally generated on our
											//   board
        SSD1306_SET_SCROLL | 0x00,			// deactivate horizontal scrolling
											//   if set. This is necessary as
											//   memory writes will corrupt if
											//   scrolling was enabled
        SSD1306_SET_DISP | 0x01,			// turn display on
    };
    ssd1306_send_cmd_list (cmds, count_of(cmds));
}

void ssd1306_render (uint8_t *buf, struct render_area *area) {
	uint8_t cmds[] = {
		SSD1306_SET_COL_ADDR,
		area->start_col,
		area->end_col,
		SSD1306_SET_PAGE_ADDR,
		area->start_page,
		area->end_page
	};
	ssd1306_send_cmd_list(cmds, count_of(cmds));
	ssd1306_send_buf(buf, area->buflen);
}