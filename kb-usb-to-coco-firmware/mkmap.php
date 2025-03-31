<?php
	$map = array();
	define ('ctrlkey', 0o64);
	define ('altkey', 0o63);
	define ('shiftkey', 0o67);
	define ('CTRL', 1);
	define ('SHIFT', 2);
	define ('ALT', 3);

	function defkey ($usb, $coco) {
		global $map;
		for ($j = 0; $j < 8; $j++) {
			$map[$usb][$j] = array();
			if (!! ($j & 1)) $map[$usb][$j][] = ctrlkey;
			if (!! ($j & 2)) $map[$usb][$j][] = shiftkey;
			if (!! ($j & 4)) $map[$usb][$j][] = altkey;
			$map[$usb][$j][] = $coco;
		}
	}

	function mapkey ($usb, $modstate, ...$cocos) {
		global $map;
		$map[$usb][$modstate] = array ();
<<<<<<< HEAD
		foreach ($cocos as $coco) $map[$usb][$modstate][] = $coco;
=======
		foreach ($cocos as $coco) $map [$usb][$modstate][] = $coco;
>>>>>>> next-video-take-2
	}
	function clearmap ($usb, $modstate) {
		global $map;
		unset ($map[$usb][$modstate]);
	}

	// This is the baseline map
	// - alphas
	for ($i = 0x04; $i <= 0x1d; $i++) defkey ($i, $i-3);
	// arrow keys
	defkey (0x4f, 0o36); // right
	defkey (0x50, 0o35); // left
	defkey (0x51, 0o34); // down
	defkey (0x52, 0o33); // up
	// - backspace (maps to left arrow)
	defkey (0x2a, 0o35);
	// - space
	defkey (0x2c, 0o37);
	// - numerics
	defkey (0x27, 0o40); // 0
	for ($i = 0x1e; $i <= 0x26; $i++) defkey ($i, $i+3); // 1–9
	// - semicolon
	defkey (0x33, 0o53);
	// - minus
	defkey (0x2d, 0o55);
	// - comma
	defkey (0x36, 0o54);
	// - period
	defkey (0x37, 0o56);
	// - forward slash
	defkey (0x38, 0o57);
	// - enter
	defkey (0x28, 0o60);
	// - home (maps to clear)
	defkey (0x4a, 0o61);
	// - esc (maps to break)
	defkey (0x29, 0o62);
	// - f-keys
	defkey (0x3a, 0o65); // f1
	defkey (0x3a, 0o66); // f2

	// The numeric keypad (doesn't do shifts)
	mapkey (0x62, 0, 0o40); // 0
	for ($i = 0x59; $i <= 0x61; $i++) mapkey ($i, 0, $i-0x38); // 1-9
	mapkey (0x55, 0, shiftkey, 0o52); // multiply
	mapkey (0x57, 0, shiftkey, 0o53); // add
	mapkey (0x56, 0, 0o55); // subtract
	mapkey (0x67, 0, shiftkey, 0o55); // equals
	mapkey (0x63, 0, 0o56); // period (decimal point)
	mapkey (0x54, 0, 0o57); // divide
	mapkey (0x58, 0, 0o60); // enter

	// Now for the special maps
	// - shifted 2 (maps to @)
	mapkey (0x1f, SHIFT, 0o00);
	// - shifted 6 (maps to ^ (up))
	mapkey (0x23, SHIFT, 0o33);
	// - shifted 7 (maps to & (shift-6))
	mapkey (0x24, SHIFT, shiftkey, 0o46);
	// - shifted 8 (maps to * (shift-colon))
	mapkey (0x25, SHIFT, shiftkey, 0o52);
	// - shifted 9 (maps to open paren (shift-8))
	mapkey (0x26, SHIFT, shiftkey, 0o50);
	// - shifted 0 (maps to close paren (shift-9))
	mapkey (0x27, SHIFT, shiftkey, 0o51);
	// - caps lock (maps to shift-0)
	mapkey (0x39, 0, shiftkey, 0o40);
	// - underscore (shifted minus) (maps to shift-up)
	mapkey (0x2d, SHIFT, shiftkey, 0o33);
	// - plus (shifted equals) (maps to shift-semicolon)
	mapkey (0x2e, SHIFT, shiftkey, 0o53);
	// - colon (shifted semicolon) (maps to colon)
	mapkey (0x33, SHIFT, 0o52);
	// - double quote (shifted apostrophe) (maps to shift-2)
	mapkey (0x34, SHIFT, shiftkey, 0o42);
	// - equal (maps to = (shift-minus))
	mapkey (0x2e, 0, shiftkey, 0o55);
	// - [ (maps to shift-down)
	mapkey (0x2f, 0, shiftkey, 0o34);
	// - ] (maps to shift-right)
	mapkey (0x30, 0, shiftkey, 0o36);
	// - apostrophe (maps to shift-7)
	mapkey (0x34, 0, shiftkey, 0o47);

	function showkey ($usb) {
		global $map;
		printf ("[%02x]\t", $usb);
		for ($j = 0; $j < 8; $j++) {
			printf ("{");
			for ($k = 0; $k < 4; $k++) printf (
				"% 3o%s",
				isset ($map[$usb][$j][$k]) ? $map[$usb][$j][$k] : 0o100,
				$k < 3 ? "," : ""
			);
			printf ("}%s%s", $j == 7 ? "\n" : ",", $j == 3 ? "\n\t" : "");
		}
	}

	for ($i = 0; $i < 256; $i++) {
		printf ("{\n\t");
		for ($j = 0; $j < 8; $j++) {
			printf ("{");
			for ($k = 0; $k < 4; $k++) printf (
				"0%03o%s",
				isset ($map[$i][$j][$k]) ? $map[$i][$j][$k] : 0o100,
				$k < 3 ? "," : ""
			);
			printf (
				"}%s%s",
				$j == 7 ? "\n" : ",",
				($j % 2) && $j < 7 ? "\n\t" : ""
			);
		}
		printf ("}%s", $i == 255 ? "" : ", ");
	}
?>