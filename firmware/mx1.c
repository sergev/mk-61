/*
 * Home made board with PIC32MX120 processor.
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

/*
 * Pin assignment for pic32mx1/2 processor in DIP28 package.
 *
 *                  Bottom view
 *                  ------------
 *                  | 28     1 |
 *                  | 27     2 | RA0 - clk
 *       dot - RB15 | 26     3 | RA1 - keypad B
 * segment G - RB14 | 25     4 | RB0 - segment A, data
 * segment F - RB13 | 24     5 | RB1 - segment B
 *                  | 23     6 | RB2 - keypad A
 *                  | 22     7 | RB3 - segment C
 *                  | 21     8 |
 *                  | 20     9 |
 *                  | 19    10 |
 *   keypad E - RB9 | 18    11 | RB4 - segment E
 *   keypad D - RB8 | 17    12 | RA4 - segment D
 *   keypad C - RB7 | 16    13 |
 *                  | 15    14 | RB5 - unused
 *                  ------------
 */
#define PIN(n)  (1 << (n))

#define FLASH_BASE          (0x9d000000 + 32*1024 - 1024)

/*
 * Chip configuration.
 */
PIC32_DEVCFG (
    DEVCFG0_DEBUG_DISABLED |    /* ICE debugger disabled */
    DEVCFG0_JTAGDIS,            /* Disable JTAG port */

    DEVCFG1_FNOSC_PRIPLL |      /* Primary oscillator with PLL */
    DEVCFG1_POSCMOD_HS |        /* HS oscillator */
    DEVCFG1_FPBDIV_4 |          /* Peripheral bus clock = SYSCLK/4 */
    DEVCFG1_OSCIOFNC_OFF |      /* CLKO output disabled */
    DEVCFG1_FCKM_DISABLE,       /* Fail-safe clock monitor disable */

#ifdef CRYSTAL_12MHZ
    DEVCFG2_FPLLIDIV_3 |        /* PLL divider = 1/3 */
#else
    /* Crystal 8 MHz. */
    DEVCFG2_FPLLIDIV_2 |        /* PLL divider = 1/2 */
#endif
    DEVCFG2_UPLLDIS |           /* Disable USB PLL */
    DEVCFG2_FPLLMUL_24 |        /* PLL multiplier = 24x */
    DEVCFG2_FPLLODIV_2,         /* PLL postscaler = 1/2 */

    DEVCFG3_USERID(0xffff));    /* User-defined ID */

/*
 * Boot code at bfc00000.
 * Setup stack pointer and $gp registers, and jump to main().
 */
asm ("          .section .exception,\"ax\",@progbits");
asm ("          .globl _start");
asm ("          .type _start, function");
asm ("_start:   la      $sp, _estack");
asm ("          la      $ra, main");
asm ("          la      $gp, _gp");
asm ("          jr      $ra");
asm ("          .text");

/*
 * Display a symbol on 7-segment LED
 */
void set_segments (unsigned digit, unsigned dot, int upper_flag)
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
    if (upper_flag)
        mask &= ~(4 + 8 + 16);

    if (mask & 1)   TRISBCLR = PIN(0);  // segment A - signal RB0
    if (mask & 2)   TRISBCLR = PIN(1);  // segment B - signal RB1
    if (mask & 4)   TRISBCLR = PIN(3);  // segment C - signal RB3
    if (mask & 8)   TRISACLR = PIN(4);  // segment D - signal RA4
    if (mask & 16)  TRISBCLR = PIN(4);  // segment E - signal RB4
    if (mask & 32)  TRISBCLR = PIN(13); // segment F - signal RB13
    if (mask & 64)  TRISBCLR = PIN(14); // segment G - signal RB14
    if (dot)        TRISBCLR = PIN(15); // dot       - signal RB15
}

/*
 * Clear 7-segment LED.
 */
static inline void clear_segments()
{
    // Set segment and dot pins to tristate.
    TRISBSET = PIN(0) | PIN(1) | PIN(3) | PIN(4) |
               PIN(13) | PIN(14) | PIN(15);
    TRISASET = PIN(4);
}

/*
 * Toggle clock signal.
 */
static inline void clk()
{
    LATASET = PIN(0);                   // set clock
    LATASET = PIN(0);                   // pulse 200 usec
    LATASET = PIN(0);
    LATASET = PIN(0);
    LATASET = PIN(0);
    LATASET = PIN(0);
    LATASET = PIN(0);
    LATASET = PIN(0);

    LATACLR = PIN(0);                   // clear clock
}

