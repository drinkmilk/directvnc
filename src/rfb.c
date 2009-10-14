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
#include <sys/socket.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <math.h>
#include <zlib.h>

#include "directvnc.h"
#include "tight.h"

int _rfb_negotiate_protocol ();
int _rfb_authenticate ();
int _rfb_initialise_client ();
int _rfb_initialise_server ();


static int _handle_raw_encoded_message(rfbFramebufferUpdateRectHeader rectheader);
static int _handle_copyrect_encoded_message(rfbFramebufferUpdateRectHeader rectheader);
static int _handle_rre_encoded_message(rfbFramebufferUpdateRectHeader rectheader);
static int _handle_corre_encoded_message(rfbFramebufferUpdateRectHeader rectheader);
static int _handle_hextile_encoded_message(rfbFramebufferUpdateRectHeader rectheader);
static int _handle_richcursor_message(rfbFramebufferUpdateRectHeader rectheader);

/*
 * ConnectToRFBServer.
 */


int 
rfb_connect_to_server (char *host, int port)
{
   struct hostent *he=NULL;
   int one=1;
   struct sockaddr_in s;


   if ( (sock = socket (AF_INET, SOCK_STREAM, 0)) <0) 
   {
      fprintf(stderr, "Error creating communication socket: %d\n", errno);
      exit(2);
   }
 
   /* if the server wasnt specified as an ip address, look it up */
   if (!inet_aton(host, &s.sin_addr)) 
   {
      if ( (he = gethostbyname(host)) )
	 memcpy (&s.sin_addr.s_addr, he->h_addr, he->h_length);
      else
      {
	 fprintf(stderr, "Couldnt resolve host!\n");
         close(sock);
	 return (0);
      }
   }
  
   s.sin_port = htons(port);
   s.sin_family = AF_INET;

   if (connect(sock,(struct sockaddr*) &s, sizeof(s)) < 0)
   {
      fprintf(stderr, "Connect error\n");
      close(sock);
      return (0);
   }
   if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char *)&one, sizeof(one)) < 0)
   {
      fprintf(stderr, "Error setting socket options\n");
      close(sock);
      return (0);
   }

   if (!set_non_blocking(sock)) return -1;
  
   return (sock);
}

int
rfb_initialise_connection ()   
{
   if (!_rfb_negotiate_protocol()) return 0;
   if (!_rfb_authenticate()) return 0;
   if (!_rfb_initialise_client()) return 0;
   if (!_rfb_initialise_server()) return 0;
   
   return(1);
}

int
_rfb_negotiate_protocol()
{
   rfbProtocolVersionMsg msg;
 
   /* read the protocol version the server uses */
   if (!read_from_rfb_server(sock, (char*)&msg, sz_rfbProtocolVersionMsg))
      return 0;
   /* FIXME actually do something with that information ;) */

   /* send the protocol version we want to use */
   sprintf(msg, rfbProtocolVersionFormat, 
	        rfbProtocolMajorVersion, 
		rfbProtocolMinorVersion);
   if (!write_exact (sock, msg, sz_rfbProtocolVersionMsg))
      return 0;

   return 1;
}


int
_rfb_authenticate()
{
   CARD32 authscheme;
  
   read_from_rfb_server(sock, (char *)&authscheme, 4);
   authscheme = Swap32IfLE(authscheme);
   switch (authscheme)
   {
      CARD32 reason_length;
      CARD8 *reason_string;
      CARD8 challenge_and_response[CHALLENGESIZE];
      CARD32 auth_result;
      
      case rfbConnFailed:	 	
         fprintf(stderr, "DIRECTVNC: Connection to VNC server failed\n");
	 read_from_rfb_server(sock, (char *)&reason_length, 4);
	 reason_length = Swap32IfLE(reason_length);
	 reason_string = malloc(sizeof(CARD8) * reason_length);
	 read_from_rfb_server(sock, (char *)reason_string, reason_length);
	 fprintf(stderr, "Errormessage: %s\n", reason_string); 
	 return (0);
      case rfbVncAuth:

	 /* we didnt get a password on the command line, so go get one */
         if (!opt.password) opt.password = getpass("Password: ");

	 if (!read_from_rfb_server(sock, challenge_and_response, CHALLENGESIZE))
	    return 0;
	 vncEncryptBytes(challenge_and_response, opt.password);
	 if (!write_exact(sock, challenge_and_response, CHALLENGESIZE))
	    return 0;
	 if (!read_from_rfb_server(sock, (char*)&auth_result, 4))
	    return 0;
	 auth_result = Swap32IfLE(auth_result);
	 switch (auth_result)
	 {
	    case rfbVncAuthFailed:
	       fprintf(stderr, "Authentication Failed\n");
	       return (0);
	    case rfbVncAuthTooMany:
	       fprintf(stderr, "Too many connections\n");
	       return (0);
	    case rfbVncAuthOK:
	       fprintf(stderr, "Authentication OK\n");
	       break;
	 }
	 break;
      case rfbNoAuth:
	 break;
   } 
   return 1;
}

