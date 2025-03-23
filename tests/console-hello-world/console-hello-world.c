#include <stdio.h>
#include "pico/stdlib.h"

int color = 0;
int bold = 0;

int main()
{
    stdio_init_all();
	gpio_set_outover (0, GPIO_OVERRIDE_INVERT);
	// Our main program loop
    while (true) {
		color++;
		if (color == 9) {
			color = 1;
			bold = 1 - bold;
		}
		printf ("\e[H\e[J\e[%d;3%dmHello, World!\r\n", bold, color);
		sleep_ms (1000);
    }
}
