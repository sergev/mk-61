/*
 * Prototype based on Olimex PIC32-T795 board.
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
#include "calc.h"
#include "pic32mx.h"

#define MHZ     80              /* CPU clock is 80 MHz. */

#define PIN(n)  (1 << (n))

/*
 * Main entry point at bd003000.
 * Setup stack pointer and $gp registers, and jump to main().
 */
asm ("          .section .exception");
asm ("          .globl _start");
asm ("          .type _start, function");
asm ("_start:   la      $sp, _estack");
asm ("          la      $ra, main");
asm ("          la      $gp, _gp");
asm ("          jr      $ra");
asm ("          .text");

/*
 * Secondary entry point at bd004000.
 */
asm ("          .section .startup");
asm ("          .globl _init");
asm ("          .type _init, function");
asm ("_init:    la      $ra, _start");
asm ("          jr      $ra");
asm ("          .text");

/*
 * Delay for a given number of microseconds.
 * The processor has a 32-bit hardware Count register,
 * which increments at half CPU rate.
 * We use it to get a precise delay.
 */
void udelay (unsigned usec)
{
    unsigned now = mfc0 (C0_COUNT, 0);
    unsigned final = now + usec * MHZ / 2;

    for (;;) {
        now = mfc0 (C0_COUNT, 0);

        /* This comparison is valid only when using a signed type. */
        if ((int) (now - final) >= 0)
            break;
    }
}

//
// Display a symbol on 7-segment LED
//
void set_segments (unsigned digit, unsigned dot)
{
    static const unsigned segments[16] = {
    //- A - B - C - D - E -- F -- G --
        1 + 2 + 4 + 8 + 16 + 32,        // digit 0
            2 + 4,                      // digit 1
        1 + 2     + 8 + 16      + 64,   // digit 2
        1 + 2 + 4 + 8           + 64,   // digit 3
            2 + 4          + 32 + 64,   // digit 4
        1     + 4 + 8      + 32 + 64,   // digit 5
        1     + 4 + 8 + 16 + 32 + 64,   // digit 6
        1 + 2 + 4,                      // digit 7
        1 + 2 + 4 + 8 + 16 + 32 + 64,   // digit 8
        1 + 2 + 4 + 8      + 32 + 64,   // digit 9
                                  64,   // symbol -
                    8 + 16 + 32,        // symbol L
        1 +         8 + 16 + 32,        // symbol C
        1 +             16 + 32,        // symbol Г
        1 +         8 + 16 + 32 + 64,   // symbol E
        0,                              // empty
    };

    switch (digit) {
    case '-': digit = 10; break;
    case 'L': digit = 11; break;
    case 'C': digit = 12; break;
    case 'R': digit = 13; break;
    case 'E': digit = 14; break;
    case ' ': digit = 15; break;
    }
    unsigned mask = segments [digit & 15];

    if (mask & 1)   TRISECLR = PIN(0);  // segment A - signal RE0
    if (mask & 2)   TRISECLR = PIN(1);  // segment B - signal RE1
    if (mask & 4)   TRISECLR = PIN(2);  // segment C - signal RE2
    if (mask & 8)   TRISECLR = PIN(3);  // segment D - signal RE3
    if (mask & 16)  TRISECLR = PIN(4);  // segment E - signal RE4
    if (mask & 32)  TRISECLR = PIN(5);  // segment F - signal RE5
    if (mask & 64)  TRISECLR = PIN(6);  // segment G - signal RE6
    if (dot)        TRISECLR = PIN(7);  // dot       - signal RE7
}

//
// Clear 7-segment LED.
//
static inline void clear_segments()
{
    TRISESET = 0xff;                    // tristate
}

//
// Toggle clock signal.
//
static inline void clk()
{
    LATFSET = PIN(0);                   // set clock
    udelay (1);                         // 1 usec
    LATFCLR = PIN(0);                   // clear clock
}

//
// Set or clear data signal.
//
static inline void data (int on)
{
    if (on)
        LATFSET = PIN(1);               // set data
    else
        LATFCLR = PIN(1);               // clear data
}

static unsigned rgd;                    // Radians/grads/degrees
static unsigned keycode;                // Code of pressed button
static unsigned key_pressed;            // Bitmask of active key