int
_rfb_initialise_client()
{
   rfbClientInitMsg cl;
   cl.shared = opt.shared;
   if (!write_exact (sock, (char *) &cl, sz_rfbClientInitMsg))
      return 0;

   return 1;
}

int
_rfb_initialise_server()
{
   int len;
   rfbServerInitMsg si;

   if (!read_from_rfb_server(sock, (char *)&si, sz_rfbServerInitMsg))
      return 0;
   opt.server.width = Swap16IfLE(si.framebufferWidth);
   opt.server.height = Swap16IfLE(si.framebufferHeight);
   opt.server.bpp = si.format.bitsPerPixel;
   opt.server.depth = si.format.bigEndian;
   opt.server.truecolour = si.format.trueColour;
   
   opt.server.redmax = Swap16IfLE(si.format.redMax);
   opt.server.greenmax = Swap16IfLE(si.format.greenMax);
   opt.server.bluemax = Swap16IfLE(si.format.blueMax);
   opt.server.redshift = si.format.redShift;
   opt.server.greenshift = si.format.greenShift;
   opt.server.blueshift = si.format.blueShift;

   len = Swap32IfLE(si.nameLength);
   opt.server.name = malloc(sizeof(char) * len+1);

   if (!read_from_rfb_server(sock, opt.server.name, len))
      return 0;

   return 1;
}

int
rfb_set_format_and_encodings()
{
   char *next;
   int num_enc =0;
   rfbSetPixelFormatMsg pf;
   rfbSetEncodingsMsg em;
   CARD32 enc[MAX_ENCODINGS];
  
   pf.type = 0;
   pf.format.bitsPerPixel = opt.client.bpp;
   pf.format.depth = opt.client.depth;
   pf.format.bigEndian = opt.client.bigendian;
   pf.format.trueColour = opt.client.truecolour;
   pf.format.redMax = Swap16IfLE(opt.client.redmax);
   pf.format.greenMax = Swap16IfLE(opt.client.greenmax);
   pf.format.blueMax = Swap16IfLE(opt.client.bluemax);
   pf.format.redShift =opt.client.redshift;
   pf.format.greenShift = opt.client.greenshift;
   pf.format.blueShift = opt.client.blueshift;

   if (!write_exact(sock, (char*)&pf, sz_rfbSetPixelFormatMsg)) return 0;

   em.type = rfbSetEncodings;
   /* figure out the encodings string given on the command line */
   next = strtok(opt.encodings, " ");
   while (next && em.nEncodings < MAX_ENCODINGS)
   {
      if (!strcmp(next, "raw"))
      {
	 enc[num_enc++] = Swap32IfLE(rfbEncodingRaw);
      }
      if (!strcmp(next, "tight"))
      {
	 enc[num_enc++] = Swap32IfLE(rfbEncodingTight);
      }
      if (!strcmp(next, "hextile"))
      {
	 enc[num_enc++] = Swap32IfLE(rfbEncodingHextile);
      }
      if (!strcmp(next, "zlib"))
      {
	 enc[num_enc++] = Swap32IfLE(rfbEncodingZlib);
      }
      else if (!strcmp(next, "copyrect"))
      {
	 enc[num_enc++] = Swap32IfLE(rfbEncodingCopyRect);
      }
      else if (!strcmp(next, "corre"))
      {
	 enc[num_enc++] = Swap32IfLE(rfbEncodingCoRRE);
      }
      else if (!strcmp(next, "rre"))
      {
	 enc[num_enc++] = Swap32IfLE(rfbEncodingRRE);
      }
      next = strtok(NULL, " ");
      em.nEncodings = Swap16IfLE(num_enc);
   }
   if (!em.nEncodings)
   {
      enc[num_enc++] = Swap32IfLE(rfbEncodingTight);
      enc[num_enc++] = Swap32IfLE(rfbEncodingHextile);
      enc[num_enc++] = Swap32IfLE(rfbEncodingZlib);
      enc[num_enc++] = Swap32IfLE(rfbEncodingCopyRect);
      enc[num_enc++] = Swap32IfLE(rfbEncodingRRE);
      enc[num_enc++] = Swap32IfLE(rfbEncodingCoRRE); 
      enc[num_enc++] = Swap32IfLE(rfbEncodingRaw);
   }

   /* Track cursor locally */
   if (opt.localcursor)
      enc[num_enc++] = Swap32IfLE(rfbEncodingRichCursor);
     
   if (opt.client.compresslevel <= 9)
      enc[num_enc++] = Swap32IfLE(rfbEncodingCompressLevel0 + 
	                          opt.client.compresslevel);
   if (opt.client.quality <= 9)
      enc[num_enc++] = Swap32IfLE(rfbEncodingQualityLevel0 + 
	                          opt.client.quality);

   em.nEncodings = Swap16IfLE(num_enc);
   
   if (!write_exact(sock, (char*)&em, sz_rfbSetEncodingsMsg)) return 0;
   if (!write_exact(sock, (char*)&enc, num_enc * 4)) return 0;

   return(1);
}


