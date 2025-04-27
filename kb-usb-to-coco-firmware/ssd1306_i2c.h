#ifndef _SSD1306_I2C_H
#define _SSD1306_I2C_H

// Get our SSD1306 command defines
#include "ssd1306_cmds.h"
#include "hardware/i2c.h"

// Define our SSD1306 parameters
#define SSD1306_PAGE_HEIGHT			_u(8)
#define SSD1306_HEIGHT				_u(64)
#define SSD1306_WIDTH				_u(128)
#define SSD1306_NUM_PAGES           (SSD1306_HEIGHT / SSD1306_PAGE_HEIGHT)
#define SSD1306_BUF_LEN             (SSD1306_NUM_PAGES * SSD1306_WIDTH)
#define SSD1306_I2C_ADDR			_u(0x3c)
#define SSD1306_I2C_CLK				400

// Redefine the I2C pins
#ifdef PICO_DEFAULT_I2C_SDA_PIN
#undef PICO_DEFAULT_I2C_SDA_PIN
#define PICO_DEFAULT_I2C_SDA_PIN 12
#endif

#ifdef PICO_DEFAULT_I2C_SCL_PIN
#undef PICO_DEFAULT_I2C_SCL_PIN
#define PICO_DEFAULT_I2C_SCL_PIN 13
#endif

// We won't count on `i2c_default` being I2C0
#define OLED_I2C I2C_INSTANCE(0)

struct render_area {
	// Start and end columns for the SSD1306 (command 0x21)
    uint8_t start_col;
    uint8_t end_col;
	// Start and end pages (each page is 8 rows) for the SSD1306 (command 0x22)
    uint8_t start_page;
    uint8_t end_page;
	// The byte length of the buffer this will be stored in (calculated)
    int buflen;
};

// Calculate the byte size of a render area
void inline calc_render_area_buflen (struct render_area *area) {
	area->buflen =
		(area->end_col - area->start_col + 1) *
		(area->end_page - area->start_page + 1);
}

void ssd1306_send_cmd (uint8_t cmd);
void inline ssd1306_send_cmd_list (uint8_t *buf, int num) {
	for (int i = 0; i < num; i++) ssd1306_send_cmd (buf[i]);
}

void ssd1306_send_buf (uint8_t buf[], int buflen);

void ssd1306_init();

void ssd1306_render (uint8_t *buf, struct render_area *area);


#endif // _SSD1306_I2C_H