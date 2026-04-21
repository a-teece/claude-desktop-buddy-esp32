// src/hw/pins.h
#pragma once

#if defined(BOARD_WAVESHARE_ESP32S3_TOUCH_AMOLED_1_8)
  #include "../boards/board_waveshare_esp32s3_touch_amoled_1_8.h"
#elif defined(BOARD_WAVESHARE_ESP32S3_TOUCH_AMOLED_1_75C)
  #include "../boards/board_waveshare_esp32s3_touch_amoled_1_75c.h"
#else
  #error "No board defined. Add -DBOARD_WAVESHARE_ESP32S3_TOUCH_AMOLED_1_8 or _1_75C to build_flags in platformio.ini."
#endif
