/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"

#ifndef LED_DELAY_MS
#define LED_DELAY_MS 500
#endif

#ifdef PICO_DEFAULT_I2C_SDA_PIN
#undef PICO_DEFAULT_I2C_SDA_PIN
#define PICO_DEFAULT_I2C_SDA_PIN 12
#endif

#ifdef PICO_DEFAULT_I2C_SCL_PIN
#undef PICO_DEFAULT_I2C_SCL_PIN
#define PICO_DEFAULT_I2C_SCL_PIN 13
#endif

#define OLED_I2C i2c_default

#define reserved_addr(addr) (((addr) & 0x78) == 0 || ((addr) & 0x78) == 0x78)

// Perform initialisation
int pico_led_init(void) {
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    return PICO_OK;
}

// Turn the led on or off
void pico_set_led(bool led_on) {
    gpio_put(PICO_DEFAULT_LED_PIN, led_on);
}

int main() {
	stdio_init_all();

	// Wait for 10 seconds to allow us to bring up `screen`
	for (int i = 0; i < 10; i++) {
		printf ("Boot wait loop…");
		sleep_ms (1000);
	}

	i2c_init (OLED_I2C, 100 * 1000);
	gpio_set_function (PICO_DEFAULT_I2C_SDA_PIN, GPIO_FUNC_I2C);
	gpio_pull_up  (PICO_DEFAULT_I2C_SDA_PIN);
	gpio_set_function (PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C);
	gpio_pull_up  (PICO_DEFAULT_I2C_SCL_PIN);

	printf ("\nI2C Bus Scan\n");
	printf ("   0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F\n");
	for (int addr = 0; addr < (1 << 7); ++addr) {
		if (addr % 16 == 0) printf ("%02x ", addr);
		int ret;
		uint8_t rxdata;
		if (reserved_addr (addr)) ret = PICO_ERROR_GENERIC;
		else ret = i2c_read_blocking (
			OLED_I2C, addr, &rxdata, 1, false
		);
		printf (ret < 0 ? "." : "@");
		printf (addr % 16 == 15 ? "\n" : "  ");
	}

    int rc = pico_led_init();
    hard_assert(rc == PICO_OK);
    while (true) {
        pico_set_led(true);
        sleep_ms(LED_DELAY_MS);
        pico_set_led(false);
        sleep_ms(LED_DELAY_MS);
    }
}
