
#ifndef _MORSE_CODE_H_
#define _MORSE_CODE_H_

#include <stdint.h>
#include "usb_hid_keys.h"

typedef struct morse_code {
	const struct morse_code const* dit;
	const struct morse_code const* dah;
	const uint8_t code;
} sMC;

#define NODE(name,code,dit,dah) const sMC node_ ##name = {&node_ ##dit, &node_ ##dah, code}
#define NODEF(name,code) const sMC node_ ##name = {&node_null, &node_null, code}

const sMC node_null = {
	NULL,
	NULL,
	0xFF
};

// dit
NODEF(apostrophe, KEY_APOSTROPHE);
NODEF(dot, KEY_DOT);
NODEF(qm, 0xFF);
NODEF(end, KEY_ENTER);
NODE(1, KEY_1, apostrophe, null);
NODE(r10, 0xFF, null, dot);
NODE(u10, 0xFF, qm, null);
NODEF(2, KEY_2);
NODE(v0, 0xFF,  null,  end);
NODEF(3, KEY_3);
NODEF(4, KEY_4);
NODEF(5, KEY_5);
NODEF(p, KEY_P);
NODE(j, KEY_J, null, 1);
NODE(r1, 0xFF, r10, null);
NODEF(l, KEY_L);
NODE(u1, 0xFF, u10, 2);
NODEF(f, KEY_F);
NODE(v, KEY_V, v0, 3);
NODE(h, KEY_H, 5, 4);
NODE(w, KEY_W, p, j);
NODE(r, KEY_R, l, r1);
NODE(u, KEY_U, f, u1);
NODE(s, KEY_S, h, v);
NODE(a, KEY_A, r, w);
NODE(i, KEY_I, s, u);
NODE(e, KEY_E, i, a);

// dah
NODEF(minus, KEY_MINUS);
NODEF(comma, KEY_COMMA);
NODEF(semicolon, KEY_SEMICOLON);
NODEF(colon, KEY_SEMICOLON);
NODE(6, KEY_6, null, minus);
NODEF(eq, KEY_EQUAL);
NODEF(slash, KEY_SLASH);
NODE(c1, 0xFF, semicolon, null);
NODEF(7, KEY_7);
NODE(z1, 0xFF, null, comma);
NODE(8, KEY_8, colon, null);
NODEF(9, KEY_9);
NODEF(0, KEY_0);
NODE(b, KEY_B, 6, eq);
NODE(x, KEY_X, slash, null);
NODE(c, KEY_C, null, c1);
NODEF(y, KEY_Y);
NODE(z, KEY_Z, 7, z1);
NODEF(q, KEY_Q);
NODE(o0, 0xFF, 8, null);
NODE(o1, 0xFF, 9, 0);
NODE(d, KEY_D, b, x);
NODE(k, KEY_K, c, y);
NODE(g, KEY_G, z, q);
NODE(o, KEY_O, o0, o1);
NODE(n, KEY_N, d, k);
NODE(m, KEY_M, g, o);
NODE(t, KEY_T, n, m);

NODE(root, 0xFF, e, t);

#endif
