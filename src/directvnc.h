/* 
 * Copyright (C) 2001  Till Adam
 * Authors: Till Adam <till@adam-lilienthal.de>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */



#ifndef DIRECTVNC_H
#define DIRECTVNC_H

/* global include file */
#include <vncauth.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <X11/Xmd.h>
#include "rfbproto.h"
#include "vncauth.h"
#include <directfb.h>


/* Note that the CoRRE encoding uses this buffer and assumes it is big enough
   to hold 255 * 255 * 32 bits -> 260100 bytes.  640*480 = 307200 bytes.
   Hextile also assumes it is big enough to hold 16 * 16 * 32 bits.
   Tight encoding assumes BUFFER_SIZE is at least 16384 bytes. */

#define BUFFER_SIZE (640*480) 
char buffer[BUFFER_SIZE];

#define MAX_ENCODINGS 10

#ifdef WORDS_BIGENDIAN
#define Swap16IfLE(s) (s)
#define Swap32IfLE(l) (l)
#else
#define Swap16IfLE(s) ((((s) & 0xff) << 8) | (((s) >> 8) & 0xff))
#define Swap32IfLE(l) ((((l) & 0xff000000) >> 24) | \
		       (((l) & 0x00ff0000) >> 8)  | \
 		       (((l) & 0x0000ff00) << 8)  | \
 		       (((l) & 0x000000ff) << 24))
#endif /* WORDS_BIGENDIAN */
 
struct _mousestate
{
   int x;
   int y;
   unsigned int buttonmask;
};

struct _mousestate mousestate;

int sock;

/* rfb.c */
int rfb_connect_to_server (char *server, int display);
int rfb_initialise_connection ();
int rfb_set_format_and_encodings ();
int rfb_send_update_request(int incremental);
int rfb_handle_server_message ();
int rfb_update_mouse ();
int rfb_send_key_event(int key, int down_flag);
void rfb_get_rgb_from_data(int *r, int *g, int *b, char *data);

/* args.c */
struct serversettings
{
   char *name;
   int width;
   int height;
   int bpp;
   int depth;
   int bigendian;
   int truecolour;
   int redmax;
   int greenmax;
   int bluemax;
   int redshift;
   int greenshift;
   int blueshift;
};

struct clientsettings
{
   int width;
   int height;
   int bpp;
   int depth;
   int bigendian;
   int truecolour;
   int redmax;
   int greenmax;
   int bluemax;
   int redshift;
   int greenshift;
   int blueshift;
   int compresslevel;
   int quality;
};


struct __dfb_vnc_options 
{
   char *servername;
   int port;
   char *password;
   char *encodings;
   struct serversettings server;
   struct clientsettings client;
   int shared;
   int stretch;
   int localcursor;
   int poll_freq;
   /* not really options, but hey ;) */
   double h_ratio;
   double v_ratio;
   int h_offset;
   int v_offset;
};


typedef struct __dfb_vnc_options dfb_vnc_options;
extern dfb_vnc_options opt;
int args_parse(int argc, char **argv);

/* sockets.c */
int read_from_rfb_server(int sock, char *out, unsigned int n);
int write_exact(int sock, char *buf, unsigned int n);
int set_non_blocking(int sock);

/* dfb.c */
void dfb_init(int argc, char *argv[]);
void dfb_deinit();
void fb_handle_error(DFBResult err);
int dfb_write_data_to_screen(int x, int y, int w, int h, void *data);
int dfb_process_events(void);
int dfb_wait_for_event_with_timeout(int milliseconds);
int dfb_copy_rect(int src_x, int src_y, int dest_x, int dest_y, int w, int h);
int dfb_draw_rect_with_rgb(int x, int y, int w, int h, int r, int g, int b);
IDirectFBSurface *dfb_create_cursor_saved_area(int width, int heigth);
void dfb_save_cursor_rect( IDirectFBSurface *surf, int x, int y, int width, int heigth);
void dfb_restore_cursor_rect( IDirectFBSurface *surf, int x, int y, int width, int heigth);

/* cursor.c */
int HandleRichCursor(int x, int y, int w, int h);
void SoftCursorLockArea(int x, int y, int w, int h);
void SoftCursorUnlockScreen(void);
void SoftCursorMove(int x, int y);


/* macro for a safe call to DirectFB functions */
#define DFBCHECK(x...)                                                    \
     {                                                                    \
          err = x;                                                        \
          if (err != DFB_OK) {                                            \
               fprintf( stderr, "%s <%d>:\n\t", __FILE__, __LINE__ );     \
               DirectFBErrorFatal( #x, err );                             \
          }                                                               \
     }

#endif


