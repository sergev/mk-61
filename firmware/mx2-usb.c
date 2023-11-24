/*
 * Home made board with PIC32MX220 processor and USB port.
 * External crystal 12 MHz used as a clock source.
 * USB driver based on open sources from Microchip library.
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
#include "usb-device.h"
#include "usb-function-hid.h"

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
 * HID packet structure.
 */
#define	PACKET_SIZE	    64      // HID packet size

/*
 * HID commands.
 */
#define CMD_QUERY_DEVICE    0xc1    // Get generic status
#define CMD_READ_STACK      0xc2    // Read X, Y, Z, T, X1 values
#define CMD_READ_REG_LOW    0xc3    // Read registers 0-7
#define CMD_READ_REG_HIGH   0xc4    // Read registers 8-E
#define CMD_READ_PROG_LOW   0xc5    // Read program 0-59
#define CMD_READ_PROG_HIGH  0xc6    // Read program 60-105
#define CMD_WRITE_PROG_LOW  0xc7    // Write program 0-59
#define CMD_WRITE_PROG_HIGH 0xc8    // Write program 60-105

typedef unsigned char packet_t [PACKET_SIZE];

static packet_t send;           // 64-byte send buffer (EP1 IN to the PC)
static packet_t receive_buffer; // 64-byte receive buffer (EP1 OUT from the PC)
static packet_t receive;        // copy of receive buffer, for processing

static USB_HANDLE request_handle;
static USB_HANDLE reply_handle;
static int packet_received;

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
    DEVCFG2_UPLLIDIV_3 |        /* USB PLL divider = 1/3 */
#else
    /* Crystal 8 MHz. */
    DEVCFG2_FPLLIDIV_2 |        /* PLL divider = 1/2 */
    DEVCFG2_UPLLIDIV_2 |        /* USB PLL divider = 1/2 */
#endif
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
static unsigned char stack[5][6];       // Stack values X, Y, Z, T, X1
static unsigned char regs[DATA_NREGS][6]; // Memory registers 0..9, A..D
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
 * Clear an array.
 */
