
#define NRF_LOG_MODULE_NAME platform
#include "nrf_log.h"
NRF_LOG_MODULE_REGISTER();

#include "platform.h"
#include "nordic_common.h"
#include "nrf.h"
#include "nrf_assert.h"
#include "app_error.h"

#include "app_button.h"
#include "app_timer.h"

#include "nrf_drv_rtc.h"
#include "nrf_drv_clock.h"
#include "nrf_pwr_mgmt.h"

#include "app_scheduler.h"

#include "nrf_delay.h"
#include "nrf_gpio.h"
#include "nrf_gpiote.h"
#include "nrf_drv_gpiote.h"
#include "nrf_drv_pwm.h"
#include "nrf_drv_timer.h"
#include "nrf_drv_spi.h"
#include "nrf_drv_twi.h"
#include "nrfx_saadc.h"

#include "nrf_queue.h"
#include "nrf_drv_rng.h"
#include "nrfx_rng.h"

#include "bluetooth.h"

#include <stdio.h>
uint8_t log_head = 0;
void LOG_HEX_RAW_IMP(const uint8_t* array, uint16_t length) {
	static char buffer[4][193];
	static uint8_t flag = 0;
	const uint8_t* pa = array;
	char* pb;
	uint8_t len;	// 当次处理长度
	while(length > 0) {
		pb = buffer[flag];
		if(length > 64) {
			len = 64;
		} else {
			len = length;
		}
		for (uint8_t i = 0; i < len; i++) {
			sprintf(pb, "%02X ", *pa);
			pb += 3;
			pa += 1;
		}
		length -= len;
		*pb = 0;
		LOG_RAW_CONTINUE("%s\r\n", buffer[flag]);
		flag = (flag + 1) & 0x3;
	}
}


#define USE_FPU
#ifdef USE_FPU
#define FPU_EXCEPTION_MASK 0x0000009F
void FPU_IRQHandler(void) {
	uint32_t* fpscr = (uint32_t*)(FPU->FPCAR + 0x40);
	(void)__get_FPSCR();
	*fpscr = *fpscr & ~(FPU_EXCEPTION_MASK);
}
#endif

void delay_ms(uint16_t ms) {
	nrf_delay_ms(ms);
}

static void app_timer_config(void) {
	app_timer_init();
}

static void log_init(void) {
	ret_code_t err_code = NRF_LOG_INIT(NULL);
	APP_ERROR_CHECK(err_code);
	NRF_LOG_DEFAULT_BACKENDS_INIT();
}

static void power_management_init(void) {
	ret_code_t err_code;
	err_code = nrf_pwr_mgmt_init();
	APP_ERROR_CHECK(err_code);
}

#define REG_POWER_BASE_ADDR	0x40000000
#define RESET_REASON_OFFSET	0x400

#define UICR_BASE_ADDR	0x10001000
#define NFC_CONFIG_OFFSET	0x20C
uint32_t read_chip_reset_reason(void) {
	return *(uint32_t*)(REG_POWER_BASE_ADDR + RESET_REASON_OFFSET);
}

uint32_t read_uicr_nfc_config(void) {
	return *(uint32_t*)(UICR_BASE_ADDR + NFC_CONFIG_OFFSET);
}

void leds_config(void) {
	nrf_gpio_cfg_output(LED_RED_PIN);
	nrf_gpio_pin_set(LED_RED_PIN);
	nrf_gpio_cfg_output(LED_BLUE_PIN);
	nrf_gpio_pin_set(LED_BLUE_PIN);
	nrf_gpio_cfg_output(LED_WHITE_PIN);
	nrf_gpio_pin_set(LED_WHITE_PIN);

	nrf_gpio_cfg(BUZZER_PIN,
				 NRF_GPIO_PIN_DIR_OUTPUT,
				 NRF_GPIO_PIN_INPUT_DISCONNECT,
				 NRF_GPIO_PIN_NOPULL,
				 NRF_GPIO_PIN_H0D1,
				 NRF_GPIO_PIN_NOSENSE);
	nrf_gpio_pin_set(BUZZER_PIN);
}
void buzzer_set(bool v) {
	if(v) {
		nrf_gpio_pin_clear(BUZZER_PIN);
	} else {
		nrf_gpio_pin_set(BUZZER_PIN);
	}
}
void led_red_set(bool v) {
	if(v) {
		nrf_gpio_pin_clear(LED_RED_PIN);
	} else {
		nrf_gpio_pin_set(LED_RED_PIN);
	}
}
void led_blue_set(bool v) {
	if(v) {
		nrf_gpio_pin_clear(LED_BLUE_PIN);
	} else {
		nrf_gpio_pin_set(LED_BLUE_PIN);
	}
}
void led_white_set(bool v) {
	if(v) {
		nrf_gpio_pin_clear(LED_WHITE_PIN);
	} else {
		nrf_gpio_pin_set(LED_WHITE_PIN);
	}
}