int
rfb_send_update_request(int incremental)
{
   rfbFramebufferUpdateRequestMsg urq;

   urq.type = rfbFramebufferUpdateRequest;
   urq.incremental = incremental;
   urq.x = 0;
   urq.y = 0;
   urq.w = opt.server.width;
   urq.h = opt.server.height;

   urq.x = Swap16IfLE(urq.x);
   urq.y = Swap16IfLE(urq.y);
   urq.w = Swap16IfLE(urq.w);
   urq.h = Swap16IfLE(urq.h);

   if (!write_exact(sock, (char*)&urq, sz_rfbFramebufferUpdateRequestMsg))
      return 0;

   return 1;
}


int
rfb_handle_server_message()
{
   int size;
   char *buf;
   rfbServerToClientMsg msg;
   rfbFramebufferUpdateRectHeader rectheader;
   
   if (!read_from_rfb_server(sock, (char*)&msg, 1)) return 0;
   switch (msg.type)
   {
      int i;
      case rfbFramebufferUpdate:
	 read_from_rfb_server(sock, ((char*)&msg.fu)+1, sz_rfbFramebufferUpdateMsg-1);
	 msg.fu.nRects = Swap16IfLE(msg.fu.nRects);
	 for (i=0;i< msg.fu.nRects;i++)
	 {
	    read_from_rfb_server(sock, (char*)&rectheader, 
		  sz_rfbFramebufferUpdateRectHeader);
	    rectheader.r.x = Swap16IfLE(rectheader.r.x);
	    rectheader.r.y = Swap16IfLE(rectheader.r.y);
	    rectheader.r.w = Swap16IfLE(rectheader.r.w);
	    rectheader.r.h = Swap16IfLE(rectheader.r.h);
	    rectheader.encoding = Swap32IfLE(rectheader.encoding);
	    SoftCursorLockArea(rectheader.r.x, rectheader.r.y, rectheader.r.w, rectheader.r.h); 
	    switch (rectheader.encoding)
	    {
	       case rfbEncodingRaw:
		  _handle_raw_encoded_message(rectheader);		  
		  break;
	       case rfbEncodingCopyRect:
		  _handle_copyrect_encoded_message(rectheader); 
		  break;
	       case rfbEncodingRRE:
		  _handle_rre_encoded_message(rectheader);
		  break;
	       case rfbEncodingCoRRE:
		  _handle_corre_encoded_message(rectheader);
		  break;
	       case rfbEncodingHextile:
		  _handle_hextile_encoded_message(rectheader);
		  break;
	       case rfbEncodingTight:
		  _handle_tight_encoded_message(rectheader);
		  break;
	       case rfbEncodingZlib:
		  _handle_zlib_encoded_message(rectheader);
		  break;
	       case rfbEncodingRichCursor:
		  _handle_richcursor_message(rectheader);
		  break;
	       case rfbEncodingLastRect:
		  printf("LAST\n");
		  break;
		  
	       default:
		  printf("Unknown encoding\n");
		  return 0;
		  break;
	    }
	    /* Now we may discard "soft cursor locks". */
	    SoftCursorUnlockScreen();

	 }
	 break;
      case rfbSetColourMapEntries:
	 fprintf(stderr, "SetColourMapEntries\n");
	 read_from_rfb_server(sock, ((char*)&msg.scme)+1, sz_rfbSetColourMapEntriesMsg-1);
	 break;
      case rfbBell:
	 fprintf(stderr, "Bell message. Unimplemented.\n");
	 break;
      case rfbServerCutText:
	 fprintf(stderr, "ServerCutText. Unimplemented.\n");
	 read_from_rfb_server(sock, ((char*)&msg.sct)+1, 
	       sz_rfbServerCutTextMsg-1);
	 size = Swap32IfLE(msg.sct.length);
	 buf = malloc(sizeof(char) * size);
	 read_from_rfb_server(sock, buf, size);
	 buf[size]=0;
	 printf("%s\n", buf);
	 free(buf);
	 break;
      default:
	 printf("Unknown server message. Type: %i\n", msg.type);
	 return 0;
	 break;
   }
   return(1);
}

