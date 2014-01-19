/*
 * Routines for MK-54/MK-61 calculator opcodes.
 *
 * Copyright (C) 2014 Serge Vakulenko
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
#include <string.h>

typedef struct {
    unsigned char opcode;
    unsigned char type;
    char *mnemonics;
} opcode_t;

static opcode_t opcodes[] = {
    { 0x0A, 0, ","        },
    { 0x0B, 0, "/-/"      },
    { 0x0C, 0, "ВП"       },
    { 0x0D, 0, "Сx"       },
    { 0x0E, 0, "В^"       },
    { 0x0F, 0, "F Вx"     },
    { 0x10, 0, "+"        },
    { 0x11, 0, "-"        },
    { 0x12, 0, "x"        },
    { 0x13, 0, "/"        },
    { 0x14, 0, "<->"      },
    { 0x15, 0, "F 10^x"   },
    { 0x16, 0, "F e^x"    },
    { 0x17, 0, "F lg"     },
    { 0x18, 0, "F ln"     },
    { 0x19, 0, "F arcsin" },
    { 0x1A, 0, "F arccos" },
    { 0x1B, 0, "F arctg"  },
    { 0x1C, 0, "F sin"    },
    { 0x1D, 0, "F cos"    },
    { 0x1E, 0, "F tg"     },
    { 0x20, 0, "F пи"     },
    { 0x21, 0, "F корень" },
    { 0x22, 0, "F x^2"    },
    { 0x23, 0, "F 1/x"    },
    { 0x24, 0, "F x^y"    },
    { 0x25, 0, "F o"      },
    { 0x26, 0, "K МГ"     },
    { 0x27, 0, "K -"      },
    { 0x28, 0, "K x"      },
    { 0x29, 0, "K /"      },
    { 0x2A, 0, "K МЧ"     },
    { 0x30, 0, "K ЧМ"     },
    { 0x31, 0, "K |x|"    },
    { 0x32, 0, "K ЗН"     },
    { 0x33, 0, "K ГМ"     },
    { 0x34, 0, "K [x]"    },
    { 0x35, 0, "K {x}"    },
    { 0x36, 0, "K max"    },
    { 0x37, 0, "K /\\"    },
    { 0x38, 0, "K \\/"    },
    { 0x39, 0, "K (+)"    },
    { 0x3A, 0, "K ИНВ"    },
    { 0x3B, 0, "K СЧ"     },
    { 0x40, 1, "xП"       },
    { 0x50, 0, "С/П"      },
    { 0x51, 2, "БП"       },
    { 0x52, 0, "В/О"      },
    { 0x53, 2, "ПП"       },
    { 0x54, 0, "K НОП"    },
    { 0x55, 0, "K 1"      },
    { 0x56, 0, "K 2"      },
    { 0x57, 2, "F x#0"    },
    { 0x58, 2, "F L2"     },
    { 0x59, 2, "F x>=0"   },
    { 0x5A, 2, "F L3"     },
    { 0x5B, 2, "F L1"     },
    { 0x5C, 2, "F x<0"    },
    { 0x5D, 2, "F L0"     },
    { 0x5E, 2, "F x=0"    },
    { 0x60, 1, "Пx"       },
    { 0x70, 1, "K x#0"    },
    { 0x80, 1, "K БП"     },
    { 0x90, 1, "K x>=0"   },
    { 0xA0, 1, "K ПП"     },
    { 0xB0, 1, "K xП"     },
    { 0xC0, 1, "K x<0"    },
    { 0xD0, 1, "K Пx"     },
    { 0xE0, 1, "K x=0"    },
    { 0 },
};

static const char hex_char[16] = "0123456789ABCDEF";
static const char *hex_string[16] = {"0","1","2","3","4","5","6","7",
                                     "8","9","a","b","c","d","e","f"};

//
// Disassemble the opcode.
//
char *decompile (unsigned opcode, int *address_flag)
{
    static char cmd[16];
    opcode_t *p;

    if (*address_flag) {
        if (opcode >= 0xA0 && opcode < 0xA5) {
            cmd[0] = '.';
        } else {
            cmd[0] = hex_char[opcode >> 4];
        }
        cmd[1] = hex_char[opcode & 15];
        cmd[2] = 0;
        *address_flag = 0;
    } else {
        for (p=opcodes; p->mnemonics; p++) {
            if (p->type == 1) {
                if ((opcode >> 4) == (p->opcode >> 4) && (opcode & 15) < 15) {
                    strcpy (cmd, p->mnemonics);
                    strcat (cmd, " ");
                    strcat (cmd, hex_string[opcode & 15]);
                    break;
                }
            } else if (opcode == p->opcode) {
                strcpy (cmd, p->mnemonics);
                *address_flag = (p->type == 2);
                break;
            }
        }
        if (! p->mnemonics) {
            if (opcode <= 9) {
                cmd[0] = hex_char[opcode & 15];
                cmd[1] = 0;
            } else {
                cmd[0] = hex_char[opcode >> 4];
                cmd[1] = hex_char[opcode & 15];
                cmd[2] = 0;
            }
        }
    }
    return cmd;
}

#ifdef TEST
#include <stdio.h>

int main()
{
    int opcode, i;
    opcode_t *p;

    printf ("^ Opcode ");
    for (i=0; i<=0xf; i++)
        printf ("^ %X ", i);
    printf ("^\n");
    for (opcode=0; opcode<=0xff; opcode++) {
        if ((opcode & 0xf) == 0)
            printf ("^ %X ", opcode >> 4);
        for (p=opcodes; p->mnemonics; p++) {
            if (p->type == 1) {
                if ((opcode >> 4) == (p->opcode >> 4) && (opcode & 15) < 15) {
                    printf ("| <html>%s</html> %X ", p->mnemonics, opcode & 15);
                    break;
                }
            } else if (opcode == p->opcode) {
                printf ((p->type == 2) ? "^ " : "| ");
                printf ("<html>%s</html> ", p->mnemonics);
                break;
            }
        }
        if (! p->mnemonics) {
            if (opcode <= 9)
                printf ("| %X ", opcode);
            else
                printf ("|  ");
        }
        if ((opcode & 0xf) == 0xf)
            printf ("|\n");
    }
    return 0;
}
#endif
