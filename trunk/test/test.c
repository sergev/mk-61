/*
 * Manufacturer's test for MK-54.
 *
 * Copyright (C) 2013 Serge Vakulenko, <serge@vak.ru>
 *
 * Permission to use, copy, modify, and distribute this software
 * and its documentation for any purpose and without fee is hereby
 * granted, provided that the above copyright notice appear in all
 * copies and that both that the copyright notice and this
 * permission notice and warranty disclaimer appear in supporting
 * documentation, and that the name of the author not be used in
 * advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.
 *
 * The author disclaim all warranties with regard to this
 * software, including all implied warranties of merchantability
 * and fitness.  In no event shall the author be liable for any
 * special, indirect or consequential damages or any damages
 * whatsoever resulting from loss of use, data or profits, whether
 * in an action of contract, negligence or other tortious action,
 * arising out of or in connection with the use or performance of
 * this software.
 */
#include <stdio.h>
#include <unistd.h>

#include "calc.h"

//
// Test from MK-54 user manual.
//
static const unsigned char test[] = {
#if 1
    KEY_F, 0,       KEY_ADD, 0,                 // 1: F pi
    KEY_F, 0,       KEY_3, 0,                   // 2: F ln
    KEY_F, 0,       KEY_2, 0,                   // 3: F lg
    KEY_F, 0,       KEY_0, 0,                   // 4: F 10^x
    KEY_F, 0,       KEY_XY, 0,                  // 5: F x^y
    MODE_DEGREES,
    KEY_F, 0,       KEY_7, 0,                   // 6: F sin
    KEY_F, 0,       KEY_8, 0,                   // 7: F cos
    KEY_F, 0,       KEY_9, 0,                   // 8: F tg
    KEY_F, 0,       KEY_6, 0,                   // 9: F tg-1
    MODE_RADIANS,
    KEY_F, 0,       KEY_5, 0,                   // 10: F cos-1
    KEY_F, 0,       KEY_4, 0,                   // 11: F sin-1
    KEY_CLEAR, 0,                               // 12: Cx
    KEY_STORE, 0,   KEY_4, 0,                   // 13: П 4
    KEY_1, 0,       KEY_4, 0,                   // 14: 1 4
    KEY_STORE, 0,   KEY_0, 0,                   // 15: П 0
    KEY_RET, 0,                                 // 16: B/O
    KEY_F, 0,       KEY_EXP, 0,                 // 17: F ПРГ
    KEY_K, 0,       KEY_STORE, 0,   KEY_0, 0,   // 18: K П 0
    KEY_F, 0,       KEY_LOAD, 0,                // 19: F L0
    KEY_1, 0,       KEY_3, 0,                   // 20: 1 3
    KEY_F, 0,       KEY_STORE, 0,               // 21: F L1
    KEY_0, 0,       KEY_9, 0,                   // 22: 0 9
    KEY_1, 0,                                   // 23: 1
    KEY_7, 0,                                   // 24: 7
    KEY_STORE, 0,   KEY_3, 0,                   // 25: П 3
    KEY_K, 0,       KEY_GOTO, 0,    KEY_3, 0,   // 26: K БП 3
    KEY_LOAD, 0,    KEY_CLEAR, 0,               // 27: ИП d
    KEY_DIV, 0,                                 // 28: /
    KEY_GOTO, 0,                                // 29: БП
    KEY_0, 0,       KEY_3, 0,                   // 30: 0 3
    KEY_LOAD, 0,    KEY_0, 0,                   // 31: ИП 0
    KEY_GOTO, 0,                                // 32: БП
    KEY_0, 0,       KEY_0, 0,                   // 33: 0 0
    KEY_MUL, 0,                                 // 34: *
    KEY_LOAD, 0,    KEY_7, 0,                   // 35: ИП 7
    KEY_ADD, 0,                                 // 36: +
    KEY_F, 0,       KEY_GOTO, 0,                // 37: F L2
    KEY_2, 0,       KEY_5, 0,                   // 38: 2 5
    KEY_F, 0,       KEY_CALL, 0,                // 39: F L3
    KEY_2, 0,       KEY_5, 0,                   // 40: 2 5
    KEY_GOTO, 0,                                // 41: БП
    KEY_2, 0,       KEY_7, 0,                   // 42: 2 7
    KEY_GOTO, 0,                                // 43: БП
    KEY_2, 0,       KEY_1, 0,                   // 44: 2 1
    KEY_K, 0,       KEY_STORE, 0,   KEY_4, 0,   // 45: K П 4
    KEY_F, 0,       KEY_NEXT, 0,                // 46: F x<0
    KEY_3, 0,       KEY_1, 0,                   // 47: 3 1
    KEY_K, 0,       KEY_0, 0,                   // 48: K НОП
    KEY_F, 0,       KEY_PREV, 0,                // 49: F x=0
    KEY_3, 0,       KEY_5, 0,                   // 50: 3 5
    KEY_GOTO, 0,                                // 51: БП
    KEY_3, 0,       KEY_9, 0,                   // 52: 3 9
    KEY_CALL, 0,                                // 53: ПП
    KEY_5, 0,       KEY_4, 0,                   // 54: 5 4
    KEY_F, 0,       KEY_RET, 0,                 // 55: F x>=0
    KEY_3, 0,       KEY_3, 0,                   // 56: 3 3
    KEY_LOAD, 0,    KEY_9, 0,                   // 57: ИП 7
    KEY_SUB, 0,                                 // 58: -
    KEY_F, 0,       KEY_STOPGO, 0,              // 59: F x!=0
    KEY_6, 0,       KEY_0, 0,                   // 60: 6 0
    KEY_4, 0,                                   // 61: 4
    KEY_7, 0,                                   // 62: 7
    KEY_STORE, 0,   KEY_6, 0,                   // 63: П 6
    KEY_XY, 0,                                  // 64: xy
    KEY_K, 0,       KEY_RET, 0,     KEY_6, 0,   // 65: K x>=0 6
    KEY_K, 0,       KEY_PREV, 0,    KEY_6, 0,   // 66: K x=0 6
    KEY_K, 0,       KEY_STOPGO, 0,  KEY_6, 0,   // 67: K x!=0 6
    KEY_K, 0,       KEY_NEXT, 0,    KEY_6, 0,   // 68: K x<0 6
    KEY_F, 0,       KEY_MUL, 0,                 // 69: F x^2
    KEY_GOTO, 0,                                // 70: БП
    KEY_5, 0,       KEY_7, 0,                   // 71: 5 7
    KEY_F, 0,       KEY_SUB, 0,                 // 72: F sqrt
    KEY_F, 0,       KEY_DIV, 0,                 // 73: F 1/x
    KEY_RET, 0,                                 // 74: B/O
    KEY_6, 0,                                   // 75: 6
    KEY_2, 0,                                   // 76: 2
    KEY_STORE, 0,   KEY_NEG, 0,                 // 77: П b
    KEY_K, 0,       KEY_CALL, 0,    KEY_NEG, 0, // 78: K ПП b
    KEY_STOPGO, 0,                              // 79: С/П
    KEY_F, 0,       KEY_ENTER, 0,               // 80: F Bx
    KEY_F, 0,       KEY_1, 0,                   // 81: F e^x
    KEY_RET, 0,                                 // 82: B/O
    KEY_F, 0,       KEY_NEG, 0,                 // 83: F ABT
    KEY_RET, 0,                                 // 84: B/O
    KEY_STOPGO, 0,                              // 85: С/П
    KEY_F, 0,       KEY_DOT, 0,                 // 86: F O
    KEY_F, 0,       KEY_DOT, 0,                 // 87: F O
    KEY_F, 0,       KEY_DOT, 0,                 // 88: F O
    KEY_LOAD, 0,    KEY_1, 0,                   // 89: ИП 1
    KEY_LOAD, 0,    KEY_2, 0,                   // 90: ИП 2
    KEY_LOAD, 0,    KEY_3, 0,                   // 91: ИП 3
    KEY_LOAD, 0,    KEY_4, 0,                   // 92: ИП 4
    KEY_LOAD, 0,    KEY_5, 0,                   // 93: ИП 5
    KEY_LOAD, 0,    KEY_8, 0,                   // 94: ИП 8
    KEY_LOAD, 0,    KEY_DOT, 0,                 // 95: ИП a
    KEY_LOAD, 0,    KEY_CLEAR, 0,               // 96: ИП d
    KEY_NEG, 0,                                 // 97: /-/
    KEY_F, 0,       KEY_SUB, 0,                 // 98: F sqrt
    KEY_CLEAR,      KEY_CLEAR, 0, KEY_CLEAR, 0, // 99: Cx
    KEY_EXP, 0,                                 // 100: ВП
    MODE_GRADS,
    KEY_F, 0,       KEY_7, 0,                   // 101: F sin
#endif
#if 0
    KEY_RET, 0,                                 // B/O
    KEY_F, 0,       KEY_EXP, 0,                 // F ПРГ
    KEY_1, 0,                                   // 1
    KEY_2, 0,                                   // 2
    KEY_3, 0,                                   // 3
    KEY_4, 0,                                   // 4
    KEY_5, 0,                                   // 5
    KEY_6, 0,                                   // 6
    KEY_7, 0,                                   // 7
    KEY_8, 0,                                   // 8
    KEY_9, 0,                                   // 9
    KEY_GOTO, 0,                                // БП
    KEY_0, 0,       KEY_0, 0,                   // 0 0
    KEY_F, 0,       KEY_NEG, 0,                 // F ABT
    KEY_RET, 0,                                 // B/O
    KEY_STOPGO, 0,                              // С/П
#endif
    0xff,
};

