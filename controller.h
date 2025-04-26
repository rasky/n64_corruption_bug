#ifndef CONTROLLER_H
#define CONTROLLER_H

#define BTN_A      0x8000
#define BTN_B      0x4000
#define BTN_Z      0x2000
#define BTN_START  0x1000
#define BTN_DUP    0x0800
#define BTN_DDOWN  0x0400
#define BTN_DLEFT  0x0200
#define BTN_DRIGHT 0x0100
#define BTN_L      0x0020
#define BTN_R      0x0010
#define BTN_CUP    0x0008
#define BTN_CDOWN  0x0004
#define BTN_CLEFT  0x0002
#define BTN_CRIGHT 0x0001

uint16_t poll_controller();

#endif
