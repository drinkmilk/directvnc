/* 
 * Copyright (C) 2001  Till Adam
 * Authors: Till Adam <till@adam-lilienthal.de>
 *
 * original implementation by Constantin Kaplinsky <const@ce.cctpu.edu.ru>
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
#include "jpeg.h"
#include "tight.h"

/*
 * Variables for the ``tight'' encoding implementation.
 */
#define RGB_TO_PIXEL(bpp,r,g,b)						\
  (((CARD16)(r) & opt.client.redmax) << opt.client.redshift |	\
   ((CARD16)(g) & opt.client.greenmax) << opt.client.greenshift |	\
   ((CARD16)(b) & opt.client.bluemax) << opt.client.blueshift)

#define TIGHT_MIN_TO_COMPRESS 12
/* Separate buffer for compressed data. */
#define ZLIB_BUFFER_SIZE 512
static char zlib_buffer[ZLIB_BUFFER_SIZE];

/* Four independent compression streams for zlib library. */
static z_stream zlibStream[4];
static int zlibStreamActive[4] = {
  0, 0, 0, 0
};

/* Filter stuff. Should be initialized by filter initialization code. */
static int cutZeros;
static int rectWidth, rectColors;
static char tightPalette[256*4];
static CARD8 tightPrevRow[2048*3*sizeof(CARD16)];

/* zlib stuff */
static int raw_buffer_size = -1;
static char *raw_buffer;
static z_stream decompStream;
static int decompStreamInited = 0;