//
// Symbols on display.
//
unsigned char display [12];
unsigned char show_dot [12];            // Show a decimal dot
unsigned display_changed = 0;

unsigned step_num = 0;

unsigned rad_grad_deg = MODE_DEGREES;   // Switch radians/grads/degrees

unsigned keycode = 0;

//
// Show the next display symbol.
// Index counter is in range 0..11.
//
void calc_display (int i, int digit, int dot)
{
    if (i >= 0) {
        if (digit >= 0) {
            if (digit != display[i] || dot != show_dot[i]) {
                display[i] = digit;
                show_dot[i] = dot;
                display_changed = 1;
            }
        }
    }
}

//
// Poll the radians/grads/degrees switch.
//
int calc_rgd()
{
    return rad_grad_deg;
}

//
// Poll the keypad.
//
int calc_keypad()
{
    return keycode;
}

//
// Poll optional peripherals.
//
void calc_poll()
{
    // Empty.
}

int main()
{
    int running, next = 0;

#ifdef MK_54
    printf ("Started MK-54.\n");
#else
    printf ("Started MK-61.\n");
#endif
    calc_init();

    for (;;) {
        // Simulate one cycle of the calculator.
        running = calc_step();
        step_num++;

        if (running) {
            printf ("%3u -- running\n", step_num);
            display_changed = 1;
            continue;
        }

        if (display_changed) {
            int i;

            printf ("%3u -- '", step_num);
            for (i=0; i<12; i++) {
                putchar ("0123456789-LCRE " [display[11-i]]);
                if (show_dot[11-i])
                    putchar ('.');
            }
            printf ("'\n");
            display_changed = 0;
        }

        if (test [next] == 0xff)
            break;

        // Switch radians/grads/degrees mode.
        if (test [next] > 0 && test [next] < 16)
            rad_grad_deg = test [next++];

        keycode = test [next++];
    }
    printf ("Finished.\n");
    return 0;
}