/*
 * Set or clear data signal: segment A - RB0.
 */
static inline void data (int on)
{
    if (on >= 0) {
        if (on)
            LATBSET = PIN(0);           // set data
        else
            LATBCLR = PIN(0);           // clear data
        TRISBCLR = PIN(0);              // activate
    } else
        TRISBSET = PIN(0);              // tristate
}

static unsigned rgd;                    // Radians/grads/degrees
static unsigned keycode;                // Code of pressed button
static unsigned key_pressed;            // Bitmask of active key
static unsigned char prog[CODE_NBYTES]; // Program code
static unsigned char new_prog[CODE_NBYTES]; // New program code
static int new_prog_flag;               // New program received
static int flash_save_flag;             // Need to save to flash memory

/*
 * Poll keypad: input pins RD4-RD7.
 */
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
    static int radians_flag;

    int porta = PORTA;
    int portb = PORTB;
    int kp_a = portb & PIN(2);  // RB2 - keypad A
    int kp_b = porta & PIN(1);  // RA1 - keypad B
    int kp_c = portb & PIN(7);  // RB7 - keypad C
    int kp_d = portb & PIN(8);  // RB8 - keypad D
    int kp_e = portb & PIN(9);  // RB9 - keypad E

    // Poll radians/grads/degrees switch
    switch (row) {
    case 0:
        radians_flag = kp_e;
        break;
    case 7:
        if (kp_e)
            rgd = MODE_DEGREES;
        else if (radians_flag)
            rgd = MODE_RADIANS;
        else
            rgd = MODE_GRADS;
        break;
    }

    if (kp_a)
        return col_a[row];
    if (kp_b)
        return col_b[row];
    if (kp_c)
        return col_c[row];
    if (kp_d)
        return col_d[row];
    return 0;
}

/*
 * Show the next display symbol.
 * Index counter is in range 0..11.
 */
