
#include "stdint.h"
#include "stdio.h"
#include "morse_code.h"


#define MAX_CODE_LENGTH (9)
uint8_t morse_code_parse(uint8_t* c, uint8_t length) {
	const sMC* node = &node_root;
	if((length > MAX_CODE_LENGTH ) || (length == 0)) {
		return;
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

uint8_t test[9] = {
	1, 0, 1, 0
};

void main(void) {
	printf("0x%02x\n", morse_code_parse(test, 4));
	// printf("0x%02x\n", node_root.dit->dit->dah->dit->code);
}
