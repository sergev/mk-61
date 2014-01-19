/*
 * Parser for MK-61 source files.
 *
 * Copyright (C) 2014 Serge Vakulenko, <serge@vak.ru>
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
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

/*
 * Types of lexemes.
 */
enum {
    LEOF = 1,           /* end of file */
    LEOL,               /* end of line */
    LNUM,               /* integer number */
    LNAME,              /* label identifier */
    LFUNC,              /* F key */
    LKKEY,              /* K key */
    LCMD,               /* instruction */
};

/*
 * Sizes of tables.
 * Hash sizes should be powers of 2!
 */
#define HASHSZ  256             /* symbol name hash table size */
#define HCMDSZ  256             /* instruction hash table size */
#define STSIZE  (HASHSZ*9/10)   /* symbol name table size */

/*
 * Main opcode table.
 */
struct optable {
    unsigned opcode;                    /* instruction code */
    unsigned f_opcode;                  /* code with F prefix */
    unsigned k_opcode;                  /* code with K prefix */
    const char *name;                   /* instruction name */
    unsigned type;                      /* flags */
};

#define FPREF   0x0001                  /* Need F prefix */
#define FPREK   0x0002                  /* Need K prefix */
#define FREG    0x0004                  /* Register number follows */
#define FADDR   0x0008                  /* Jump address follows */

static const struct optable optable [] = {
    /* Norm   F     K   Name        Flags */
    { 0x00,    0,    0, "0",        },
    { 0x01,    0, 0x55, "1",        },          // ??
    { 0x02,    0, 0x56, "2",        },          // ??
    { 0x03,    0,    0, "3",        },
    { 0x04,    0,    0, "4",        },
    { 0x05,    0,    0, "5",        },
    { 0x06,    0,    0, "6",        },
    { 0x07,    0,    0, "7",        },
    { 0x08,    0,    0, "8",        },
    { 0x09,    0,    0, "9",        },
    { 0x0a,    0,    0, ",",        },
    { 0x0b,    0,    0, "/-/",      },
    { 0x0c,    0,    0, "ВП",       },
    { 0x0d,    0,    0, "Cx",       },
    { 0x0e,    0,    0, "B^",       },
    {    0, 0x0f,    0, "Bx",       },
    { 0x10,    0,    0, "+",        },
    { 0x11,    0, 0x27, "-",        },
    { 0x12,    0, 0x28, "x",        },
    { 0x12,    0, 0x28, "*",        },
    { 0x13,    0, 0x29, "/",        },
    { 0x14,    0,    0, "<->",      },
    {    0, 0x15,    0, "10^x",     },
    {    0, 0x16,    0, "e^x",      },
    {    0, 0x17,    0, "lg",       },
    {    0, 0x18,    0, "ln",       },
    {    0, 0x19,    0, "arcsin",   },
    {    0, 0x1a,    0, "arccos",   },
    {    0, 0x1b,    0, "arctg",    },
    {    0, 0x1c,    0, "sin",      },
    {    0, 0x1d,    0, "cos",      },
    {    0, 0x1e,    0, "tg",       },
    {    0, 0x20,    0, "пи",       },
    {    0, 0x20,    0, "pi",       },
    {    0, 0x20,    0, "@",        },            // F π
    {    0, 0x21,    0, "корень",   },
    {    0, 0x21,    0, "sqrt",     },
    {    0, 0x22,    0, "x^2",      },
    {    0, 0x23,    0, "1/x",      },
    {    0, 0x24,    0, "x^y",      },
    {    0, 0x25,    0, "O",        },            // F ⟳
    {    0, 0x25,    0, "o",        },
    {    0,    0, 0x26, "MГ",       },
    {    0,    0, 0x2a, "MЧ",       },
    {    0,    0, 0x30, "ЧM",       },
    {    0,    0, 0x31, "|x|",      },
    {    0,    0, 0x32, "ЗH",       },
    {    0,    0, 0x33, "ГM",       },
    {    0,    0, 0x34, "[x]",      },
    {    0,    0, 0x35, "{x}",      },
    {    0,    0, 0x36, "max",      },
    {    0,    0, 0x37, "/\\",      },
    {    0,    0, 0x38, "\\/",      },
    {    0,    0, 0x39, "(+)",      },
    {    0,    0, 0x3a, "ИHB",      },
    {    0,    0, 0x3b, "CЧ",       },
    { 0x40,    0, 0xb0, "xП",       FREG },
    { 0x50,    0,    0, "C/П",      },
    { 0x51,    0, 0x80, "БП",       FADDR },    // FREG for K
    { 0x52,    0,    0, "B/O",      },
    { 0x53,    0, 0xa0, "ПП",       FADDR },    // FREG for K
    {    0,    0, 0x54, "HOП",      },
    {    0, 0x57, 0x70, "x#0",      FADDR },    // FREG for K
    {    0, 0x58,    0, "L2",       FADDR },
    {    0, 0x59, 0x90, "x~0",      FADDR },    // F x≥0, FREG for K
    {    0, 0x59, 0x90, "x>=0",     FADDR },    // FREG for K
    {    0, 0x5a,    0, "L3",       FADDR },
    {    0, 0x5b,    0, "L1",       FADDR },
    {    0, 0x5c, 0xc0, "x<0",      FADDR },    // FREG for K
    {    0, 0x5d,    0, "L0",       FADDR },
    {    0, 0x5e, 0xe0, "x=0",      FADDR },    // FREG for K
    { 0x60,    0, 0xd0, "Пx",       FREG },
    { 0 },
};

