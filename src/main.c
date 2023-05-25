

#include <stdint.h>
#include <string.h>

#include "platform.h"
#include "uevent.h"
#include "bluetooth.h"

#include "nrf_gpio.h"
#include "app_timer.h"

#ifdef CONFIG_NFCT_PINS_AS_GPIOS
	const uint32_t UICR_ADDR_0x20C __attribute__((section(".uicr_nfc_section"))) __attribute__((used)) = 0xFFFFFFFE;
#else
	const uint32_t UICR_ADDR_0x20C __attribute__((section(".uicr_nfc_section"))) __attribute__((used)) = 0xFFFFFFFF;
#endif

// TODO:
//// [DONE] 电量检测
//// [DONE] 低电状态红灯闪烁，不再闪点划灯
//// [DONE] buzzer开关打开时，蓝牙连接则鸣响2次
//// [DONE] 广播状态蓝灯闪烁不闪点划灯，连接状态蓝灯熄灭
//// [DONE] 连接状态，识别到“点”闪红灯，识别到“划”闪蓝灯，发送字符闪白灯
//// [DONE] 开机或唤醒，蜂鸣器鸣响加号摩斯码
//// [DONE] buzzer开时，蜂鸣器跟随按键，蜂鸣器最多连续鸣响3秒
//// [DONE] 15分钟无操作休眠, 按键唤醒
//// [DONE] 长按2秒进入办公模式，短按发送ctrl+v 长按发送ctrl+c, 长按2秒退出
//// [DONE] 按住按键启动重置蓝牙

// 0xFF for unknown
uint8_t batt_percent = 0xFF;
uint32_t batt_volt = 4200;

uint32_t idle_timer = 0;

static bool is_office_mode = false;
static bool is_lowpower = false;
static bool is_buzzer_on = false;
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

void (*const led_set[3])(bool) = {
	led_red_set,
	led_blue_set,
	led_white_set
};
const uint8_t led_off_click_timer[3] = {10, 10, 30};
static uint8_t led_off_timer[3] = {0, 0, 0};
static void led_click_blink(uint8_t led) {
	if(!is_bt_connected()) {
		return;
	}
	if(is_lowpower) {
		return;
	}
	led_set[led](1);
	led_off_timer[led] = led_off_click_timer[led];
}
static void led_slow_blink(uint8_t led) {
	led_set[led](1);
	led_off_timer[led] = 100;
}
static void led_blink(uint8_t led, uint8_t timer) {
	led_set[led](1);
	led_off_timer[led] = timer;
}

static void led_routine(void) {
	for (uint8_t i = 0; i < 3; i++) {
		if(led_off_timer[i] > 0) {
			led_off_timer[i] -= 1;
			if(led_off_timer[i] == 0) {
				led_set[i](0);
			}
		}
	}
}

const uint8_t buzzer_task_dit[] = {8, 8, 0};
const uint8_t buzzer_task_plus[] = {8, 8, 24, 8, 8, 8, 24, 8, 8, 0};
const uint8_t buzzer_task_K[] = {24, 8, 8, 8, 24, 0};
const uint8_t* buzzer_task = NULL;
uint8_t buzzer_task_counter = 0;
uint8_t buzzer_task_p = 0;
uint8_t buzzer_task_timer = 0;
void buzzer_task_start(const uint8_t* task, uint8_t count) {
	if(is_lowpower) {
		return;
	}
	BUZZER_ON();
	buzzer_task_p = 0;
	buzzer_task_timer = task[0];
	buzzer_task_counter = count;
	buzzer_task = task;
}
static void buzzer_routine(void) {
	if(buzzer_task_counter == 0) {
		return;
	}
	if(buzzer_task_timer > 0) {
		buzzer_task_timer -= 1;
		return;
	}
	buzzer_task_p += 1;
	buzzer_task_timer = buzzer_task[buzzer_task_p];
	if(buzzer_task_timer == 0) {
		if(buzzer_task_counter <= 1) {
			buzzer_task_counter = 0;
			BUZZER_OFF();
		} else {
			buzzer_task_counter -= 1;
			buzzer_task_timer = buzzer_task[0];
			buzzer_task_p = 0;
			BUZZER_ON();
		}
		return;
	}
	if(buzzer_task_p & 0x1) {
		BUZZER_OFF();
	} else {
		BUZZER_ON();
	}
}

static void btn_buzzer_routine(void) {
	static uint16_t buzzer_time = 0;
	if(is_buzzer_on && buzzer_task_counter == 0) {
		if(nrf_gpio_pin_read(BUTTON_PIN) == 1) {
			if(buzzer_time >= 100) {
				BUZZER_OFF();
			} else {
				buzzer_time += 1;
				if(is_lowpower) {
					return;
				}
				BUZZER_ON();
			}
		} else {
			BUZZER_OFF();
			buzzer_time = 0;
		}
	}
}

