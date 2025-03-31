#ifndef ___RAW_MATRIX_H
#define ___RAW_MATRIX_H

#include "tusb.h"

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
static uint8_t raw_matrix_row_of[256];
static uint8_t raw_matrix_col_of[256];

<<<<<<< HEAD


=======
>>>>>>> next-video-take-2
#endif