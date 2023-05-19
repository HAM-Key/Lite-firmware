
#ifndef _UENENT_H_
#define _UENENT_H_

#include <stdint.h>
#include <string.h>

typedef struct {
	uint16_t evt_id;
	void* content;
} uevt_t;

typedef void (*fpevt_h)(uevt_t*);

#include "platform.h"

void user_event_broadcast(uevt_t evt);
void user_event_init(void);
void user_event_handler_regist(fpevt_h func);
void user_event_handler_unregist(fpevt_h func);
void user_event_array_dispatcher(uevt_t evt);

#if NRF_LOG_ENABLED==1 && EVT_LOG_ENABLED==1
#define uevt_bc(x,y) LOG_RAW("EVT Push:" #x "\r\n"); \
	user_event_broadcast((uevt_t){x,y})
#define uevt_bc_e(x) LOG_RAW("EVT Push:" #x "\r\n"); \
	user_event_broadcast((uevt_t){x,NULL})
#define uevt_sd(x,y,hFunc) LOG_RAW("EVT Send:" #x "to " #hFunc "\r\n"); \
	user_event_send((uevt_t){x,y},hFunc)
#define uevt_sd_e(x,hFunc) LOG_RAW("EVT Send:" #x "to " #hFunc "\r\n"); \
	user_event_send((uevt_t){x,NULL},hFunc)
#else
#define uevt_bc(x,y) user_event_broadcast((uevt_t){x,y})
#define uevt_bc_e(x) user_event_broadcast((uevt_t){x,NULL})
#define uevt_sd(x,y,hFunc) user_event_send((uevt_t){x,y},hFunc)
#define uevt_sd_e(x,hFunc) user_event_send((uevt_t){x,NULL},hFunc)
#endif

#endif