int
rfb_update_mouse()
{
   rfbPointerEventMsg msg;

   if (mousestate.x < 0) mousestate.x = 0;
   if (mousestate.y < 0) mousestate.y = 0;

   if (mousestate.x > opt.client.width) mousestate.x = opt.client.width;
   if (mousestate.y > opt.client.height) mousestate.y = opt.client.height;


   msg.type = rfbPointerEvent;
   msg.buttonMask = mousestate.buttonmask;
   
   /* scale to server resolution */
   msg.x = rint(mousestate.x * opt.h_ratio);
   msg.y = rint(mousestate.y * opt.v_ratio);
   
   SoftCursorMove(msg.x, msg.y);
   
   msg.x = Swap16IfLE(msg.x);
   msg.y = Swap16IfLE(msg.y);

   return(write_exact(sock, (char*)&msg, sz_rfbPointerEventMsg));
}

int
rfb_send_key_event(int key, int down_flag)
{
   rfbKeyEventMsg ke;

   ke.type = rfbKeyEvent;
   ke.down = down_flag;
   ke.key = Swap32IfLE(key);

   return (write_exact(sock, (char*)&ke, sz_rfbKeyEventMsg));
}

static int
_handle_raw_encoded_message(rfbFramebufferUpdateRectHeader rectheader)
{
   int size;
   char *buf;
   size = (opt.client.bpp/8 * rectheader.r.w) * rectheader.r.h;
   buf = malloc(size);
   if (!read_from_rfb_server(sock, buf, size)) return 0;
   dfb_write_data_to_screen(
	 rectheader.r.x, 
	 rectheader.r.y, 
	 rectheader.r.w, 
	 rectheader.r.h, 
	 buf
	 );
   free (buf);
   return 1;
}

static int
_handle_copyrect_encoded_message(rfbFramebufferUpdateRectHeader rectheader)
{
   int src_x, src_y;
   
   if (!read_from_rfb_server(sock, (char*)&src_x, 2)) return 0;
   if (!read_from_rfb_server(sock, (char*)&src_y, 2)) return 0;

    /* If RichCursor encoding is used, we should extend our
           "cursor lock area" (previously set to destination
           rectangle) to the source rectangle as well. */
   SoftCursorLockArea(src_x, src_y, rectheader.r.w, rectheader.r.h);
   dfb_copy_rect(
	 Swap16IfLE(src_x),
	 Swap16IfLE(src_y),
	 rectheader.r.x,
	 rectheader.r.y,
	 rectheader.r.w, 
	 rectheader.r.h
	 );
     return 1;
}

