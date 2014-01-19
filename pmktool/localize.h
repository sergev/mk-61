/*
 * Localization using gettext.
 *
 * Copyright (C) 2010-2013 Serge Vakulenko
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
#if 1
    /* No localization. */
    #define _(str)                      (str)
    #define N_(str)                     str
    #define textdomain(name)            /* empty */
    #define bindtextdomain(name,dir)    /* empty */
#else
    /* Use gettext(). */
    #include <libintl.h>
    #define _(str)                      gettext (str)
    #define gettext_noop(str)           str
    #define N_(str)                     gettext_noop (str)
#endif
