/*
 * USB utility to control the replica of MK-54/MK-61 programmable calculator.
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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <getopt.h>
#include <locale.h>

#include "device.h"
#include "localize.h"

#define VERSION         "1."SVNVERSION

int debug_level;
char *progname;
const char *copyright;
device_t *device;

static const char symbol[] = "0123456789-LCRE ";

extern char *decompile (unsigned code, int *address_flag);

extern int parse_prog (char *filename, unsigned char code[]);

void quit (void)
{
    if (device != 0) {
        device_close (device);
        device = 0;
    }
}

void interrupted (int signum)
{
    fprintf (stderr, _("\nInterrupted.\n"));
    quit();
    _exit (-1);
}

static void format_value (char *buf, unsigned char value[6])
{
    int nibble[12];
    int i;

    // Split the value into 12 nibbles.
    for (i=0; i<6; i++) {
        nibble[i+i] = value[i] & 15;
        nibble[i+i+1] = (value[i] >> 4) & 15;
    }

    int negative = (nibble[3] == 9);
    int exponent = nibble[1] * 10 + nibble[2];
    int exp_negative = (nibble[0] == 9);
    if (exp_negative)
        exponent = - (100 - exponent);

    // Count unused digits.
    for (i=0; i<7; i++) {
        if (nibble[11-i] != 0 || exponent == 7-i)
            break;
    }
    int ndigits = 8 - i;

    // Print mantissa.
    int comma = 0;
    *buf++ = negative ? '-' : ' ';
    for (i=0; i<ndigits; i++) {
        *buf++ = symbol[nibble[4+i]];
        if ((i==0 && (exponent<0 || exponent>7)) || i == exponent) {
            *buf++ = '.';
            comma = 1;
        }
    }
    if (! comma)
        *buf++ = '.';

    if (exponent<0 || exponent>7) {
        // Add exponent
        for (i=0; i < 8-ndigits; i++)
            *buf++ = ' ';
        if (exponent < 0) {
            *buf++ = '-';
            exponent = -exponent;
        } else
            *buf++ = ' ';
        *buf++ = '0' + (exponent / 10);
        *buf++ = '0' + (exponent % 10);
    }
    *buf = 0;
}

void do_status ()
{
    static const char *name[5] = { "X1", "X", "Y", "Z", "T" };
    unsigned char stack[5][6], reg[15][6];
    char buf[16];
    int i;

    /* Open and detect the device. */
    atexit (quit);
    device = device_open (debug_level);
    if (! device) {
        fprintf (stderr, _("Error detecting device -- check cable!\n"));
        exit (1);
    }
    device_read_stack (device, stack);
    printf (_("Stack:\n"));
    for (i=1; i<5; i++) {
        format_value (buf, stack[i]);
        printf ("    %-2s = %-16s", name[i], buf);
        if (i == 1) {
            format_value (buf, stack[0]);
            printf ("  %-2s = %s", name[0], buf);
        }
        printf ("\n");
    }
    device_read_regs (device, reg);
    printf (_("Registers:\n"));
    for (i=0; i<8; i++) {
        format_value (buf, reg[i]);
        printf ("    R%x = %-16s", i, buf);
        if (i < device_data_nregs() - 8) {
            format_value (buf, reg[i+8]);
            printf ("  R%x = %s", i+8, buf);
        }
        printf ("\n");
    }
}

void do_parse (char *filename)
{
    unsigned char code[105];
    int nbytes, i, address_flag;

    /* Parse the program source. */
    nbytes = parse_prog (filename, code);

    /* Print the program. */
    printf ("\nProgram: %d bytes\n", nbytes);
    address_flag = 0;
    for (i=0; i<nbytes; i++) {
        char *mnemonics = decompile (code[i], &address_flag);
        printf ("   %3d: %s", i, mnemonics);
        printf ("\n");
    }
}