int
_handle_tight_encoded_message(rfbFramebufferUpdateRectHeader rectheader)
{
   CARD8 comp_ctl;
   CARD8 filter_id;
   CARD16 fill_colour;
   int r=0, g=0, b=0;
   filterPtr filterFn;
   int err, stream_id, compressedLen, bitsPixel;
   int bufferSize, rowSize, numRows, portionLen, rowsProcessed, extraBytes;
   void *dst;
   z_streamp zs;

   /* read the compression type */
   if (!read_from_rfb_server(sock, (char*)&comp_ctl, 1)) return 0;

   /* The lower 4 bits are apparently used as active flags for the zlib
    * streams. Iterate over them and right shift 1, so the encoding ends up in
    * the first 4 bits. */
   for (stream_id = 0; stream_id < 4; stream_id++) {
    if ((comp_ctl & 1) && zlibStreamActive[stream_id]) {
      if (inflateEnd (&zlibStream[stream_id]) != Z_OK &&
          zlibStream[stream_id].msg != NULL)
        fprintf(stderr, "inflateEnd: %s\n", zlibStream[stream_id].msg);
      zlibStreamActive[stream_id] = 0;
    }
    comp_ctl >>= 1;
  }

  /* Handle solid rectangles. */
   if (comp_ctl == rfbTightFill) {

      if (!read_from_rfb_server(sock, (char*)&fill_colour, sizeof(fill_colour)))
	 return 0;

      rfb_get_rgb_from_data(&r, &g, &b, (char*)&fill_colour);
      dfb_draw_rect_with_rgb(
	    rectheader.r.x,
	    rectheader.r.y,
	    rectheader.r.w,
	    rectheader.r.h,
	    r,g,b
	    );
      return 1;
   }

   /* Handle jpeg compressed rectangle */
  if (comp_ctl == rfbTightJpeg) {
    return DecompressJpegRect(
	  rectheader.r.x, 
	  rectheader.r.y, 
	  rectheader.r.w, 
	  rectheader.r.h);
  }

  /* Quit on unsupported subencoding value. */
  if (comp_ctl > rfbTightMaxSubencoding) {
    fprintf(stderr, "Tight encoding: bad subencoding value received.\n");
    return 0;
  }

  /*
   * Here primary compression mode handling begins.
   * Data was processed with optional filter + zlib compression.
   */

  /* First, we should identify a filter to use. */
  if ((comp_ctl & rfbTightExplicitFilter) != 0) {
    if (!read_from_rfb_server(sock, (char*)&filter_id, 1))
      return 0;

    switch (filter_id) {
    case rfbTightFilterCopy:
      filterFn = FilterCopy;
      bitsPixel = InitFilterCopy(rectheader.r.w, rectheader.r.h);
      break;
    case rfbTightFilterPalette:
      filterFn = FilterPalette;
      bitsPixel = InitFilterPalette(rectheader.r.w, rectheader.r.h);
      break;
    case rfbTightFilterGradient:
      filterFn = FilterGradient;
      bitsPixel = InitFilterGradient(rectheader.r.w, rectheader.r.h);
      break;
    default:
      fprintf(stderr, "Tight encoding: unknown filter code received.\n");
      return 0;
    }
  } else {
    filterFn = FilterCopy;
    bitsPixel = InitFilterCopy(rectheader.r.w, rectheader.r.h);
  }
  if (bitsPixel == 0) {
    fprintf(stderr, "Tight encoding: error receiving palette.\n");
    return 0;
  }

   /* Determine if the data should be decompressed or just copied. */
  rowSize = (rectheader.r.w * bitsPixel + 7) / 8;
 
  /* rect is to small to be compressed reasonably, simply copy */
  if (rectheader.r.h * rowSize < TIGHT_MIN_TO_COMPRESS) {
    if (!read_from_rfb_server(sock, (char*)buffer, rectheader.r.h * rowSize))
      return 0;

    dst = (void *) &buffer[TIGHT_MIN_TO_COMPRESS * 4];
    filterFn(rectheader.r.h, buffer, dst);
    dfb_write_data_to_screen(
	 rectheader.r.x, 
	 rectheader.r.y, 
	 rectheader.r.w, 
	 rectheader.r.h, 
	 dst
	 );

    return 1;
  }

  /* Read the length (1..3 bytes) of compressed data following. */
  compressedLen = (int)ReadCompactLen();
  if (compressedLen <= 0) {
     fprintf(stderr, "Incorrect data received from the server.\n");
     return 0;
  }

  /* Now let's initialize compression stream if needed. */
  stream_id = comp_ctl & 0x03;
  zs = &zlibStream[stream_id];
  if (!zlibStreamActive[stream_id]) {
     zs->zalloc = Z_NULL;
     zs->zfree = Z_NULL;
     zs->opaque = Z_NULL;
     err = inflateInit(zs);
     if (err != Z_OK) {
	if (zs->msg != NULL)
	   fprintf(stderr, "InflateInit error: %s.\n", zs->msg);
	return 0;
     }
     zlibStreamActive[stream_id] = 1;
  }

  /* Read, decode and draw actual pixel data in a loop. */
  bufferSize = 640*480 * bitsPixel / (bitsPixel + opt.client.bpp) & 0xFFFFFFFC;
  dst = &buffer[bufferSize];
  if (rowSize > bufferSize) {
     /* Should be impossible when BUFFER_SIZE >= 16384 */
     fprintf(stderr, "Internal error: incorrect buffer size.\n");
     return 0;
  }

  rowsProcessed = 0;
  extraBytes = 0;

  while (compressedLen > 0) {
     if (compressedLen > ZLIB_BUFFER_SIZE)
	portionLen = ZLIB_BUFFER_SIZE;
     else
	portionLen = compressedLen;

     if (!read_from_rfb_server(sock, (char*)zlib_buffer, portionLen))
	return 0;

     compressedLen -= portionLen;

     zs->next_in = (Bytef *)zlib_buffer;
     zs->avail_in = portionLen;

     do {
	zs->next_out = (Bytef *)&buffer[extraBytes];
	zs->avail_out = bufferSize - extraBytes;

	err = inflate(zs, Z_SYNC_FLUSH);
	if (err == Z_BUF_ERROR)   /* Input exhausted -- no problem. */
	   break;
	if (err != Z_OK && err != Z_STREAM_END) {
	   if (zs->msg != NULL) {
	      fprintf(stderr, "Inflate error: %s.\n", zs->msg);
	   } else {
	      fprintf(stderr, "Inflate error: %d.\n", err);
	   }
	   return 0;
	}

	numRows = (bufferSize - zs->avail_out) / rowSize;

	filterFn(numRows, buffer, dst);

	extraBytes = bufferSize - zs->avail_out - numRows * rowSize;
	if (extraBytes > 0)
	   memcpy(buffer, &buffer[numRows * rowSize], extraBytes);

        dfb_write_data_to_screen(
	      rectheader.r.x, 
	      rectheader.r.y + rowsProcessed, 
	      rectheader.r.w, 
	      numRows, 
	      dst);
	rowsProcessed += numRows;
     }
     while (zs->avail_out == 0);
  }

  if (rowsProcessed != rectheader.r.h) {
     fprintf(stderr, "Incorrect number of scan lines after decompression.\n");
     return 0;
  }
  
  return 1;
}