static struct {
    char *name;
    unsigned len;
    unsigned value;
    unsigned undef;
} label [STSIZE];

static unsigned count;
static unsigned char *code;
static unsigned char labelref[105];
static char *infile;
static int line;                               /* Source line number */
static int labelfree;
static char space [STSIZE*8];                  /* Area for symbol names */
static int lastfree;                           /* Free space offset */
static char name [256];
static int intval;
static int blexflag, backlex;
static short hashtab [HASHSZ], hashctab [HCMDSZ];

/*
 * Fatal error message.
 */
static void uerror (char *fmt, ...)
{
    va_list ap;

    va_start (ap, fmt);
    fprintf (stderr, "as: ");
    if (infile)
        fprintf (stderr, "%s, ", infile);
    if (line)
        fprintf (stderr, "%d: ", line);
    vfprintf (stderr, fmt, ap);
    va_end (ap);
    fprintf (stderr, "\n");
    exit (1);
}

/*
 * Suboptimal 32-bit hash function.
 * Copyright (C) 2006 Serge Vakulenko.
 */
static unsigned hash_rot13 (s)
    const char *s;
{
    unsigned hash, c;

    hash = 0;
    while ((c = (unsigned char) *s++) != 0) {
        hash += c;
        hash -= (hash << 13) | (hash >> 19);
    }
    return hash;
}

static void hashinit()
{
    int i, h;
    const struct optable *p;

    for (i=0; i<HCMDSZ; i++)
        hashctab[i] = -1;
    for (p=optable; p->name; p++) {
        h = hash_rot13 (p->name) & (HCMDSZ-1);
        while (hashctab[h] != -1)
            if (--h < 0)
                h += HCMDSZ;
        hashctab[h] = p - optable;
    }
    for (i=0; i<HASHSZ; i++)
        hashtab[i] = -1;
}

/*
 * Get decimal number.
 */
static void getnum (c)
    int c;
{
    char *cp;

    for (cp=name; c>='0' && c<='9'; c=getchar())
        *cp++ = c - '0';
    ungetc (c, stdin);
    intval = 0;
    for (c=1; ; c*=10) {
        if (--cp < name)
            return;
        intval += *cp * c;
    }
}

/*
 * Read a name and store it into name[] array.
 */
static void getname (c)
    int c;
{
    char *cp;

    for (cp=name; c>=0 && !isspace(c); c=getchar()) {
        if (c & 0x80) {
            // Parse utf8 encoding.
            int c2 = getchar();
            if (! (c & 0x20)) {
                // Convert cyrillics and symbol π
                switch (c<<8 | c2) {
                case 0xd092: c = 'B'; break;    // В
                case 0xd09a: c = 'K'; break;    // К
                case 0xd09c: c = 'M'; break;    // М
                case 0xd09d: c = 'H'; break;    // Н
                case 0xd09e: c = 'O'; break;    // О
                case 0xd0a1: c = 'C'; break;    // С
                case 0xd0be: c = 'o'; break;    // о
                case 0xd183: c = 'y'; break;    // у
                case 0xd185: c = 'x'; break;    // х
                case 0xcf80: c = '@'; break;    // π -> @
                default:
                    *cp++ = c;
                    c = c2;
                }
            } else {
                int c3 = getchar();
                // Convert symbols ⟳, ≥, –, ≠
                switch (c<<16 | c2<<8 | c3) {
                case 0xe28093: c = '-'; break;  // – -> -
                case 0xe289a0: c = '#'; break;  // ≠ -> #
                case 0xe289a5: c = '~'; break;  // ≥ -> ~
                case 0xe29fb3: c = 'O'; break;  // ⟳ -> O
                case 0xefbbbf: continue;        // Skip zero width space
                default:
                    *cp++ = c;
                    *cp++ = c2;
                    c = c3;
                }
            }
        }
        *cp++ = c;
        *cp = 0;

        /* Detect prefixes. */
        static const char *prefixes[] = {
            "F", "K", "БП", "ПП", "xП", "Пx",
            "x#0", "x~0", "x>=0", "x<0", "x=0", 0
        };
        const char **pp;
        for (pp=prefixes; *pp; pp++)
            if (strcmp (*pp, name) == 0)
                return;
    }
    ungetc (c, stdin);
}