void do_program (char *filename)
{
    unsigned char code[105];
    int nbytes, last;

    /* Parse the program source. */
    nbytes = parse_prog (filename, code);

    /* Open and detect the device. */
    atexit (quit);
    device = device_open (debug_level);
    if (! device) {
        fprintf (stderr, _("Error detecting device -- check cable!\n"));
        exit (1);
    }

    /* Find the last instruction. */
    last = nbytes;
    while (--last > 0)
        if (code[last] != 0)
            break;

    printf (_("Write program: %d instructions\n"), last);
    if (nbytes > device_code_nbytes()) {
        fprintf (stderr, _("Program too large for this device!\n"));
        exit (1);
    }
    device_write_program (device, code);
}

void do_read()
{
    unsigned char code[105];
    int i, last, address_flag;

    /* Open and detect the device. */
    atexit (quit);
    device = device_open (debug_level);
    if (! device) {
        fprintf (stderr, _("Error detecting device -- check cable!\n"));
        exit (1);
    }
    device_read_program (device, code);

    /* Find the last instruction. */
    last = device_code_nbytes();
    while (--last > 0)
        if (code[last] != 0)
            break;

    printf ("Program:\n");
    address_flag = 0;
    for (i=0; i<=last; i++) {
        char *mnemonics = decompile (code[i], &address_flag);
        printf ("   %3d: %s", i, mnemonics);
#if 0
        // Print opcode.
        char c1 = symbol[code[i] >> 4];
        char c2 = symbol[code[i] & 15];
        printf (" (%c%c)", c1, c2);
#endif
        printf ("\n");
    }
}

/*
 * Print copying part of license
 */
static void show_copying (void)
{
    printf ("%s.\n\n", copyright);
    printf ("Permission to use, copy, modify, and distribute this software\n");
    printf ("and its documentation for any purpose and without fee is hereby\n");
    printf ("granted, provided that the above copyright notice appear in all\n");
    printf ("copies and that both that the copyright notice and this\n");
    printf ("permission notice and warranty disclaimer appear in supporting\n");
    printf ("documentation, and that the name of the author not be used in\n");
    printf ("advertising or publicity pertaining to distribution of the\n");
    printf ("software without specific, written prior permission.\n");
    printf ("\n");
    printf ("The author disclaim all warranties with regard to this\n");
    printf ("software, including all implied warranties of merchantability\n");
    printf ("and fitness.  In no event shall the author be liable for any\n");
    printf ("special, indirect or consequential damages or any damages\n");
    printf ("whatsoever resulting from loss of use, data or profits, whether\n");
    printf ("in an action of contract, negligence or other tortious action,\n");
    printf ("arising out of or in connection with the use or performance of\n");
    printf ("this software.\n");
    printf ("\n");
}

/*
 * Print NO WARRANTY part of license
 */
static void show_warranty (void)
{
    printf ("%s.\n\n", copyright);
    printf ("BECAUSE THE PROGRAM IS LICENSED FREE OF CHARGE, THERE IS NO WARRANTY\n");
    printf ("FOR THE PROGRAM, TO THE EXTENT PERMITTED BY APPLICABLE LAW.  EXCEPT WHEN\n");
    printf ("OTHERWISE STATED IN WRITING THE COPYRIGHT HOLDERS AND/OR OTHER PARTIES\n");
    printf ("PROVIDE THE PROGRAM \"AS IS\" WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED\n");
    printf ("OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF\n");
    printf ("MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  THE ENTIRE RISK AS\n");
    printf ("TO THE QUALITY AND PERFORMANCE OF THE PROGRAM IS WITH YOU.  SHOULD THE\n");
    printf ("PROGRAM PROVE DEFECTIVE, YOU ASSUME THE COST OF ALL NECESSARY SERVICING,\n");
    printf ("REPAIR OR CORRECTION.\n");
    printf("\n");
    printf ("IN NO EVENT UNLESS REQUIRED BY APPLICABLE LAW OR AGREED TO IN WRITING\n");
    printf ("WILL ANY COPYRIGHT HOLDER, OR ANY OTHER PARTY WHO MAY MODIFY AND/OR\n");
    printf ("REDISTRIBUTE THE PROGRAM AS PERMITTED ABOVE, BE LIABLE TO YOU FOR DAMAGES,\n");
    printf ("INCLUDING ANY GENERAL, SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES ARISING\n");
    printf ("OUT OF THE USE OR INABILITY TO USE THE PROGRAM (INCLUDING BUT NOT LIMITED\n");
    printf ("TO LOSS OF DATA OR DATA BEING RENDERED INACCURATE OR LOSSES SUSTAINED BY\n");
    printf ("YOU OR THIRD PARTIES OR A FAILURE OF THE PROGRAM TO OPERATE WITH ANY OTHER\n");
    printf ("PROGRAMS), EVEN IF SUCH HOLDER OR OTHER PARTY HAS BEEN ADVISED OF THE\n");
    printf ("POSSIBILITY OF SUCH DAMAGES.\n");
    printf("\n");
}