/*----------------------------------------------------------------------------
 *
 * Filter stuff.
 *
 */


int
InitFilterCopy (int rw, int rh)
{
  rectWidth = rw;
  return opt.client.bpp;
}

void
FilterCopy (int numRows, void *src, void *dst)
{
   memcpy (dst, src, numRows * rectWidth * (opt.client.bpp / 8));
}

int
InitFilterGradient (int rw, int rh)
{
  int bits;

  bits = InitFilterCopy(rw, rh);
  if (cutZeros)
    memset(tightPrevRow, 0, rw * 3);
  else
    memset(tightPrevRow, 0, rw * 3 * sizeof(CARD16));

  return bits;
}

void
FilterGradient (int numRows, void* buffer, void *buffer2)
{
  int x, y, c;
  CARD16 *src = (CARD16 *)buffer;
  CARD16 *dst = (CARD16 *)buffer2;
  CARD16 *thatRow = (CARD16 *)tightPrevRow;
  CARD16 thisRow[2048*3];
  CARD16 pix[3];
  CARD16 max[3];
  int shift[3];
  int est[3];


  max[0] = opt.client.redmax;
  max[1] = opt.client.greenmax;
  max[2] = opt.client.bluemax;

  shift[0] = opt.client.redshift;
  shift[1] = opt.client.greenshift;
  shift[2] = opt.client.blueshift;

  for (y = 0; y < numRows; y++) {

      /* First pixel in a row */
      for (c = 0; c < 3; c++) {
      pix[c] = (CARD16)(((src[y*rectWidth] >> shift[c]) + thatRow[c]) & max[c]);
      thisRow[c] = pix[c];
    }
    dst[y*rectWidth] = RGB_TO_PIXEL(opt.client.bpp, pix[0], pix[1], pix[2]);

      /* Remaining pixels of a row */
      for (x = 1; x < rectWidth; x++) {
      for (c = 0; c < 3; c++) {
	est[c] = (int)thatRow[x*3+c] + (int)pix[c] - (int)thatRow[(x-1)*3+c];
	if (est[c] > (int)max[c]) {
	  est[c] = (int)max[c];
	} else if (est[c] < 0) {
	  est[c] = 0;
	}
	pix[c] = (CARD16)(((src[y*rectWidth+x] >> shift[c]) + est[c]) & max[c]);
	thisRow[x*3+c] = pix[c];
      }
      dst[y*rectWidth+x] = RGB_TO_PIXEL(opt.client.bpp, pix[0], pix[1], pix[2]);
    }
    memcpy(thatRow, thisRow, rectWidth * 3 * sizeof(CARD16));
  }
}

int
InitFilterPalette (int rw, int rh)
{
  CARD8 numColors;
  rectWidth = rw;

  if (!read_from_rfb_server(sock, (char*)&numColors, 1))
    return 0;

  rectColors = (int)numColors;
  if (++rectColors < 2)
    return 0;

  if (!read_from_rfb_server(sock, (char*)&tightPalette, rectColors * (opt.client.bpp / 8)))
    return 0;

  return (rectColors == 2) ? 1 : 8;
}

void
FilterPalette (int numRows, void *buffer, void *buffer2)
{
   /* FIXME works only in 16 bpp. The dst and palette pointers would need to
    * be adjusted in size. Maybe after dinner ;) */
  int x, y, b, w;
  CARD8 *src = (CARD8 *)buffer;
  CARD16 *dst = (CARD16 *)buffer2;
  CARD16 *palette = (CARD16 *)tightPalette;

  if (rectColors == 2) {
    w = (rectWidth + 7) / 8;
    for (y = 0; y < numRows; y++) {
      for (x = 0; x < rectWidth / 8; x++) {
	for (b = 7; b >= 0; b--)
	  dst[y*rectWidth+x*8+7-b] = palette[src[y*w+x] >> b & 1];
      }
      for (b = 7; b >= 8 - rectWidth % 8; b--) {
	dst[y*rectWidth+x*8+7-b] = palette[src[y*w+x] >> b & 1];
      }
    }
  } else {
    for (y = 0; y < numRows; y++)
      for (x = 0; x < rectWidth; x++)
	 dst[y*rectWidth+x] = palette[(int)src[y*rectWidth+x]];
  }
}

