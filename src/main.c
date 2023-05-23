

#include <stdint.h>
#include <string.h>

#include "platform.h"
#include "uevent.h"
#include "bluetooth.h"

#include "nrf_gpio.h"
#include "app_timer.h"

#ifdef CONFIG_NFCT_PINS_AS_GPIOS
	volatile uint32_t UICR_ADDR_0x20C __attribute__((section("uicr_nfc"))) = 0xFFFFFFFE;
#endif

// TODO:
// 广播状态蓝灯闪烁，连接状态蓝灯熄灭
// 连接状态，识别到“点”闪红灯，识别到“划”闪蓝灯，发送字符闪白灯
// 低电状态红灯闪烁，不再闪点划灯。
// 开机或唤醒，蜂鸣器短鸣一次
// buzzer开时，蜂鸣器跟随按键，蜂鸣器最多连续鸣响3秒

static bool is_buzzer_on = false;
static void buzzer_switch_routine(void) {
	static uint8_t buzzer_onoff = 0;
	if(nrf_gpio_pin_read(SW1_PIN) == 1) {
		if(buzzer_onoff < 50) {
			buzzer_onoff += 1;
			if(buzzer_onoff >= 50) {
				LOG_RAW("BUZZER ON\n");
				is_buzzer_on = true;
			}
		}
	} else {
		if(buzzer_onoff > 0) {
			buzzer_onoff -= 1;
			if(buzzer_onoff == 0) {
				LOG_RAW("BUZZER OFF\n");
				is_buzzer_on = false;
				// TODO: 打断buzzer事件
				nrf_gpio_pin_set(BUZZER_PIN);
			}
		}
	}
}

bool is_caps_on = false;
static void caps_switch_routine(void) {
	static uint8_t caps_onoff = 0;
	if(nrf_gpio_pin_read(SW2_PIN) == 1) {
		if(caps_onoff < 50) {
			caps_onoff += 1;
			if(caps_onoff >= 50) {
				LOG_RAW("CAPS ON\n");
				is_caps_on = true;
			}
		}
	} else {
		if(caps_onoff > 0) {
			caps_onoff -= 1;
			if(caps_onoff == 0) {
				LOG_RAW("CAPS OFF\n");
				is_caps_on = false;
			}
		}
	}
}


static void led_routine(void) {

}
static void buzzer_routine(void) {

}
static void btn_buzzer_routine(void) {
	static uint16_t buzzer_time = 0;
	if(is_buzzer_on) {
		if(nrf_gpio_pin_read(BUTTON_PIN) == 1) {
			if(buzzer_time >= 300) {
				nrf_gpio_pin_set(BUZZER_PIN);
			} else {
				nrf_gpio_pin_clear(BUZZER_PIN);
				buzzer_time += 1;
			}
		} else {
			nrf_gpio_pin_set(BUZZER_PIN);
			buzzer_time = 0;
		}
	}
}

#define MAX_CODE_LENGTH (9)
#include "morse_code.h"
uint8_t morse_code_parse(uint8_t* c, uint8_t length) {
	const sMC* node = &node_root;
	if((length > MAX_CODE_LENGTH ) || (length == 0)) {
		return 0xFF;
	}
	while(length > 0) {
		if(*c > 0) {
			node = node->dah;
		} else {
			node = node->dit;
		}
		if(node == &node_null) {
			return 0xFF;
		}
		c += 1;
		length -= 1;
	}
	return node->code;
}

#define BASE_TIME (8)

uint8_t dida_depth = 0;
uint8_t dida_idle_timer = 0;
uint8_t dida_cache[9];
uint8_t valid_cache[9];
static void dida_push(uint8_t time) {
	if(dida_depth < 9) {
		dida_cache[dida_depth] = time;
		dida_depth += 1;
		dida_idle_timer = 50;
	}
}
extern void char_send(uint8_t character);
extern void str_send(uint8_t* str, uint8_t len);
static void dida_parse(void) {
	if(dida_depth <= 9) {
		for (uint8_t i = 0; i < dida_depth; i++) {
			LOG_RAW("[%d]", dida_cache[i]);
		}
		LOG_RAW("\n");
		for (uint8_t i = 0; i < dida_depth; i++) {
			if(dida_cache[i] <= BASE_TIME + 1) {
				LOG_RAW(".")
			} else if(dida_cache[i] >= BASE_TIME * 2.5 && dida_cache[i] <= BASE_TIME * 3.5) {
				LOG_RAW("_")
			} else {
				LOG_RAW("|", dida_cache[i]);
			}
			if(dida_cache[i] <= BASE_TIME * 1.5) {
				valid_cache[i] = 0;
			} else if(dida_cache[i] >= BASE_TIME * 2 && dida_cache[i] <= BASE_TIME * 3.5) {
				valid_cache[i] = 1;
			} else {
				valid_cache[i] = 0xFF;
				break;
			}
		}
		uint8_t send_code = morse_code_parse(valid_cache, dida_depth);
		if(send_code == 0xFE) {
			static uint8_t sos[2] = {KEY_S, KEY_O};
			str_send(sos, 2);
			char_send(KEY_S);
		} else if(send_code != 0xFF) {
			char_send(send_code);
			if(send_code == KEY_ENTER) {
				// no space after enter key
				dida_idle_timer = 0;
			}
		}
	}
	LOG_RAW("\n");
	dida_depth = 0;
}