static void buzzer_switch_routine(void) {
	static uint8_t buzzer_onoff = 0;
	if(nrf_gpio_pin_read(SW1_PIN) > 0) {
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
				buzzer_task_counter = 0;
				BUZZER_OFF();
			}
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
extern void char_send(uint8_t character, uint8_t mod);
extern void str_send(uint8_t* str, uint8_t len, uint8_t mod);
static void dida_parse(void) {
	if(dida_depth <= 9) {
		for (uint8_t i = 0; i < dida_depth; i++) {
			LOG_RAW("[%d]", dida_cache[i]);
		}
		LOG_RAW("\n");
		for (uint8_t i = 0; i < dida_depth; i++) {
			if(dida_cache[i] <= BASE_TIME * 1.5) {
				valid_cache[i] = 0;
				LOG_RAW(".");
			} else if(dida_cache[i] >= BASE_TIME * 2 && dida_cache[i] <= BASE_TIME * 3.5) {
				valid_cache[i] = 1;
				LOG_RAW("_");
			} else {
				valid_cache[i] = 0xFF;
				break;
			}
		}
		uint8_t send_code = morse_code_parse(valid_cache, dida_depth);
		if(send_code == 0xFE) {
			uint8_t mod;
			if(is_caps_on) {
				mod = KEY_MOD_LSHIFT;
			} else {
				mod = 0;
			}
			char_send(KEY_S, mod);
			char_send(KEY_O, mod);
			char_send(KEY_S, mod);
			if(mod) {
				char_send(KEY_NONE, 0);
			}
		} else if(send_code != 0xFF) {
			led_click_blink(2);
			LOG_RAW("SEND CODE [%02X]\n", send_code);
			uint8_t mod;
			if(is_caps_on && send_code <= KEY_Z) {
				mod = KEY_MOD_LSHIFT;
			} else {
				mod = 0;
			}
			char_send(send_code, mod);
			if(mod) {
				char_send(KEY_NONE, 0);
			}
			if(send_code == KEY_ENTER) {
				// no space after enter key
				dida_idle_timer = 0;
			}
		}
	}
	LOG_RAW("\n");
	dida_depth = 0;
}

static uint16_t current_btn_time = 0;
static void office_mode_btn_press(uint16_t t) {
	if(t >= 200 || (t == 0)) {
		return;
	}
	if(t < 12) {
		LOG_RAW("SEND PASTE\n");
		led_click_blink(0);
		char_send(KEY_V, KEY_MOD_LCTRL);
		char_send(KEY_NONE, 0);
	} else if(t < 50) {
		LOG_RAW("SEND COPY\n");
		led_click_blink(1);
		char_send(KEY_C, KEY_MOD_LCTRL);
		char_send(KEY_NONE, 0);
	}
}
static void morse_mode_btn_press(uint16_t t) {
	if(t >= 200) {
		return;
	}
	if(t > 0 && t < 50) {
		if(t <= BASE_TIME * 1.5) {
			led_click_blink(0);
		} else if(t >= BASE_TIME * 2 && t <= BASE_TIME * 3.5) {
			led_click_blink(1);
		}
		dida_push(t);
	}
	if(dida_idle_timer > 0) {
		dida_idle_timer -= 1;
		if(dida_idle_timer == 29) {
			dida_parse();
		}
		if(dida_idle_timer == 1) {
			char_send(KEY_SPACE, 0);
		}
	}
}
static void btn_routine(void) {
	if(nrf_gpio_pin_read(BUTTON_PIN) == 1) {
		idle_timer = 0;
		if(current_btn_time < 200) {
			current_btn_time += 1;
			if(current_btn_time == 200) {
				is_office_mode = !is_office_mode;
				if(is_office_mode) {
					led_blink(2, 100);
					if(is_buzzer_on) {
						buzzer_task_start(buzzer_task_dit, 3);
					}
				} else {
					led_blink(1, 100);
					if(is_buzzer_on) {
						buzzer_task_start(buzzer_task_dit, 2);
					}
				}
			}
		}
	} else {
		if(is_office_mode) {
			office_mode_btn_press(current_btn_time);
		} else {
			morse_mode_btn_press(current_btn_time);
		}
		current_btn_time = 0;
	}
}
void shutdown_prepare(void) {
	app_timer_stop_all();
	BUZZER_OFF();
	led_set[0](0);
	led_set[1](0);
	led_set[2](0);
}
#define SHUTDOWN_COUNT (15*60*100)
static void func_routine(void) {
	static uint8_t adc_tmr = 0;
	static uint8_t lowpower_tmr = 0;
	static uint8_t bt_adv_tmr = 0;
	static bool old_bt_connected = false;
	static uint8_t bt_connected_timer = 0;
	idle_timer += 1;
	if(idle_timer > SHUTDOWN_COUNT) {
		shutdown_prepare();
		platform_powerdown(true);
		return;
	}
	adc_tmr += 1;
	if(adc_tmr == 0) {
		adc_start();
		if(batt_percent < 15) {
			is_lowpower = true;
		} else if (batt_percent > 90) {
			is_lowpower = false;
		}
	}
	if(is_lowpower || (lowpower_tmr != 0)) {
		lowpower_tmr += 1;
		if(lowpower_tmr == 90) {
			led_set[0](1);
		}
		if(lowpower_tmr == 100) {
			led_set[0](0);
			lowpower_tmr = 0;
		}
	}
	if(!is_bt_connected()) {
		bt_connected_timer = 0;
		old_bt_connected = false;
		bt_adv_tmr += 1;
		if(bt_adv_tmr >= 100) {
			bt_adv_tmr = 0;
			led_blink(1, 7);
		}
	} else {
		if(!old_bt_connected) {
			bt_connected_timer += 1;
			if(bt_connected_timer >= 100) {
				old_bt_connected = true;
				if(is_buzzer_on) {
					buzzer_task_start(buzzer_task_dit, 2);
				}
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
	func_routine();
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
	app_timer_start(sys_100hz_timer, APP_TIMER_TICKS(10), NULL);
	if(nrf_gpio_pin_read(BUTTON_PIN) == 1) {
		bluetooth_adv_start(true);
		buzzer_task_start(buzzer_task_K, 1);
	} else {
		bluetooth_adv_start(false);
		if(nrf_gpio_pin_read(SW1_PIN) > 0) {
			buzzer_task_start(buzzer_task_plus, 1);
		}
	}
	led_slow_blink(0);
	led_slow_blink(1);
	led_slow_blink(2);

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
