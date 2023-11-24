/*
 * К145ИР2 chip.
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
// Initialize the FIFO register data structure.
//
void fifo_init (fifo_t *t)
{
    int i;

    for (i=0; i<FIFO_NWORDS; i++)
        t->data[i] = 0;
    t->input = 0;
    t->output = 0;
    t->cycle = 0;
}

//
// Simulate one cycle of the FIFO chip.
//
void fifo_step (fifo_t *t)
{
    t->output = t->data[t->cycle];
    t->data[t->cycle] = t->input;
    t->cycle++;
    if (t->cycle >= FIFO_NWORDS)
        t->cycle = 0;
}