static void btn_routine(void) {
	static uint8_t current_btn_time = 0;
	if(nrf_gpio_pin_read(BUTTON_PIN) == 1) {
		if(current_btn_time < 100) {
			current_btn_time += 1;
		}
	} else {
		if(current_btn_time > 0 && current_btn_time < 50) {
			dida_push(current_btn_time);
			current_btn_time = 0;
		}
		if(dida_idle_timer > 0) {
			dida_idle_timer -= 1;
			if(dida_idle_timer == 29) {
				dida_parse();
			}
			if(dida_idle_timer == 1) {
				char_send(KEY_SPACE);
			}
		}
	}
}

APP_TIMER_DEF(sys_100hz_timer);
static void sys_100hz_handler(void* p_context) {
	btn_routine();
	buzzer_switch_routine();
	caps_switch_routine();
	btn_buzzer_routine();
	buzzer_routine();
	led_routine();
}

void user_init(void) {
	uint32_t err;
	err = app_timer_create(&sys_100hz_timer, APP_TIMER_MODE_REPEATED, sys_100hz_handler);
	APP_ERROR_CHECK(err);
}

void main(void) {
	platform_init();
	user_init();

	LOG_RAW("RTT Started.\n");
	uevt_bc_e(UEVT_SYS_SETUP);
	NRF_LOG_INFO("HAMKey-Lite started.");
	bluetooth_adv_start(false);
	app_timer_start(sys_100hz_timer, APP_TIMER_TICKS(10), NULL);

	for (;;) {
		platform_scheduler();
	}
}


#include "app_error.h"

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "app_util_platform.h"
#include "nrf_strerror.h"

#if defined(SOFTDEVICE_PRESENT) && SOFTDEVICE_PRESENT
	#include "nrf_sdm.h"
#endif
void app_error_fault_handler(uint32_t id, uint32_t pc, uint32_t info) {
	__disable_irq();

#ifndef DEBUG
	NRF_LOG_ERROR("Fatal error");
#else
	switch (id) {
#if defined(SOFTDEVICE_PRESENT) && SOFTDEVICE_PRESENT
		case NRF_FAULT_ID_SD_ASSERT:
			NRF_LOG_ERROR("SOFTDEVICE: ASSERTION FAILED");
			break;
		case NRF_FAULT_ID_APP_MEMACC:
			NRF_LOG_ERROR("SOFTDEVICE: INVALID MEMORY ACCESS");
			break;
#endif
		case NRF_FAULT_ID_SDK_ASSERT: {
			assert_info_t* p_info = (assert_info_t*)info;
			NRF_LOG_ERROR("ASSERTION FAILED at %s:%u",
						  p_info->p_file_name,
						  p_info->line_num);
			break;
		}
		case NRF_FAULT_ID_SDK_ERROR: {
			error_info_t* p_info = (error_info_t*)info;
			NRF_LOG_ERROR("Start of error report");
			NRF_LOG_ERROR("ERROR %u [%s] at %s:%u\r\nPC at: 0x%08x",
						  p_info->err_code,
						  nrf_strerror_get(p_info->err_code),
						  p_info->p_file_name,
						  p_info->line_num,
						  pc);
			NRF_LOG_ERROR("Error code[%d]\r\n", p_info->err_code);
			NRF_LOG_ERROR("File[%s]\n", p_info->p_file_name);
			NRF_LOG_ERROR("Line[%d]\n", p_info->line_num);
			NRF_LOG_ERROR("End of error report");
			break;
		}
		default:
			NRF_LOG_ERROR("UNKNOWN FAULT at 0x%08X", pc);
			break;
	}
#endif

	// On assert, the system can only recover with a reset.

	NRF_LOG_FINAL_FLUSH();
#ifndef DEBUG
	NRF_LOG_WARNING("System reset");
	NVIC_SystemReset();
#else
	app_error_save_and_stop(id, pc, info);
#endif // DEBUG
	NRF_BREAKPOINT_COND;
}
