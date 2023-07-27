
#ifndef _PLATFORM_H_
#define _PLATFORM_H_
#include <stdint.h>
#include <string.h>

#include "app_scheduler.h"
#define platform_simple_evt_put(handler) APP_ERROR_CHECK(app_sched_event_put(NULL,0,(app_sched_event_handler_t)handler))
#define platform_evt_put(data,size,handler) APP_ERROR_CHECK(app_sched_event_put(data,size,(app_sched_event_handler_t)handler))

#define LOG_INIT(x) NRF_LOG_MODULE_REGISTER();

#if NRF_LOG_ENABLED==1
	extern uint8_t log_head;
	#define LOG_HEX_RAW LOG_HEX_RAW_IMP
	#define LOG_RAW_CONTINUE(...) NRF_LOG_RAW_INFO(__VA_ARGS__)
	#define LOG_RAW(...) if(log_head==1){log_head=0;NRF_LOG_RAW_INFO("\r\n\t");}else{NRF_LOG_RAW_INFO("\t");}NRF_LOG_RAW_INFO(__VA_ARGS__)
	#define LOG_HEAD(...) log_head=1;NRF_LOG_RAW_INFO(__VA_ARGS__)
#else
	#define LOG_HEX_RAW(...)
	#define LOG_RAW_CONTINUE(...)
	#define LOG_RAW(...)
	#define LOG_HEAD(...)
#endif

void LOG_HEX_RAW_IMP(const uint8_t* array, uint16_t length);

#define LED_RED_PIN (1)
#define LED_BLUE_PIN (4)
#define LED_WHITE_PIN (16)
#define BUZZER_PIN (9)

#define SW1_PIN (15)
#define SW2_PIN (14)

#define BUTTON_PIN (6)
#define CHARGING_PIN (18)

#define BUTTON_POL (NRF_GPIOTE_POLARITY_LOTOHI)
#define BUTTON_PULL (nrf_gpio_pin_pull_t)(BUTTON_POL*2-1)
#define BUTTON_SENSE (nrf_gpio_pin_sense_t)(BUTTON_POL+1)

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

#include "uevent.h"

void platform_init(void);
void platform_scheduler(void);
void platform_reboot(void);
void platform_powerdown(bool flag);

void adc_config(void);
void adc_start(void);
// int16_t adc_get(uint8_t channel);

void buzzer_set(bool v);
void led_red_set(bool v);
void led_blue_set(bool v);
void led_white_set(bool v);

#define BUZZER_ON() nrf_gpio_pin_clear(BUZZER_PIN)
#define BUZZER_OFF() nrf_gpio_pin_set(BUZZER_PIN)

#define UEVT_SYS_BASE (0xAE00)
// 上电信号
#define UEVT_SYS_POWERUP (UEVT_SYS_BASE|0x01)
// 线性执行段
#define UEVT_SYS_SETUP (UEVT_SYS_BASE|0x02)
// 多任务开始
#define UEVT_SYS_START (UEVT_SYS_BASE|0x03)
// 准备休眠信号
#define UEVT_SYS_BEFORE_SLEEP (UEVT_SYS_BASE|0x0E)

#define UEVT_RTC_BASE (0x0000)
#define UEVT_RTC_8HZ (UEVT_RTC_BASE | 0x01)
#define UEVT_RTC_1HZ (UEVT_RTC_BASE | 0x02)
#define UEVT_RTC_NEWDAY (UEVT_RTC_BASE | 0x03)

#define UEVT_BTN_BASE (0x0100)

#define UEVT_ADC_BASE (0x0200)
#define UEVT_ADC_INIT (UEVT_ADC_BASE|0x01)
#define UEVT_ADC_NEWDATA (UEVT_ADC_BASE|0x02)
#define UEVT_ADC_NEWDATA_FL (UEVT_ADC_BASE|0x03)

#endif