void memzero (void *data, unsigned nbytes)
{
    unsigned char *charp = (unsigned char*) data;

    while (nbytes-- > 0)
        *charp++ = 0;
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
 * A command packet was received from PC.
 * Process it and return 1 when a reply is ready.
 */
static int handle_packet()
{
    int i;

    switch (receive[0]) {
    case CMD_QUERY_DEVICE:      // Get generic status
        memzero (&send, PACKET_SIZE);
        send[0] = receive[0];
        send[1] = 2;
        return 1;
    case CMD_READ_STACK:        // Read X, Y, Z, T, X1 values
        memzero (&send, PACKET_SIZE);
        send[0] = receive[0];
        send[1] = 2 + 6*5;
        for (i=0; i<5; i++) {
            memcopy (&stack[i], &send[2 + i*6], 6);
        }
        return 1;
    case CMD_READ_REG_LOW:      // Read registers 0..7
        memzero (&send, PACKET_SIZE);
        send[0] = receive[0];
        send[1] = 2 + 6 * 8;
        for (i=0; i<8; i++) {
            memcopy (&regs[i], &send[2 + i*6], 6);
        }
        return 1;
    case CMD_READ_REG_HIGH:     // Read registers 8..D or E
        memzero (&send, PACKET_SIZE);
        send[0] = receive[0];
        send[1] = 2 + 6 * (DATA_NREGS - 8);
        for (i=0; i<DATA_NREGS-8; i++) {
            memcopy (&regs[i+8], &send[2 + i*6], 6);
        }
        return 1;
    case CMD_READ_PROG_LOW:     // Read program code 0..59
        memzero (&send, PACKET_SIZE);
        send[0] = receive[0];
        send[1] = 2 + 60;
        memcopy (&prog[0], &send[2], 60);
        return 1;
    case CMD_READ_PROG_HIGH:    // Read program code 60..98 or 105
        memzero (&send, PACKET_SIZE);
        send[0] = receive[0];
        send[1] = 2 + CODE_NBYTES - 60;
        memcopy (&prog[60], &send[2], CODE_NBYTES - 60);
        return 1;
    case CMD_WRITE_PROG_LOW:    // Write program 0-59
        memcopy (&receive[2], &new_prog[0], 60);
        memzero (&send, PACKET_SIZE);
        send[0] = receive[0];
        send[1] = 2;
        new_prog_flag = 1;
        return 1;
    case CMD_WRITE_PROG_HIGH:   // Write program 60-105
        memcopy (&receive[2], &new_prog[60], CODE_NBYTES - 60);
        memzero (&send, PACKET_SIZE);
        send[0] = receive[0];
        send[1] = 2;
        return 1;
    }
    return 0;
}

/*
 * Poll the USB port.
 */
void calc_poll()
{
    // Check bus status and service USB interrupts.
    usb_device_tasks();
    if (usb_device_state < CONFIGURED_STATE || usb_is_device_suspended())
        return;

    // Are we done sending the last response.  We need to be before we
    // receive the next command because we clear the send buffer
    // once we receive a command
    if (reply_handle != 0) {
        if (usb_handle_busy (reply_handle))
            return;
        reply_handle = 0;
    }

    if (packet_received) {
        if (handle_packet())
            reply_handle = usb_transfer_one_packet (HID_EP,
                IN_TO_HOST, &send[0], PACKET_SIZE);
        packet_received = 0;
        return;
    }

    // Did we receive a command?
    if (usb_handle_busy (request_handle))
        return;

    // Make a copy of received data.
    memcopy (&receive_buffer, &receive, PACKET_SIZE);
    packet_received = 1;

    // Restart receiver, to be ready for a next packet.
    request_handle = usb_transfer_one_packet (HID_EP, OUT_FROM_HOST,
        (unsigned char*) &receive_buffer, PACKET_SIZE);
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

/*
 * Main program entry point.
 */
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

    // Initialize USB module SFRs and firmware variables to known states.
    // USB port uses pins B10 and B11.
    TRISBSET = PIN(10) | PIN(11);
    usb_device_init();

    for (;;) {
        // Simulate one cycle of the calculator.
        int running = calc_step();

        // Fetch data stack and registers.
        calc_get_stack (stack);
        calc_get_regs (regs);
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
}

/*
 * USB Callback Functions
 */
/*
 * Process device-specific SETUP requests.
 */
void usbcb_check_other_req()
{
    hid_check_request();
}

/*
 * This function is called when the device becomes initialized.
 * It should initialize the endpoints for the device's usage
 * according to the current configuration.
 */
void usbcb_init_ep()
{
    // Enable the HID endpoint
    usb_enable_endpoint (HID_EP, USB_IN_ENABLED | USB_OUT_ENABLED |
        USB_HANDSHAKE_ENABLED | USB_DISALLOW_SETUP);

    // Arm the OUT endpoint for the first packet
    request_handle = usb_rx_one_packet (HID_EP,
        &receive_buffer[0], PACKET_SIZE);
}

/*
 * The device descriptor.
 */
const USB_DEVICE_DESCRIPTOR usb_device = {
    sizeof (usb_device),    // Size of this descriptor in bytes
    USB_DESCRIPTOR_DEVICE,  // DEVICE descriptor type
    0x0200,                 // USB Spec Release Number in BCD format
    0x00,                   // Class Code
    0x00,                   // Subclass code
    0x00,                   // Protocol code
    USB_EP0_BUFF_SIZE,      // Max packet size for EP0
    0xca1c,                 // Bogus vendor ID: 'calc'
#ifdef MK_54
    0x0054,                 // Product ID: MK-54
#else
    0x0061,                 // Product ID: MK-61
#endif
    0x0101,                 // Device release number in BCD format
    1,                      // Manufacturer string index
    2,                      // Product string index
    0,                      // Device serial number string index
    1,                      // Number of possible configurations
};

/*
 * Configuration 1 descriptor
 */
const unsigned char usb_config1_descriptor[] = {
    /*
     * Configuration descriptor
     */
    sizeof (USB_CONFIGURATION_DESCRIPTOR),
    USB_DESCRIPTOR_CONFIGURATION,
    0x29, 0x00,             // Total length of data for this cfg
    1,                      // Number of interfaces in this cfg
    1,                      // Index value of this configuration
    0,                      // Configuration string index
    _DEFAULT | _SELF,       // Attributes
    200 / 2,                // Max power consumption (2X mA)

    /*
     * Interface descriptor
     */
    sizeof (USB_INTERFACE_DESCRIPTOR),
    USB_DESCRIPTOR_INTERFACE,
    0,                      // Interface Number
    0,                      // Alternate Setting Number
    2,                      // Number of endpoints in this intf
    HID_INTF,               // Class code
    0,                      // Subclass code
    0,                      // Protocol code
    0,                      // Interface string index

    /*
     * HID Class-Specific descriptor
     */
    sizeof (USB_HID_DSC) + 3,
    DSC_HID,
    0x11, 0x01,             // HID Spec Release Number in BCD format (1.11)
    0x00,                   // Country Code (0x00 for Not supported)
    HID_NUM_OF_DSC,         // Number of class descriptors
    DSC_RPT,                // Report descriptor type
    HID_RPT01_SIZE, 0x00,   // Size of the report descriptor

    /*
     * Endpoint descriptor
     */
    sizeof (USB_ENDPOINT_DESCRIPTOR),
    USB_DESCRIPTOR_ENDPOINT,
    HID_EP | _EP_IN,        // EndpointAddress
    _INTERRUPT,             // Attributes
    PACKET_SIZE, 0,         // Size
    1,                      // Interval

    /*
     * Endpoint descriptor
     */
    sizeof (USB_ENDPOINT_DESCRIPTOR),
    USB_DESCRIPTOR_ENDPOINT,
    HID_EP | _EP_OUT,       // EndpointAddress
    _INTERRUPT,             // Attributes
    PACKET_SIZE, 0,         // Size
    1,                      // Interval
};

/*
 * Class specific descriptor - HID
 */
const unsigned char hid_rpt01 [HID_RPT01_SIZE] = {
    0x06, 0x00, 0xFF,       // Usage Page = 0xFF00 (Vendor Defined Page 1)
    0x09, 0x01,             // Usage (Vendor Usage 1)
    0xA1, 0x01,             // Collection (Application)

    0x19, 0x01,             // Usage Minimum
    0x29, 0x40,             // Usage Maximum 64 input usages total (0x01 to 0x40)
    0x15, 0x00,             // Logical Minimum (data bytes in the report may have minimum value = 0x00)
    0x26, 0xFF, 0x00,       // Logical Maximum (data bytes in the report may have maximum value = 0x00FF = unsigned 255)
    0x75, 0x08,             // Report Size: 8-bit field size
    0x95, 0x40,             // Report Count: Make sixty-four 8-bit fields (the next time the parser hits an "Input", "Output", or "Feature" item)
    0x81, 0x00,             // Input (Data, Array, Abs): Instantiates input packet fields based on the above report size, count, logical min/max, and usage.

    0x19, 0x01,             // Usage Minimum
    0x29, 0x40,             // Usage Maximum 64 output usages total (0x01 to 0x40)
    0x91, 0x00,             // Output (Data, Array, Abs): Instantiates output packet fields.  Uses same report size and count as "Input" fields, since nothing new/different was specified to the parser since the "Input" item.

    0xC0,                   // End Collection
};

/*
 * USB Strings
 */
static const USB_STRING_INIT(1) string0_descriptor = {
    sizeof(string0_descriptor),
    USB_DESCRIPTOR_STRING,              /* Language code */
    { 0x0409 },
};

static const USB_STRING_INIT(29) string1_descriptor = {
    sizeof(string1_descriptor),
    USB_DESCRIPTOR_STRING,              /* Manufacturer */
    { 'S','e','r','g','e','y',' ','V','a','k',
      'u','l','e','n','k','o',' ','s','e','r',
      'g','e','@','v','a','k','.','r','u' },
};

static const USB_STRING_INIT(16) string2_descriptor = {
    sizeof(string2_descriptor),
    USB_DESCRIPTOR_STRING,              /* Product */
#ifdef MK_54
    { 'M','K','-','5','4',' ','C','a','l','c',
      'u','l','a','t','o','r' },
#else
    { 'M','K','-','6','1',' ','C','a','l','c',
      'u','l','a','t','o','r' },
#endif
};

/*
 * Array of configuration descriptors
 */
const unsigned char *const usb_config[] = {
    (const unsigned char *const) &usb_config1_descriptor,
};

/*
 * Array of string descriptors
 */
const unsigned char *const usb_string[USB_NUM_STRING_DESCRIPTORS] = {
    (const unsigned char *const) &string0_descriptor,
    (const unsigned char *const) &string1_descriptor,
    (const unsigned char *const) &string2_descriptor,
};
