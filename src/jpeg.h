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

#include <unistd.h>
#include <errno.h>
#include <pwd.h>
#include <vncauth.h>
#include "directvnc.h"
#include <jpeglib.h>

void JpegInitSource(j_decompress_ptr cinfo);
int JpegFillInputBuffer(j_decompress_ptr cinfo);
void JpegSkipInputData(j_decompress_ptr cinfo, long num_bytes);
void JpegTermSource(j_decompress_ptr cinfo);
void JpegSetSrcManager(j_decompress_ptr cinfo, CARD8 *compressedData,
                              int compressedLen);
int DecompressJpegRect(int x, int y, int w, int h);

long ReadCompactLen (void);


#define RGB24_TO_PIXEL(bpp,r,g,b)                                       \
   ((((CARD##bpp)(r) & 0xFF) * opt.client.redmax + 127) / 255             \
    << opt.client.redshift |                                              \
    (((CARD##bpp)(g) & 0xFF) * opt.client.greenmax + 127) / 255           \
    << opt.client.greenshift |                                            \
    (((CARD##bpp)(b) & 0xFF) * opt.client.bluemax + 127) / 255            \
    << opt.client.blueshift)    