int
_handle_zlib_encoded_message(rfbFramebufferUpdateRectHeader rectheader)
{
  rfbZlibHeader hdr;
  int remaining;
  int inflateResult;
  int toRead;

  /* First make sure we have a large enough raw buffer to hold the
   * decompressed data.  In practice, with a fixed BPP, fixed frame
   * buffer size and the first update containing the entire frame
   * buffer, this buffer allocation should only happen once, on the
   * first update.
   */
  if ( raw_buffer_size < (( rectheader.r.w * rectheader.r.h ) 
	                   * ( opt.client.bpp / 8 ))) {

    if ( raw_buffer != NULL ) {

      free( raw_buffer );

    }

    raw_buffer_size = (( rectheader.r.w * rectheader.r.h ) 
	                * ( opt.client.bpp / 8 ));
    raw_buffer = (char*) malloc( raw_buffer_size );

  }

  if (!read_from_rfb_server(sock, (char *)&hdr, sz_rfbZlibHeader))
    return 0;

  remaining = Swap32IfLE(hdr.nBytes);

  /* Need to initialize the decompressor state. */
  decompStream.next_in   = ( Bytef * )buffer;
  decompStream.avail_in  = 0;
  decompStream.next_out  = ( Bytef * )raw_buffer;
  decompStream.avail_out = raw_buffer_size;
  decompStream.data_type = Z_BINARY;

  /* Initialize the decompression stream structures on the first invocation. */
  if ( decompStreamInited == 0 ) {

    inflateResult = inflateInit( &decompStream );

    if ( inflateResult != Z_OK ) {
      fprintf(stderr,
              "inflateInit returned error: %d, msg: %s\n",
              inflateResult,
              decompStream.msg);
      return 0;
    }

    decompStreamInited = 1;

  }

  inflateResult = Z_OK;

  /* Process buffer full of data until no more to process, or
   * some type of inflater error, or Z_STREAM_END.
   */
  while (( remaining > 0 ) &&
         ( inflateResult == Z_OK )) {
  
    if ( remaining > BUFFER_SIZE ) {
      toRead = BUFFER_SIZE;
    }
    else {
      toRead = remaining;
    }

    /* Fill the buffer, obtaining data from the server. */
    if (!read_from_rfb_server(sock, buffer,toRead))
      return 0;

    decompStream.next_in  = ( Bytef * )buffer;
    decompStream.avail_in = toRead;

    /* Need to uncompress buffer full. */
    inflateResult = inflate( &decompStream, Z_SYNC_FLUSH );

    /* We never supply a dictionary for compression. */
    if ( inflateResult == Z_NEED_DICT ) {
      fprintf(stderr,"zlib inflate needs a dictionary!\n");
      return 0;
    }
    if ( inflateResult < 0 ) {
      fprintf(stderr,
              "zlib inflate returned error: %d, msg: %s\n",
              inflateResult,
              decompStream.msg);
      return 0;
    }

    /* Result buffer allocated to be at least large enough.  We should
     * never run out of space!
     */
    if (( decompStream.avail_in > 0 ) &&
        ( decompStream.avail_out <= 0 )) {
      fprintf(stderr,"zlib inflate ran out of space!\n");
      return 0;
    }

    remaining -= toRead;

  } /* while ( remaining > 0 ) */

  if ( inflateResult == Z_OK ) {

    /* Put the uncompressed contents of the update on the screen. */
    dfb_write_data_to_screen(
	 rectheader.r.x, 
	 rectheader.r.y, 
	 rectheader.r.w, 
	 rectheader.r.h, 
	 raw_buffer
	 );


  }
  else {

    fprintf(stderr,
            "zlib inflate returned error: %d, msg: %s\n",
            inflateResult,
            decompStream.msg);
    return 0;

  }

  return 1;
}