static int lookcmd()
{
    int i, h;

    h = hash_rot13 (name) & (HCMDSZ-1);
    while ((i = hashctab[h]) != -1) {
        if (! strcmp (optable[i].name, name))
            return (i);
        if (--h < 0)
            h += HCMDSZ;
    }
    return (-1);
}

static char *alloc (len)
{
    int r;

    r = lastfree;
    lastfree += len;
    if (lastfree > sizeof(space))
        uerror ("out of memory");
    return (space + r);
}

static int looklabel()
{
    int i, h;

    /* Search for symbol name. */
    h = hash_rot13 (name) & (HASHSZ-1);
    while ((i = hashtab[h]) != -1) {
        if (! strcmp (label[i].name, name))
            return (i);
        if (--h < 0)
            h += HASHSZ;
    }

    /* Add a new symbol to table. */
    i = labelfree++;
    if (i >= STSIZE)
        uerror ("symbol table overflow");
    label[i].len = strlen (name);
    label[i].name = alloc (1 + label[i].len);
    strcpy (label[i].name, name);
    label[i].value = 0;
    label[i].undef = 1;
    hashtab[h] = i;
    return (i);
}

/*
 * Read a lexical element.
 * Return the type code.
 */
static int getlex()
{
    int c;

    if (blexflag) {
        blexflag = 0;
        return (backlex);
    }
    for (;;) {
        switch (c = getchar()) {
        case ';':
            /* Comment to end of line. */
skiptoeol:  while ((c = getchar()) != '\n')
                if (c == EOF)
                    return (LEOF);
        case '\n':
            /* New line. */
            ++line;
            c = getchar();
            if (c == ';')
                goto skiptoeol;
            ungetc (c, stdin);
            return (LEOL);
        case ' ':
        case '\t':
            /* Spaces ignored. */
            continue;
        case EOF:
            /* End of file. */
            return (LEOF);
        case ':':
            /* Syntax deimiters. */
            return (c);
        case '0':       case '1':       case '2':       case '3':
        case '4':       case '5':       case '6':       case '7':
        case '8':       case '9':
            /* Decimal constant. */
            getnum (c);
            return (LNUM);
        default:
            if (isspace (c))
                continue;
            getname (c);
            if (name[1] == 0) {
                if (name[0] == 'F')
                    return (LFUNC);
                if (name[0] == 'K')
                    return (LKKEY);
            }
            intval = lookcmd();
            if (intval >= 0)
                return (LCMD);
            return (LNAME);
        }
    }
}

static void ungetlex (val)
{
    blexflag = 1;
    backlex = val;
}

/*
 * Get register number 0..9, a..d.
 * Return the value.
 */
static unsigned getreg()
{
    int clex, i;
    static const struct {
        char *name;
        int value;
    } tab[] = {
        { "a", 10 }, { "A", 10 }, { "а", 10 }, { "А", 10 },
        { "b", 11 }, { "B", 11 }, { "б", 11 }, { "Б", 11 },
        { "c", 12 }, { "C", 12 }, { "с", 12 }, { "С", 12 },
        { "d", 13 }, { "D", 13 }, { "д", 13 }, { "Д", 13 },
        { "e", 14 }, { "E", 14 }, { "е", 14 }, { "Е", 14 },
        { 0 },
    };

    /* look a first lexeme */
    clex = getlex();
    switch (clex) {
    default:
        uerror ("operand missing");
    case LNUM:
        if (intval > 9)
            uerror ("bad register number '%u'", intval);
        return intval;
    case LNAME:
        for (i=0; tab[i].name; i++)
            if (strcmp (name, tab[i].name) == 0) {
                intval = tab[i].value;
                return intval;
            }
        uerror ("bad register number '%s'", name);
        return 0;
    }
}