void calc_display (int i, int digit, int dot)
{
    clear_segments();
    if (i >= 0) {
        if (i == 0) {
            data (1);                   // set data
            clk();                      // toggle clock
        }
        data (0);                       // clear data
        clk();                          // toggle clock
        data (-1);                      // tristate data

        if (digit >= 0)                 // display a digit
            set_segments (digit, dot, i==2);

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

/*
 * Poll the radians/grads/degrees switch.
 */
int calc_rgd()
{
    return rgd;
}

/*
 * Poll the keypad.
 */
int calc_keypad()
{
    if (! key_pressed)
        return 0;
    return keycode;
}

/*
 * Copy an array.
 */
void memcopy (void *from, void *to, unsigned nbytes)
{
    unsigned char *src = (unsigned char*) from;
    unsigned char *dst = (unsigned char*) to;

    while (nbytes-- > 0)
        *dst++ = *src++;
}

/*
 * Poll the USB port.
 */
void calc_poll()
{
    /* Empty */
}

/*
 * Perform non-volatile memory operation.
 */
static void nvm_operation (unsigned op, unsigned address, unsigned data)
{
    int i;

    // Convert virtual address to physical
    NVMADDR = address & 0x1fffffff;
    NVMDATA = data;

    // Enable Flash Write/Erase Operations
    NVMCON = PIC32_NVMCON_WREN | op;

    // Data sheet prescribes 6us delay for LVD to become stable.
    // To be on the safer side, we shall set 7us delay.
    for (i=0; i<7*48/8; i++) {
        /* 8 clocks per cycle */
        asm volatile ("nop");
        asm volatile ("nop");
        asm volatile ("nop");
        asm volatile ("nop");
        asm volatile ("nop");
        /* gcc adds three extra instructions */
    }

    NVMKEY = 0xAA996655;
    NVMKEY = 0x556699AA;
    NVMCONSET = PIC32_NVMCON_WR;

    // Wait for WR bit to clear
    while (NVMCON & PIC32_NVMCON_WR)
        continue;

    // Disable Flash Write/Erase operations
    NVMCONCLR = PIC32_NVMCON_WREN;
}

/*
 * Check that a program is available in flash memory.
 */
int prog_available()
{
    const unsigned char *ptr = (const unsigned char*) FLASH_BASE;
    int i;

    for (i=0; i<CODE_NBYTES; i++)
        if (*ptr++ != 0xff)
            return 1;
    return 0;
}

/*
 * Check that the program has been modified by user.
 */
int prog_modified()
{
    const unsigned char *ptr = (const unsigned char*) FLASH_BASE;
    int i;

    for (i=0; i<CODE_NBYTES; i++)
        if (*ptr++ != prog[i])
            return 1;
    return 0;
}

/*
 * Read a program from flash memory.
 */
void restore_prog()
{
    void *flash = (void*) FLASH_BASE;

    memcopy (flash, new_prog, CODE_NBYTES);
}

/*
 * Write a program to flash memory.
 */
void save_prog()
{
    int i;

#if 1
    // DEBUG: set segments to visualize the operation.
    TRISBCLR = PIN(0) | PIN(1) | PIN(3) | PIN(4) |
               PIN(13) | PIN(14) | PIN(15);
    TRISACLR = PIN(4);
#endif
    /* Erase flash page. */
    nvm_operation (PIC32_NVMCON_PAGE_ERASE, FLASH_BASE, 0);

    for (i=0; i<CODE_NBYTES; i+=4) {
        /* Write word to flash memory. */
        unsigned word = prog[i] | prog[i+1] << 8 |
                        prog[i+2] << 16 | prog[i+3] << 24;
        nvm_operation (PIC32_NVMCON_WORD_PGM, FLASH_BASE + i, word);
    }
    clear_segments();
}

int main()
{
    /* Initialize coprocessor 0. */
    mtc0 (C0_COUNT, 0, 0);
    mtc0 (C0_COMPARE, 0, -1);
    mtc0 (C0_EBASE, 1, 0x9fc00000);     /* Vector base */
    mtc0 (C0_INTCTL, 1, 1 << 5);        /* Vector spacing 32 bytes */
    mtc0 (C0_CAUSE, 0, 1 << 23);        /* Set IV */
    mtc0 (C0_STATUS, 0, 0);             /* Clear BEV */

    /* Copy the .data image from flash to ram.
     * Linker places it at the end of .text segment. */
    extern void _etext();
    extern unsigned __data_start, _edata;
    unsigned *src = (unsigned*) &_etext;
    unsigned *dest = &__data_start;
    while (dest < &_edata)
        *dest++ = *src++;

    /* Initialize .bss segment by zeroes. */
    extern unsigned _end;
    dest = &_edata;
    while (dest < &_end)
        *dest++ = 0;

    /* Use all ports as digital. */
    ANSELA = 0;
    ANSELB = 0;
    LATA = 0;
    LATB = 0;

    /* Input pins: keypad.
     * Enable pull-down resistors. */
    TRISASET = PIN(1);          // keypad B - signal RA1
    TRISBSET = PIN(2) |         // keypad A - signal RB2
               PIN(9) |         // keypad E - signal RB9
               PIN(8) |         // keypad D - signal RB8
               PIN(7);          // keypad C - signal RB7
    CNPDASET = PIN(1);
    CNPDBSET = PIN(2);

    /* RA0 - clock. */
    TRISACLR = PIN(0);

    /* Output/tristate pins: segments A-G and dot. */
    clear_segments();

    int i;
    data (0);                           // clear data
    for (i=0; i<16; i++)                // clear register
        clk();
    data (-1);                          // tristate data

    calc_init();
    rgd = MODE_DEGREES;
    keycode = 0;
    key_pressed = 0;
    new_prog_flag = 0;
    flash_save_flag = 0;

    // Restore a program from flash memory.
    if (prog_available()) {
        restore_prog();
        new_prog_flag = 1;
    }

#if 1
    for (;;) {
        // Simulate one cycle of the calculator.
        int running = calc_step();

        if (running)
            continue;

        if (new_prog_flag) {
            // Got new program code - send ot to calculator engine.
            calc_write_code (new_prog);
            new_prog_flag = 0;
        } else {
            // Fetch program code.
            calc_get_code (prog);

            // Check when program has been changed and save it
            // to flash memory.
            // Need some delay here to avoid glitches.
            if (prog_modified()) {
                flash_save_flag++;
                if (flash_save_flag > 5) {
                    save_prog();
                    flash_save_flag = 0;
                }
            } else
                flash_save_flag = 0;
        }
    }
#else
    int next = 0;
    for (;;) {
        // Simulate one cycle of the calculator.
        int running = calc_step();

        // Simple test.
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
    }
#endif
}
