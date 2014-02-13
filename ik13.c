/*
 * К145ИК130x chip.
 * Based on sources of emu145 project: https://code.google.com/p/emu145/
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

//
// Initialize the PLM data structure.
//
void plm_init (plm_t *t, const unsigned inst_rom[],
    const unsigned cmd_rom[], const unsigned char prog_rom[])
{
    int i;

    t->inst_rom = inst_rom;
    t->cmd_rom = cmd_rom;
    t->prog_rom = prog_rom;

    for (i=0; i<REG_NWORDS; i++) {
        t->R[i] = 0;
        t->M[i] = 0;
        t->ST[i] = 0;
    }
    t->input = 0;
    t->output = 0;
    t->S = 0;
    t->Q = 0;
    t->carry = 0;
    t->keypad_event = 0;
    t->opcode = 0;
    t->keyb_x = 0;
    t->keyb_y = 0;
    t->dot = 0;
    t->command = 0;
    t->enable_display = 0;
    for (i=0; i<14; i++) {
        t->show_dot[i] = 0;
    }
}

//
// Simulate one cycle of the PLM chip.
//
void plm_step (plm_t *t, unsigned cycle)
{
    /* D stage in range 0...13 */
    unsigned d = cycle / 3;

    /*
     * Fetch program counter from the R register.
     */
    if (cycle == 0) {
        unsigned pc = t->R[36] + (t->R[39] << 4);

        t->command = t->cmd_rom[pc];
        if ((t->command & 0xfc0000) == 0)
            t->keypad_event = 0;
    }

    /*
     * Use PC to get the program index.
     */
    unsigned prog_index;
    if (cycle < 27)
        prog_index = t->command & 0xff;
    else if (cycle < 36)
        prog_index = (t->command >> 8) & 0xff;
    else {
        prog_index = (t->command >> 16) & 0xff;
        if (prog_index > 0x1f) {
            if (cycle == 36) {
                t->R[37] = prog_index & 0xf;
                t->R[40] = prog_index >> 4;
            }
            prog_index = 0x5f;
        }
    }
    unsigned modifier = (t->command >> 24) & 0xff;

    /*
     * Fetch the instruction opcode.
     */
    const unsigned char remap[42] = {
        0, 1, 2, 3, 4, 5, 3, 4, 5, 3, 4, 5, 3, 4,
        5, 3, 4, 5, 3, 4, 5, 3, 4, 5, 6, 7, 8, 0,
        1, 2, 3, 4, 5, 6, 7, 8, 0, 1, 2, 3, 4, 5,
    };
    unsigned inst_addr = t->prog_rom[prog_index*9 + remap[cycle]] & 0x3f;
    if (inst_addr >= 60) {
        inst_addr += inst_addr - 60;
        if (! t->carry)
            inst_addr++;
    }
    t->opcode = t->inst_rom[inst_addr];

    /*
     * Execute the opcode.
     */
    unsigned alpha = 0, beta = 0, gamma = 0;
    if ((t->opcode & UCMD_Q_MASK) == UCMD_Q_QSUM) {
        if (d != (t->keyb_x - 1) && t->keyb_y > 0)
            t->Q |= t->keyb_y;
    }

    /* Alpha. */
    if (t->opcode & UCMD_ALPHA_R)   alpha |= t->R[cycle];
    if (t->opcode & UCMD_ALPHA_M)   alpha |= t->M[cycle];
    if (t->opcode & UCMD_ALPHA_ST)  alpha |= t->ST[cycle];
    if (t->opcode & UCMD_ALPHA_NR)  alpha |= t->R[cycle] ^ 0xf;
    if (t->opcode & UCMD_ALPHA_C10
        && ! t->carry)              alpha |= 0xa;
    if (t->opcode & UCMD_ALPHA_S)   alpha |= t->S;
    if (t->opcode & UCMD_ALPHA_4)   alpha |= 4;

    /* Beta. */
    if (t->opcode & UCMD_BETA_S)    beta |= t->S;
    if (t->opcode & UCMD_BETA_NS)   beta |= t->S ^ 0xf;
    if (t->opcode & UCMD_BETA_Q)    beta |= t->Q;
    if (t->opcode & UCMD_BETA_6)    beta |= 6;
    if (t->opcode & UCMD_BETA_1)    beta |= 1;

    /*
     * Poll keypad.
     */
    if (t->command & 0xfc0000) {
        if (t->keyb_y == 0)
            t->keypad_event = 0;
    } else {
        t->enable_display = 1;
        if (d == (t->keyb_x - 1)) {
            if (t->keyb_y > 0) {
                t->Q = t->keyb_y;
                t->keypad_event = 1;
            }
        }
        if (t->carry && d < 12)
            t->dot = d;
        t->show_dot[d] = t->carry;
    }

    /* Gamma. */
    if (t->opcode & UCMD_GAMMA_CARRY)   gamma |= t->carry;
    if (t->opcode & UCMD_GAMMA_NCARRY)  gamma |= t->carry ^ 1;
    if (t->opcode & UCMD_GAMMA_NKEY)    gamma |= t->keypad_event ^ 1;

    /*
     * Compute sum and carry.
     */
    unsigned sum = alpha + beta + gamma;
    if (t->opcode & UCMD_HOLD_CARRY)
        t->carry = (sum >> 4) & 1;
    sum &= 0xf;

    if (modifier == 0 || cycle >= 36) {
        /*
         * Update R register.
         */
        unsigned cycle_plus_3 = cycle + 3;
        unsigned cycle_minus_1 = cycle - 1 + REG_NWORDS;
        unsigned cycle_minus_2 = cycle - 2 + REG_NWORDS;
        if (cycle_plus_3 >= REG_NWORDS)
            cycle_plus_3 -= REG_NWORDS;
        if (cycle_minus_1 >= REG_NWORDS)
            cycle_minus_1 -= REG_NWORDS;
        if (cycle_minus_2 >= REG_NWORDS)
            cycle_minus_2 -= REG_NWORDS;

        switch (t->opcode & UCMD_R_MASK) {
        case UCMD_R_R3:     t->R[cycle]  = t->R[cycle_plus_3];  break;
        case UCMD_R_SUM:    t->R[cycle]  = sum;                 break;
        case UCMD_R_S:      t->R[cycle]  = t->S;                break;
        case UCMD_R_RSSUM:  t->R[cycle] |= t->S | sum;          break;
        case UCMD_R_SSUM:   t->R[cycle]  = t->S | sum;          break;
        case UCMD_R_RS:     t->R[cycle] |= t->S;                break;
        case UCMD_R_RSUM:   t->R[cycle] |= sum;                 break;
        }
        if (t->opcode & UCMD_R1_SUM) t->R[cycle_minus_1] = sum;
        if (t->opcode & UCMD_R2_SUM) t->R[cycle_minus_2] = sum;
    }

    /*
     * Update M register.
     */
    if (t->opcode & UCMD_M_S)
        t->M[cycle] = t->S;

    /*
     * Update S register.
     */
    switch (t->opcode & UCMD_S_MASK) {
    case UCMD_S_Q:      t->S = t->Q;        break;
    case UCMD_S_SUM:    t->S = sum;         break;
    case UCMD_S_QSUM:   t->S = t->Q | sum;  break;
    }

    /*
     * Update Q register.
     */
    switch (t->opcode & UCMD_Q_MASK) {
    case UCMD_Q_SUM:    t->Q = sum;     break;
    case UCMD_Q_QSUM:   t->Q |= sum;    break;
    }

    /*
     * Update ST register.
     */
    unsigned cycle_plus_1 = cycle + 1;
    unsigned cycle_plus_2 = cycle + 2;
    if (cycle_plus_1 >= REG_NWORDS)
        cycle_plus_1 = 0;
    if (cycle_plus_2 >= REG_NWORDS)
        cycle_plus_2 -= REG_NWORDS;

    if (t->opcode & UCMD_ST_SUM) {
        t->ST[cycle_plus_2] = t->ST[cycle_plus_1];
        t->ST[cycle_plus_1] = t->ST[cycle];
        t->ST[cycle]        = sum;
    }
    if (t->opcode & UCMD_ST_ROT) {
        unsigned x = t->ST[cycle];
        t->ST[cycle]        = t->ST[cycle_plus_1];
        t->ST[cycle_plus_1] = t->ST[cycle_plus_2];
        t->ST[cycle_plus_2] = x;
    }

    /*
     * Store input, pass output.
     */
    t->output = t->M[cycle] & 0xf;
    t->M[cycle] = t->input;
}