static void pass1()
{
    int clex, i, opcode, type;
    int f_seen = 0, k_seen = 0, need_address = 0;
    const struct optable *op;

    while (count < sizeof(labelref)) {
        clex = getlex();
        switch (clex) {
        case LEOF:
            if (need_address)
                uerror ("jump address required");
            return;
        case LEOL:
            continue;
        case LFUNC:
            if (need_address)
                uerror ("jump address required");
            if (f_seen)
                uerror ("duplicate F key");
            f_seen = 1;
            k_seen = 0;
            continue;
        case LKKEY:
            if (need_address)
                uerror ("jump address required");
            if (k_seen)
                uerror ("duplicate K key");
            k_seen = 1;
            f_seen = 0;
            continue;
        case LNAME:
            /* Named label. */
            if (f_seen)
                uerror ("unexpected F key before label");
            if (k_seen)
                uerror ("unexpected K key before label");
            i = looklabel();
            clex = getlex();
            if (clex == ':') {
                /* Label defined. */
                i = looklabel();
                label[i].value = count;
                label[i].undef = 0;
                continue;
            }
            /* Label referenced. */
            ungetlex (clex);
            if (! need_address)
                uerror ("unknown instruction '%s'", name);
            labelref[count] = 1;
            code[count++] = i;
            break;
        case LNUM:
            /* Numeric label or address. */
            if (f_seen)
                uerror ("unexpected F key before label");
            if (k_seen)
                uerror ("unexpected K key before label");
            clex = getlex();
            if (clex == ':') {
                /* Digital label. */
                if (intval != count)
                    uerror ("incorrect label value %d, expected %d", intval, count);
            } else {
                ungetlex (clex);
                if (need_address) {
                    /* Jump address. */
                    code[count++] = (intval / 10) << 4 | (intval % 10);
                    need_address = 0;
                } else {
                    /* Enter decimal digit. */
                    if (intval > 9)
                        uerror ("unknown opcode '%d'", intval);
                    goto op;
                }
            }
            continue;
        case LCMD:
            /* Machine instruction. */
op:         if (need_address)
                uerror ("jump address required");
            op = &optable[intval];
            opcode = op->opcode;
            type = op->type;

            /* Verify F and K prefixes. */
            if (f_seen) {
                opcode = op->f_opcode;
                if (! opcode)
                    uerror ("incorrect F prefix for '%s'", op->name);
                f_seen = 0;
            } else if (k_seen) {
                opcode = op->k_opcode;
                if (! opcode)
                    uerror ("incorrect K prefix for '%s'", op->name);
                /* With K prefix, address-type ops change to register type. */
                if (type == FADDR)
                    type = FREG;
                k_seen = 0;
            } else if (! opcode && intval != 0) {
                uerror ("F or K prefix missing for '%s'", op->name);
            }

            /* Register number follows. */
            if (type & FREG) {
                opcode |= getreg();
            }

            /* Output resulting value. */
            code[count++] = opcode;

            /* Whether jump address follows. */
            need_address = (type & FADDR);
            break;
        default:
            uerror ("bad syntax");
        }
    }
}

static void pass2()
{
    int i;

    for (i=0; i<labelfree; i++) {
        /* Undefined label is fatal. */
        if (label[i].undef)
            uerror ("label '%s' undefined", label[i].name);
    }
    for (i=0; i<count; i++) {
        if (labelref[i]) {
            /* Use value of the label. */
            code[i] = label[code[i]].value;
        }
    }
}

/*
 * Parse the program source.
 */
int parse_prog (char *filename, unsigned char prog[])
{
    /* Setup input. */
    if (! freopen (filename, "r", stdin))
        uerror ("Cannot open %s", infile);
    infile = filename;

    /* Setup output. */
    code = prog;

    /* Clear local data. */
    count = 0;
    labelfree = 0;
    lastfree = 0;
    blexflag = 0;
    memset (code, 0, sizeof(labelref));
    memset (labelref, 0, sizeof(labelref));
    line = 1;

    hashinit();                         /* Initialize hash tables */
    pass1();                            /* First pass */
    pass2();                            /* Second pass */

    return count;
}

#ifdef TEST_PARSER
int main (argc, argv)
    char *argv[];
{
    int i;
    char *cp, *filename = 0;
    unsigned char prog[105];

    /*
     * Parse options.
     */
    for (i=1; i<argc; i++) {
        switch (argv[i][0]) {
        case '-':
            for (cp=argv[i]+1; *cp; cp++) {
                switch (*cp) {
                default:
                    fprintf (stderr, "Unknown option: %s\n", cp);
                    goto usage;
                }
            }
            break;
        default:
            if (filename)
                uerror ("too many input files");
            filename = argv[i];
            break;
        }
    }
    if (! filename) {
usage:  fprintf (stderr, "Usage:\n");
        fprintf (stderr, "    parse infile.pmk\n");
        exit (1);
    }

    parse_prog (filename, prog, sizeof(prog));

    //TODO: print result
    return 0;
}
#endif
