

#include <stdint.h>
#include <string.h>

#include "platform.h"
#include "uevent.h"
#include "bluetooth.h"


#ifdef CONFIG_NFCT_PINS_AS_GPIOS
	volatile uint32_t UICR_ADDR_0x20C __attribute__((section("uicr_nfc"))) = 0xFFFFFFFE;
#endif

void user_init(void) {
}

void main(void) {
	platform_init();
	user_init();

	LOG_RAW("RTT Started.\n");
	uevt_bc_e(UEVT_SYS_SETUP);
	NRF_LOG_INFO("HAMKey-Lite started.");
	bluetooth_adv_start(false);

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
