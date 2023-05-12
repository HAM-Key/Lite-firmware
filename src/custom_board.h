
#ifndef _CUSTOM_BOARD_H_
#define _CUSTOM_BOARD_H_

#ifdef __cplusplus
extern "C" {
#endif
#include "nrf_gpio.h"

#define BUTTONS_NUMBER 1
#define LEDS_NUMBER    3

#define LED_0          1
#define LED_1          4
#define LED_2          16
#define LEDS_ACTIVE_STATE 0

#define BUTTON_0       6
#define BUTTON_PULL    NRF_GPIO_PIN_PULLDOWN
#define BUTTONS_ACTIVE_STATE 1

#define BUTTONS_LIST { BUTTON_0 }
#define LEDS_LIST { LED_0, LED_1, LED_2 }

#ifdef __cplusplus
}
#endif



#endif
