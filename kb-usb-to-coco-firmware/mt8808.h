#ifndef ___MT8808_H
#define ___MT8808_H

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

#endif // ___MT8808_H