int main (int argc, char **argv)
{
    int ch, read_mode = 0, parse_mode = 0;
    static const struct option long_options[] = {
        { "help",        0, 0, 'h' },
        { "warranty",    0, 0, 'W' },
        { "copying",     0, 0, 'C' },
        { "version",     0, 0, 'V' },
        { "skip-verify", 0, 0, 'S' },
        { NULL,          0, 0, 0 },
    };

    /* Set locale and message catalogs. */
    setlocale (LC_ALL, "");
#if defined (__CYGWIN32__) || defined (MINGW32)
    /* Files with localized messages should be placed in
     * the current directory or in c:/Program Files/pmktool. */
    if (access ("./ru/LC_MESSAGES/pmktool.mo", R_OK) == 0)
        bindtextdomain ("pmktool", ".");
    else
        bindtextdomain ("pmktool", "c:/Program Files/pmktool");
#else
    bindtextdomain ("pmktool", "/usr/local/share/locale");
#endif
    textdomain ("pmktool");

    setvbuf (stdout, (char *)NULL, _IOLBF, 0);
    setvbuf (stderr, (char *)NULL, _IOLBF, 0);
    printf (_("Utility for MK-54/MK-61 calculator, Version %s\n"), VERSION);
    progname = argv[0];
    copyright = _("Copyright: (C) 2014 Serge Vakulenko");
    signal (SIGINT, interrupted);
#ifdef __linux__
    signal (SIGHUP, interrupted);
#endif
    signal (SIGTERM, interrupted);

    while ((ch = getopt_long (argc, argv, "vDhrpCVW",
      long_options, 0)) != -1) {
        switch (ch) {
        case 'D':
            ++debug_level;
            continue;
        case 'r':
            ++read_mode;
            continue;
        case 'p':
            ++parse_mode;
            continue;
        case 'h':
            break;
        case 'V':
            /* Version already printed above. */
            return 0;
        case 'C':
            show_copying ();
            return 0;
        case 'W':
            show_warranty ();
            return 0;
        }
usage:
        printf ("%s.\n\n", copyright);
        printf ("PMKtool comes with ABSOLUTELY NO WARRANTY; for details\n");
        printf ("use `--warranty' option. This is Open Source software. You are\n");
        printf ("welcome to redistribute it under certain conditions. Use the\n");
        printf ("'--copying' option for details.\n\n");
        printf ("Show stack and registers:\n");
        printf ("       pmktool\n");
        printf ("\nWrite program:\n");
        printf ("       pmktool [-p] file.txt\n");
        printf ("\nRead program:\n");
        printf ("       pmktool -r > file.txt\n");
        printf ("\nArgs:\n");
        printf ("       file.txt            Program file in text format\n");
        printf ("       -r                  Read mode\n");
        printf ("       -p                  Parse only, don't write to device\n");
        printf ("       -D                  Debug mode\n");
        printf ("       -h, --help          Print this help message\n");
        printf ("       -V, --version       Print version\n");
        printf ("       -C, --copying       Print copying information\n");
        printf ("       -W, --warranty      Print warranty information\n");
        printf ("\n");
        return 0;
    }
    printf ("%s\n", copyright);
    argc -= optind;
    argv += optind;

    switch (argc) {
    case 0:
        if (read_mode)
            do_read();
        else
            do_status();
        break;
    case 1:
        if (parse_mode)
            do_parse (argv[0]);
        else
            do_program (argv[0]);
        break;
    default:
        goto usage;
    }
    quit ();
    return 0;
}
