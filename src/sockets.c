/*
 *  Copyright (C) 1999 AT&T Laboratories Cambridge.  All Rights Reserved.
 *
 *  This is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This software is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this software; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 *  USA.
 */

/*
 * sockets.c - functions to deal with sockets.
 */

#include <unistd.h>
#include <sys/socket.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <memory.h>
#include <stdio.h>
#include "directvnc.h"

void PrintInHex(char *buf, int len);

int errorMessageOnReadFailure = 1;

#define BUF_SIZE 8192
static char buf[BUF_SIZE];
static char *bufoutptr = buf;
static int buffered = 0;

/*
 * ReadFromRFBServer is called whenever we want to read some data from the RFB
 * server.  It is non-trivial for two reasons:
 *
 * 1. For efficiency it performs some intelligent buffering, avoiding invoking
 *    the read() system call too often.  For small chunks of data, it simply
 *    copies the data out of an internal buffer.  For large amounts of data it
 *    reads directly into the buffer provided by the caller.
 *
 * 2. Whenever read() would block, it invokes the Xt event dispatching
 *    mechanism to process X events.  In fact, this is the only place these
 *    events are processed, as there is no XtAppMainLoop in the program.
 */


int
read_from_rfb_server(int sock, char *out, unsigned int n)
{
   if (n <= buffered)
   {
      memcpy(out, bufoutptr, n);
      bufoutptr += n;
      buffered -= n;
      return 1;
   }

   memcpy(out, bufoutptr, buffered);
   out += buffered;
   n -= buffered;

   bufoutptr = buf;
   buffered = 0;

   if (n <= BUF_SIZE)
   {

      while (buffered < n)
      {
         int i = read(sock, buf + buffered, BUF_SIZE - buffered);

         if (i <= 0)
         {
            if (i < 0)
            {
               if (errno == EWOULDBLOCK || errno == EAGAIN)
               {
		  dfb_process_events();
                  i = 0;
               }
               else
               {
                  fprintf(stderr, "DIRECTVNC");
                  perror(": read");
                  return -1;
               }
            }
            else
            {
               if (errorMessageOnReadFailure)
               {
                  fprintf(stderr, "%s: VNC server closed connection\n",
                          "DIRECTVNC");
               }
	       close(sock);
	       dfb_deinit();
	       exit (-1);
            }
         }
         buffered += i;
      }

      memcpy(out, bufoutptr, n);
      bufoutptr += n;
      buffered -= n;
      return 1;

   }
   else
   {

      while (n > 0)
      {
         int i = read(sock, out, n);

         if (i <= 0)
         {
            if (i < 0)
            {
               if (errno == EWOULDBLOCK || errno == EAGAIN)
               {
		  dfb_process_events();
                  i = 0;
               }
               else
               {
                  fprintf(stderr, "DIRECTVNC");
                  perror(": read");
                  return -1;
               }
            }
            else
            {
               if (errorMessageOnReadFailure)
               {
                  fprintf(stderr, "%s: VNC server closed connection\n",
                          "DIRECTVNC");
               }
	       close(sock);
	       dfb_deinit();
	       exit (-1);
            }
         }
         out += i;
         n -= i;
      }

      return 1;
   }
}


/*
 * Write an exact number of bytes, and don't return until you've sent them.
 */

int
write_exact(int sock, char *buf, unsigned int n)
{
   fd_set fds;
   int i = 0;
   int j;

   while (i < n)
   {
      j = write(sock, buf + i, (n - i));
      if (j <= 0)
      {
         if (j < 0)
         {
            if (errno == EWOULDBLOCK || errno == EAGAIN)
            {
               FD_ZERO(&fds);
               FD_SET(sock, &fds);

               if (select(sock + 1, NULL, &fds, NULL, NULL) <= 0)
               {
                  fprintf(stderr, "DIRECTVNC");
                  perror(": select");
                  return -1;
               }
               j = 0;
            }
            else
            {
               fprintf(stderr, "DIRECTVNC");
               perror(": write");
               return -1;
            }
         }
         else
         {
            fprintf(stderr, "%s: write failed\n", "DIRECTVNC");
            return -1;
         }
      }
      i += j;
   }
   return 1;
}


/*
 * SetNonBlocking sets a socket into non-blocking mode.
 */

int
set_non_blocking(int sock)
{
   if (fcntl(sock, F_SETFL, O_NONBLOCK) < 0)
   {
      fprintf(stderr, "DIRECTVNC Setting socket to non-blocking failed\n.");
      return -1;
   }
   return 1;
}


/*
 * Print out the contents of a packet for debugging.
 */

void
PrintInHex(char *buf, int len)
{
   int i, j;
   char c, str[17];

   str[16] = 0;

   fprintf(stderr, "ReadExact: ");

   for (i = 0; i < len; i++)
   {
      if ((i % 16 == 0) && (i != 0))
      {
         fprintf(stderr, "           ");
      }
      c = buf[i];
      str[i % 16] = (((c > 31) && (c < 127)) ? c : '.');
      fprintf(stderr, "%02x ", (unsigned char) c);
      if ((i % 4) == 3)
         fprintf(stderr, " ");
      if ((i % 16) == 15)
      {
         fprintf(stderr, "%s\n", str);
      }
   }
   if ((i % 16) != 0)
   {
      for (j = i % 16; j < 16; j++)
      {
         fprintf(stderr, "   ");
         if ((j % 4) == 3)
            fprintf(stderr, " ");
      }
      str[i % 16] = 0;
      fprintf(stderr, "%s\n", str);
   }

   fflush(stderr);
}