static int
_handle_rre_encoded_message(rfbFramebufferUpdateRectHeader rectheader)
{
   rfbRREHeader header;
   char *colour;
   CARD16 rect[4];
   int i;
   int r=0, g=0, b=0;

   colour = malloc(sizeof(opt.client.bpp/8));
   if (!read_from_rfb_server(sock, (char *)&header, sz_rfbRREHeader)) return 0;
   header.nSubrects = Swap32IfLE(header.nSubrects);
   
   /* draw background rect */
   if (!read_from_rfb_server(sock, colour, opt.client.bpp/8)) return 0;
   rfb_get_rgb_from_data(&r, &g, &b, colour);
   dfb_draw_rect_with_rgb(
	         rectheader.r.x,
	         rectheader.r.y,
	         rectheader.r.w,
	         rectheader.r.h,
		 r,g,b
		 );
   
   /* subrect pixel values */
   for (i=0;i<header.nSubrects;i++)
   {
      if (!read_from_rfb_server(sock, colour, opt.client.bpp/8)) 
	 return 0;
      rfb_get_rgb_from_data(&r, &g, &b, colour);
      if (!read_from_rfb_server(sock, (char *)&rect, sizeof(rect))) 
	 return 0;
      dfb_draw_rect_with_rgb(
	    Swap16IfLE(rect[0]) + rectheader.r.x,
	    Swap16IfLE(rect[1]) + rectheader.r.y,
	    Swap16IfLE(rect[2]),
	    Swap16IfLE(rect[3]),
	    r,g,b
	    );
   }   
   free (colour);
   return 1;
}
   
static int
_handle_corre_encoded_message(rfbFramebufferUpdateRectHeader rectheader)
{
   rfbRREHeader header;
   char *colour;
   CARD8 rect[4];
   int i;
   int r=0, g=0, b=0;

   colour = malloc(sizeof(opt.client.bpp/8));
   if (!read_from_rfb_server(sock, (char *)&header, sz_rfbRREHeader)) return 0;
   header.nSubrects = Swap32IfLE(header.nSubrects);
   
   /* draw background rect */
   if (!read_from_rfb_server(sock, colour, opt.client.bpp/8)) return 0;
   rfb_get_rgb_from_data(&r, &g, &b, colour);
   dfb_draw_rect_with_rgb( 
	 rectheader.r.x, rectheader.r.y, rectheader.r.w, rectheader.r.h, r,g,b);
   
   /* subrect pixel values */
   for (i=0;i<header.nSubrects;i++)
   {
      if (!read_from_rfb_server(sock, colour, opt.client.bpp/8)) 
	 return 0;
      rfb_get_rgb_from_data(&r, &g, &b, colour);
      if (!read_from_rfb_server(sock, (char *)&rect, sizeof(rect))) 
	 return 0;
      dfb_draw_rect_with_rgb( rect[0] + rectheader.r.x, 
	                      rect[1] + rectheader.r.y, 
			      rect[2], rect[3], r,g,b);
   }   
   free (colour);
   return 1;
}

