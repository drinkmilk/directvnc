/*
 *  Copyright (C) 2000, 2001 Const Kaplinsky.  All Rights Reserved.
 *  Copyright (C) 2000 Tridia Corporation.  All Rights Reserved.
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

#include "jpeg.h"
/*
 * JPEG source manager functions for JPEG decompression in Tight decoder.
 */
static int jpegError;
static struct jpeg_source_mgr jpegSrcManager;
static JOCTET *jpegBufferPtr;
static size_t *jpegBufferLen;

void
JpegInitSource(j_decompress_ptr cinfo)
{
  jpegError = 0;
}

int
JpegFillInputBuffer(j_decompress_ptr cinfo)
{
  jpegError = 1;
  jpegSrcManager.bytes_in_buffer = (int)jpegBufferLen;
  jpegSrcManager.next_input_byte = (JOCTET *)jpegBufferPtr;

  return TRUE;
}

void
JpegSkipInputData(j_decompress_ptr cinfo, long num_bytes)
{
  if (num_bytes < 0 || num_bytes > jpegSrcManager.bytes_in_buffer) {
    jpegError = 1;
    jpegSrcManager.bytes_in_buffer = (int)jpegBufferLen;
    jpegSrcManager.next_input_byte = (JOCTET *)jpegBufferPtr;
  } else {
    jpegSrcManager.next_input_byte += (size_t) num_bytes;
    jpegSrcManager.bytes_in_buffer -= (size_t) num_bytes;
  }
}

void
JpegTermSource(j_decompress_ptr cinfo)
{
  /* No work necessary here. */
}

void
JpegSetSrcManager(j_decompress_ptr cinfo, CARD8 *compressedData,
		  int compressedLen)
{
  jpegBufferPtr = (JOCTET *)compressedData;
  jpegBufferLen = (size_t*)compressedLen;

  jpegSrcManager.init_source = JpegInitSource;
  jpegSrcManager.fill_input_buffer = JpegFillInputBuffer;
  jpegSrcManager.skip_input_data = JpegSkipInputData;
  jpegSrcManager.resync_to_restart = jpeg_resync_to_restart;
  jpegSrcManager.term_source = JpegTermSource;
  jpegSrcManager.next_input_byte = jpegBufferPtr;
  jpegSrcManager.bytes_in_buffer = (int)jpegBufferLen;

  cinfo->src = &jpegSrcManager;
}


/*----------------------------------------------------------------------------
 *
 * JPEG decompression.
 *
 */

int
DecompressJpegRect(int x, int y, int w, int h)
{
  struct jpeg_decompress_struct cinfo;
  struct jpeg_error_mgr jerr;
  int compressedLen;
  CARD8 *compressedData;
  CARD16 *pixelPtr;
  JSAMPROW rowPointer[1];
  int dx, dy;

  compressedLen = (int)ReadCompactLen();
  if (compressedLen <= 0) {
    fprintf(stderr, "Incorrect data received from the server.\n");
    return 0;
  }

  compressedData = malloc(compressedLen);
  if (compressedData == NULL) {
    fprintf(stderr, "Memory allocation error.\n");
    return 0;
  }

  if (!read_from_rfb_server(sock, (char*)compressedData, compressedLen)) {
    free(compressedData);
    return 0;
  }

  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_decompress(&cinfo);

  JpegSetSrcManager(&cinfo, compressedData, compressedLen);

  jpeg_read_header(&cinfo, TRUE);
  cinfo.out_color_space = JCS_RGB;

  jpeg_start_decompress(&cinfo);
  if (cinfo.output_width != w || cinfo.output_height != h ||
      cinfo.output_components != 3) { 
    fprintf(stderr, "Tight Encoding: Wrong JPEG data received.\n");
    jpeg_destroy_decompress(&cinfo);
    free(compressedData);
    return 0;
  }

  rowPointer[0] = (JSAMPROW)buffer;
  dy = 0;
  while (cinfo.output_scanline < cinfo.output_height) {
    jpeg_read_scanlines(&cinfo, rowPointer, 1);
    if (jpegError) {
      break;
    }
    /* FIXME 16 bpp hardcoded */
    /* Fill the second half of our global buffer with the uncompressed data */
    pixelPtr = (CARD16 *)&buffer[BUFFER_SIZE / 2];
    for (dx = 0; dx < w; dx++) {
       *pixelPtr++ =
 	RGB24_TO_PIXEL(16, buffer[dx*3], buffer[dx*3+1], buffer[dx*3+2]);
     }
    
    /* write scanline to screen */
    dfb_write_data_to_screen(x, y + dy, w, 1, &buffer[BUFFER_SIZE/2]);
    dy++;
  }

  if (!jpegError)
    jpeg_finish_decompress(&cinfo);

  jpeg_destroy_decompress(&cinfo);
  free(compressedData);

  return !jpegError;
}

long
ReadCompactLen (void)
{
  long len;
  CARD8 b;

  if (!read_from_rfb_server(sock, (char *)&b, 1))
    return -1;
  len = (int)b & 0x7F;
  if (b & 0x80) {
    if (!read_from_rfb_server(sock, (char *)&b, 1))
      return -1;
    len |= ((int)b & 0x7F) << 7;
    if (b & 0x80) {
      if (!read_from_rfb_server(sock, (char *)&b, 1))
	return -1;
      len |= ((int)b & 0xFF) << 14;
    }
  }
  return len;
}