void switch_btn_config(void) {
	nrf_gpio_cfg(SW1_PIN,
				 NRF_GPIO_PIN_DIR_INPUT,
				 NRF_GPIO_PIN_INPUT_CONNECT,
				 NRF_GPIO_PIN_NOPULL,
				 NRF_GPIO_PIN_S0S1,
				 NRF_GPIO_PIN_NOSENSE);
	nrf_gpio_cfg(SW2_PIN,
				 NRF_GPIO_PIN_DIR_INPUT,
				 NRF_GPIO_PIN_INPUT_CONNECT,
				 NRF_GPIO_PIN_NOPULL,
				 NRF_GPIO_PIN_S0S1,
				 NRF_GPIO_PIN_NOSENSE);
	nrf_gpio_cfg(BUTTON_PIN,
				 NRF_GPIO_PIN_DIR_INPUT,
				 NRF_GPIO_PIN_INPUT_CONNECT,
				 BUTTON_PULL,
				 NRF_GPIO_PIN_S0S1,
				 NRF_GPIO_PIN_NOSENSE);
}

static nrf_saadc_value_t adc_buffer[8];
static void saadc_event_handler(nrfx_saadc_evt_t const* p_evt) {
	if (p_evt->type == NRFX_SAADC_EVT_DONE) {
		NRF_LOG_INFO("ADC = %d", p_evt->data.done.p_buffer[0]);
		// volt = (adc*0.6/0.5/2048) + 3.3 v
	}
}

void adc_config(void) {
	uint32_t err_code;

	nrfx_saadc_config_t saadc_config = {
		.resolution         = (nrf_saadc_resolution_t)NRF_SAADC_RESOLUTION_12BIT,
		.oversample         = (nrf_saadc_oversample_t)NRF_SAADC_OVERSAMPLE_DISABLED,
		.interrupt_priority = NRFX_SAADC_CONFIG_IRQ_PRIORITY,
		.low_power_mode     = 0
	};

	err_code = nrfx_saadc_init(&saadc_config, saadc_event_handler);
	APP_ERROR_CHECK(err_code);

	// 差分电池电压检测145 = 4.2v, 460 = 3.7v
	nrf_saadc_channel_config_t config = {
		.resistor_p = NRF_SAADC_RESISTOR_DISABLED,
		.resistor_n = NRF_SAADC_RESISTOR_DISABLED,
		.gain       = NRF_SAADC_GAIN1_2,
		.reference  = NRF_SAADC_REFERENCE_INTERNAL,
		.acq_time   = NRF_SAADC_ACQTIME_20US,
		.mode       = NRF_SAADC_MODE_DIFFERENTIAL,
		.burst      = NRF_SAADC_BURST_ENABLED,
		.pin_p      = NRF_SAADC_INPUT_AIN4,
		.pin_n      = NRF_SAADC_INPUT_AIN6
	};
	err_code = nrfx_saadc_channel_init(0, &config);
	APP_ERROR_CHECK(err_code);
}

void adc_start(void) {
	uint32_t err_code;
	if (!nrfx_saadc_is_busy()) {
		LOG_RAW("adc_start\n");
		err_code = nrfx_saadc_buffer_convert(adc_buffer, 1);
		APP_ERROR_CHECK(err_code);
		err_code = nrfx_saadc_sample();
		APP_ERROR_CHECK(err_code);
	}
}

// int16_t adc_get(uint8_t channel) {
// 	static nrf_saadc_value_t adc_value;
// 	if (!nrfx_saadc_is_busy()) {
// 		nrfx_saadc_sample_convert(channel, &adc_value);
// 	}
// 	return (int16_t)adc_value;
// }

void platform_init(void) {
#if NRF_LOG_ENABLED==1
	log_init();
#endif
	// Initialize the async SVCI interface to bootloader before any interrupts are enabled.
	// 在没有烧录bootloader时，必须禁用，否则会报错
	// #ifdef NDEBUG
	// 	uint32_t err_code = ble_dfu_buttonless_async_svci_init();
	// 	APP_ERROR_CHECK(err_code);
	// #endif
	NRF_LOG_ERROR("Log Level ERROR");
	NRF_LOG_WARNING("Log Level WARNING");
	NRF_LOG_INFO("Log Level INFO");
	NRF_LOG_DEBUG("Log Level DEBUG");
	LOG_RAW("RESET=0x%04X\r\n", read_chip_reset_reason());
	LOG_RAW("UICR_NFC=0x%04X\r\n", read_uicr_nfc_config());
	APP_SCHED_INIT(16, 32);
	user_event_init();
	power_management_init();
	leds_config();
	switch_btn_config();
	adc_config();
	bluetooth_init();

#if MINIMAL_PERI_MODE==1
	sd_power_dcdc_mode_set(NRF_POWER_DCDC_DISABLE);
#else
	sd_power_dcdc_mode_set(NRF_POWER_DCDC_ENABLE);
#endif
}

void platform_reboot(void) {
	sd_nvic_SystemReset();
}

bool is_going_to_shutdown = false;
void platform_powerdown(bool flag) {
	is_going_to_shutdown = flag;
}

void shutdown_routine(void) {
	nrf_gpio_cfg_sense_set(BUTTON_PIN, BUTTON_SENSE);
	nrf_pwr_mgmt_shutdown(NRF_PWR_MGMT_SHUTDOWN_GOTO_SYSOFF);
	while(1);
}

void platform_scheduler(void) {
	app_sched_execute();
	if (NRF_LOG_PROCESS() == false) {
		if(is_going_to_shutdown) {
			shutdown_routine();
		}
		nrf_pwr_mgmt_run();
	}
}