static int
_handle_hextile_encoded_message(rfbFramebufferUpdateRectHeader rectheader)
{
   int rect_x, rect_y, rect_w,rect_h, i=0,j=0, n;
   int tile_w = 16, tile_h = 16;
   int remaining_w, remaining_h;
   CARD8 subrect_encoding;
   int bpp = opt.client.bpp / 8;
   int nr_subr = 0;		  
   int x,y,w,h;
   int r=0, g=0, b=0;
   int fg_r=0, fg_g=0, fg_b=0;
   int bg_r=0, bg_g=0, bg_b=0;
   rect_w = remaining_w = rectheader.r.w;
   rect_h = remaining_h = rectheader.r.h;
   rect_x = rectheader.r.x;
   rect_y = rectheader.r.y;

   /* the rect is divided into tiles of width and height 16. Iterate over
    * those */
   while ((double)i < (double) rect_h/16)
   {
      /* the last tile in a column could be smaller than 16 */
      if ( (remaining_h -=16) <= 0) tile_h = remaining_h +16;
	
      j=0;
      while ((double)j < (double) rect_w/16)
      {
	 /* the last tile in a row could also be smaller */
	 if ( (remaining_w -= 16) <= 0 ) tile_w = remaining_w +16;
	 
	 if (!read_from_rfb_server(sock, (char*)&subrect_encoding, 1)) return 0;
	 /* first, check if the raw bit is set */
	 if (subrect_encoding & rfbHextileRaw)
	 {
	    if (!read_from_rfb_server(sock, buffer, bpp*tile_w*tile_h)) return 0;
	    dfb_write_data_to_screen( 
		  rect_x+(j*16), rect_y+(i*16), tile_w, tile_h, buffer);
	 } 
	 else  /* subrect encoding is not raw */
	 {
	    /* check whether theres a new bg or fg colour specified */
	    if (subrect_encoding & rfbHextileBackgroundSpecified)
	    {
	       if (!read_from_rfb_server(sock, buffer, bpp)) return 0;
	       rfb_get_rgb_from_data(&bg_r, &bg_g, &bg_b, buffer);
	    }
	    if (subrect_encoding & rfbHextileForegroundSpecified)
	    {
	       if (!read_from_rfb_server(sock, buffer, bpp)) return 0;
	       rfb_get_rgb_from_data(&fg_r, &fg_g, &fg_b, buffer);
	    }
	    /* fill the background */
	    dfb_draw_rect_with_rgb(
		    rect_x+(j*16), rect_y+(i*16), tile_w, tile_h, bg_r, bg_g, bg_b);

	    if (subrect_encoding & rfbHextileAnySubrects)
	    {
	       if (!read_from_rfb_server(sock, (char*)&nr_subr, 1)) return 0;
	       for (n=0;n<nr_subr;n++)
	       {
		  if (subrect_encoding & rfbHextileSubrectsColoured)
		  {
		     read_from_rfb_server(sock, buffer, bpp);
		     rfb_get_rgb_from_data(&r, &g, &b, buffer);
		     
		     read_from_rfb_server(sock, buffer, 2);
		     x = rfbHextileExtractX( (CARD8) *buffer);
		     y = rfbHextileExtractY( (CARD8) *buffer);
		     w = rfbHextileExtractW( (CARD8)*(buffer+1));
		     h = rfbHextileExtractH( (CARD8)*(buffer+1));
		     dfb_draw_rect_with_rgb(
			   x+(rect_x+(j*16)), y+(rect_y+(i*16)), w, h, r,g,b);
		  }
		  else
		  {
		     read_from_rfb_server(sock, buffer, 2);
		     x = rfbHextileExtractX( (CARD8) *buffer);
		     y = rfbHextileExtractY( (CARD8) *buffer);
		     w = rfbHextileExtractW( (CARD8)* (buffer+1));
		     h = rfbHextileExtractH( (CARD8)* (buffer+1));
		     dfb_draw_rect_with_rgb(
			   x+(rect_x+(j*16)), y+(rect_y+(i*16)), w, h, fg_r,fg_g,fg_b);
		  }
	       }
	    }
	 }
	 j++;
      }
      remaining_w = rectheader.r.w;
      tile_w = 16; /* reset for next row */
      i++;
   }	
   return 1;
}

static int
_handle_richcursor_message(rfbFramebufferUpdateRectHeader rectheader)
{
  return HandleRichCursor(rectheader.r.x, rectheader.r.y, rectheader.r.w, rectheader.r.h); 
}

void
rfb_get_rgb_from_data(int *r, int *g, int *b, char *data)
{
   CARD16 foo16;

   switch (opt.client.bpp)
   {
      case 8:
	 printf("FIXME unimplemented\n");	 
	 break;
      case 16:
	 memcpy(&foo16, data, 2);
	 *r = (( foo16 >> opt.client.redshift ) & opt.client.redmax) <<3;
 	 *g = (( foo16 >> opt.client.greenshift ) & opt.client.greenmax)<<2;
 	 *b = (( foo16 >> opt.client.blueshift ) & opt.client.bluemax)<<3;
	 break;
      case 24:
	 printf("FIXME unimplemented\n");
	 break;
      case 32:
	 printf("FIXME unimplemented\n");
	 break;
   }
}
