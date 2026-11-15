// PicoBoy (RP2040, PicoBoy-BSP rev 2.0): ST7789 240x240, 8 onboard buttons
// (active high, external pull-downs), PWM speaker on GPIO 11.
//
// Video goes through hal/rp2040/video_picoboy.*: galagino renders 240 x 288
// strips, the backend squeezes them into the BSP's 240 x 240 frame buffer
// (5 of every 6 rows), then pb_display_flush() puts the picture on the panel.
// Audio goes through hal/rp2040/audio_pwm.cpp: galagino's fixed 24 kHz mono
// stream is resampled to the BSP's PWM carrier and handed to pb_audio_submit().
#ifndef HAL_BOARD_PICOBOY_H
#define HAL_BOARD_PICOBOY_H

#include "picoboy/board.h"

// backend axes: this board has a SPI panel and a PWM speaker. *CHOSEN keeps
// config.h from also applying its "no backend chosen" default.
#ifndef HAL_VIDEO_CHOSEN
#define HAL_VIDEO_CHOSEN
#endif
#ifndef HAL_AUDIO_CHOSEN
#define HAL_AUDIO_CHOSEN
#endif
#ifndef HAL_VIDEO_PICOBOY
#define HAL_VIDEO_PICOBOY
#endif
#ifndef HAL_AUDIO_PWM
#define HAL_AUDIO_PWM
#endif

// Buttons, from board.h: A=18, B=19, X=17, Y=16, UP=23, DOWN=20, LEFT=22,
// RIGHT=21. galagino needs START/COIN/FIRE plus the four directions, plan A
// uses the onboard set only:
//   FIRE = X, START = A, COIN = B, Y stays free
#ifndef BTN_START_PIN
#define BTN_START_PIN   PB_PIN_BTN_A
#endif
#ifndef BTN_COIN_PIN
#define BTN_COIN_PIN    PB_PIN_BTN_B
#endif
#ifndef BTN_FIRE_PIN
#define BTN_FIRE_PIN    PB_PIN_BTN_X
#endif
#ifndef BTN_UP_PIN
#define BTN_UP_PIN      PB_PIN_BTN_UP
#endif
#ifndef BTN_DOWN_PIN
#define BTN_DOWN_PIN    PB_PIN_BTN_DOWN
#endif
#ifndef BTN_LEFT_PIN
#define BTN_LEFT_PIN    PB_PIN_BTN_LEFT
#endif
#ifndef BTN_RIGHT_PIN
#define BTN_RIGHT_PIN   PB_PIN_BTN_RIGHT
#endif

// PWM speaker pin for the audio backend
#ifndef AUDIO_PWM_PIN
#define AUDIO_PWM_PIN   PB_PIN_AUDIO
#endif

// The board already runs at 250 MHz (board.h PB_SYS_CLOCK_KHZ); the HAL keeps
// it there and does not raise the core voltage.
#define GALAGINO_CLOCK_KHZ PB_SYS_CLOCK_KHZ

#endif // HAL_BOARD_PICOBOY_H
