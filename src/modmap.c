/*
 * Copyright (C) 2007  Dmitry Golubovsky
 * Authors: Dmitry Golubovsky <golubovsky@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, a copy can be downloaded from
 * http://www.gnu.org/licenses/gpl.html, or obtained by writing to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

/*
 * Part of this module is derived from XFREE86 sources, and has the following notice:
 */

/*
 *
 * Copyright 1988, 1998  The Open Group
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name of The Open Group shall
 * not be used in advertising or otherwise to promote the sale, use or
 * other dealings in this Software without prior written authorization
 * from The Open Group.
 */


#include <unistd.h>
#include <ctype.h>
#include "directvnc.h"
#include "keysym.h"

/*
 * This is an extension to the original directvnc enabling processing of a subset
 * of X11-style modmap file to provide better Unicode capabilities. Sources of this file
 * are based on original xmodmap sources.
 */

typedef struct {
  char keycode;
  int base;
  int shift;
  int mode;
  int modshift;
} MAPENTRY;

static MAPENTRY kbmap[256];

#define MIN_KEYCODE 8

/*
 * Perform translation of the keycode to keysym. If there is no translation
 * for the given keycode, return XK_VoidSymbol. ScrollLock is hardcoded
 * to switch between modes. CAPS does not apply to all keys: roughly
 * approximating, assume that if base is a letter then CAPS applies.
 * It additionally applies to [];',./` if Scroll is on (but this may need
 * additional configuration for non-cyrillic keyboards). This assumption
 * is based on the fact that keysyms for ASCII characters are same as characters.
 */

int modmap_translate_code(int keycode,DFBInputDeviceLockState lock, int shift) {
   int apply_caps = 0;

   if (keycode > 255)
       return XK_VoidSymbol;

   keycode += MIN_KEYCODE;

   if (kbmap[keycode].keycode != keycode)
       return XK_VoidSymbol;

   if (isalpha(kbmap[keycode].base))
       apply_caps = 1;

   if (lock & DILS_SCROLL && index ("[];',./`", kbmap[keycode].base) != NULL)
       apply_caps = 1;

   if (lock & DILS_CAPS && apply_caps)
       shift = !shift;

   return (lock & DILS_SCROLL) ? (shift ? kbmap[keycode].modshift : kbmap[keycode].mode) :
                                 (shift ? kbmap[keycode].shift : kbmap[keycode].base);
}

/*
 * Main entry: process a modmap file whose path is stored in options.
 */

static int process_file (char *);
static void process_line (int, char *);

int modmap_read_file(char *filename)
{
    int i;

    for (i = 0; i < 256; i ++) {
       bzero (&kbmap[i],sizeof(MAPENTRY));
    }

    return process_file(filename);
}

/*
 * filename: NULL means use stdin
 */
static int process_file (char *filename)
{
    FILE *fp;
    char buffer[BUFSIZ];
    int lineno;

    /* open the file, eventually we'll want to pipe through cpp */
    if (!filename) {
        return 0; /* use the hardcoded map */
    } else {
        fp = fopen (filename, "r");
        if (!fp) {
            fprintf (stderr, "unable to open modmap file '%s' for reading\n",
                     filename);
            return -1;
        }
    }

    for (lineno = 0; ; lineno++) {
        buffer[0] = '\0';
        if (fgets (buffer, BUFSIZ, fp) == NULL)
          break;

        process_line (lineno, buffer);
    }

    (void) fclose (fp);
    return 0;
}


static void process_line (lineno, buffer)
    int lineno;
    char *buffer;
{
    int len;
    int i;
    char *cp;

    char kkbuf[10];
    MAPENTRY me;

    len = strlen (buffer);

    for (i = 0; i < len; i++) {         /* look for blank lines */
        register char c = buffer[i];
        if (!(isspace(c) || c == '\n')) break;
    }
    if (i == len) return;

    cp = &buffer[i];

    if (*cp == '!') return;             /* look for comments */
    len -= (cp - buffer);               /* adjust len by how much we skipped */

                                        /* pipe through cpp */

                                        /* strip trailing space */
    for (i = len-1; i >= 0; i--) {
        register char c = cp[i];
        if (!(isspace(c) || c == '\n')) break;
    }
    if (i >= 0) cp[len = (i+1)] = '\0';  /* nul terminate */

    /* handle input */
    /*handle_line (cp, len);*/

    me.keycode = 0;
    me.base = me.shift = me.mode = me.modshift = XK_VoidSymbol;

    sscanf(cp,"%s %c %x %x %x %x", kkbuf, &me.keycode, &me.base, &me.shift, &me.mode, &me.modshift);

    kbmap[me.keycode&0xFF] = me;
}

