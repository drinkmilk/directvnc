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

#include <unistd.h>
#include "directvnc.h"
#include <math.h>
#include <signal.h>

/* little convenience function */
static inline double get_time(void);
static inline void sig_handler(int foo) { exit(1); }

int
main (int argc,char **argv)
{
   /* parse arguments */
   args_parse(argc, argv);
   mousestate.buttonmask = 0;

  
   /* Connect to server */
   if (!rfb_connect_to_server(opt.servername, 5900 + opt.port))	
   {
      printf("Couldnt establish connection with the VNC server. Exiting\n");
      close(sock);
      exit(0);
   }

   /* initialize the connection */
   if (!rfb_initialise_connection())
   {
      printf("Connection with VNC server couldnt be initialized. Exiting\n");
      close (sock);
      exit(0);
   }

   /* Tell the VNC server which pixel format and encodings we want to use */
   if (!rfb_set_format_and_encodings())
   {
      printf("Error negotiating format and encodings. Exiting.\n");
      close(sock);
      exit(0);
   }

   /* initialize the framebuffer lib */
   dfb_init(argc, argv);

   /* hook in sighandler, so we can clean up on ctrl-c */
   signal(SIGINT, sig_handler);

   /* calculate horizontal and vertical offset */
   if (opt.client.width > opt.server.width)
      opt.h_offset = rint( (opt.client.width - opt.server.width) /2);
   if (opt.client.height > opt.server.height)
      opt.v_offset = rint( (opt.client.height - opt.server.height) /2);

   mousestate.x = opt.client.width / 2;
   mousestate.y = opt.client.height / 2;

/*  FIXME disabled for now 
 *    opt.h_ratio = (double) opt.client.width / (double) opt.server.width;
 *    opt.v_ratio = (double) opt.client.height / (double) opt.server.height;
 */
   
   /* Now enter the main loop, processing VNC messages.  mouse and keyboard
    * events will automatically be processed whenever the VNC connection is 
    * idle. */
   rfb_send_update_request(0);
   while (1) 
   {

      if (!rfb_handle_server_message())
	 break; 

      rfb_send_update_request(1);

      /* If we've just been here and there are no events pending, let the 
       * other kids play for a bit. */
      dfb_wait_for_event_with_timeout(opt.poll_freq);
   }
   dfb_deinit();
   close(sock);
   return (1);
}

static inline double
get_time(void)
{
   struct timeval v;
   gettimeofday(&v, NULL);
   
   return ((double)v.tv_sec + (((double)v.tv_usec) /1000000));
}
      