//
// Poll keypad: input pins RD4-RD7.
//
int scan_keys (int row)
{
    static const int col_a [8] = {
        KEY_7,      //  7   sin
        KEY_4,      //  4   sin-1
        KEY_1,      //  1   e^x
        KEY_0,      //  0   10^x
        KEY_DOT,    //  ,   O
        KEY_NEG,    //  /-/ АВТ
        KEY_EXP,    //  ВП  ПРГ
        KEY_CLEAR,  //  Cx  CF
    };
    static const int col_b [8] = {
        KEY_K,      //  K
        KEY_LOAD,   //  ИП  L0
        KEY_8,      //  8   cos
        KEY_5,      //  5   cos-1
        KEY_2,      //  2   lg
        KEY_3,      //  3   ln
        KEY_XY,     //  xy  x^y
        KEY_ENTER,  //  B^  Bx
    };
    static const int col_c [8] = {
        KEY_F,      //  F
        KEY_NEXT,   //  ШГ> x<0
        KEY_PREV,   //  <ШГ x=0
        KEY_STORE,  //  П   L1
        KEY_9,      //  9   tg
        KEY_6,      //  6   tg-1
        KEY_ADD,    //  +   pi
        KEY_MUL,    //  *   x^2
    };
    static const int col_d [8] = {
        0,
        KEY_RET,    //  B/O x>=0
        KEY_GOTO,   //  БП  L2
        KEY_SUB,    //  -   sqrt
        KEY_DIV,    //  /   1/x
        KEY_CALL,   //  ПП  L3
        KEY_STOPGO, //  C/П x!=0
        0,
    };
    int pins = PORTD & 0xf0;
    int pins2 = PORTD & 0xf0;

    if (pins && pins == pins2) {
        if (pins & 0x10)
            return col_a[row];
        if (pins & 0x20)
            return col_b[row];
        if (pins & 0x40)
            return col_c[row];
        if (pins & 0x80)
            return col_d[row];
#if 0
        // TODO: poll radians/grads/degrees switch
        if (?) {
            switch (row) {
            case 0: rgd = MODE_RADIANS; break;
            case 7: rgd = MODE_DEGREES; break;
            }
        }
#endif
    }
    return 0;
}

//
// Show the next display symbol.
// Index counter is in range 0..11.
//
void calc_display (int i, int digit, int dot)
{
    clear_segments();
    if (i >= 0) {
        if (i == 0) {
            data (1);                   // set data
            clk();                      // toggle clock
            data (0);                   // clear data
        }
        clk();                          // toggle clock
        if (digit >= 0)
            set_segments (digit, dot);

        if (i < 8) {                    // scan keypad
            int key = scan_keys (i);
            if (key) {
                keycode = key;
                key_pressed |= (1 << i);
            } else {
                key_pressed &= ~(1 << i);
            }
        }
    }
}

//
// Poll the radians/grads/degrees switch.
//
int calc_rgd()
{
    return rgd;
}

//
// Poll the keypad.
//
int calc_keypad()
{
    if (! key_pressed)
        return 0;
    return keycode;
}

int main()
{
    /* Set memory wait states, for speedup. */
    CHECON = 2;
    BMXCONCLR = 0x40;
    CHECONSET = 0x30;

    /* Enable cache for kseg0 segment. */
    int config = mfc0 (C0_CONFIG, 0);
    mtc0 (C0_CONFIG, 0, config | 3);

    /* Initialize coprocessor 0. */
    mtc0 (C0_COUNT, 0, 0);
    mtc0 (C0_COMPARE, 0, -1);
    mtc0 (C0_EBASE, 1, 0x9fc00000);     /* Vector base */
    mtc0 (C0_INTCTL, 1, 1 << 5);        /* Vector spacing 32 bytes */
    mtc0 (C0_CAUSE, 0, 1 << 23);        /* Set IV */
    mtc0 (C0_STATUS, 0, 0);             /* Clear BEV */

    /* Disable JTAG and Trace ports, to make more pins available. */
    DDPCONCLR = 3 << 2;

    /* Use all ports as digital. */
    AD1PCFG = ~0;
    LATA = 0;
    LATB = 0;
    LATE = 0;
    LATF = 0;

    /* Use pins RB15, RB12 as output: LED1 and LED2 control. */
    TRISBCLR = PIN(15);
    TRISBCLR = PIN(12);

    /* RD4-RD7 - keypad. */
    TRISDSET = 0xf0;

    /* RF0 - clock, RF1 - data. */
    TRISFCLR = PIN(0) | PIN(1);

    /* RE0-RE6,RE7 - segments A-G and dot. */
    TRISESET = 0xff;                    // tristate
    LATECLR = 0xff;

    int i;
    data (0);                           // clear data
    for (i=0; i<16; i++)                // clear register
        clk();

    calc_init();
    rgd = MODE_DEGREES;
    keycode = 0;
    key_pressed = 0;

    for (;;) {
        // Simulate one cycle of the calculator.
        int running = calc_step();
#if 1
        // Simple test.
        static int next;
        static const unsigned char test[] = {
            KEY_CLEAR,  0,      // Cx
            KEY_3,      0,      // 3
            KEY_4,      0,      // 4
            KEY_5,      0,      // 5
            KEY_6,      0,      // 6
            KEY_7,      0,      // 7
            KEY_8,      0,      // 8
            KEY_9,      0,      // 9
            KEY_ENTER,  0,      // B^
            KEY_7,      0,      // 7
            KEY_7,      0,      // 7
            KEY_DIV,    0,      // /
            KEY_ENTER,  0,      // B^
            KEY_1,      0,      // 1
            KEY_EXP,    0,      // ВП
            KEY_5,      0,      // 5
            KEY_0,      0,      // 0
            KEY_F,      0,      // F
            KEY_MUL,    0,      // x^2
            KEY_F,      0,      // F
            KEY_MUL,    0,      // x^2
            0xff,
        };

        if (! running) {
            if (test [next] == 0xff)
                next = 0;

            // Switch radians/grads/degrees mode.
            if (test [next] > 0 && test [next] < 16)
                rgd = test [next++];

            keycode = test [next++];
            key_pressed = (keycode != 0);
        }
#endif
    }
}
