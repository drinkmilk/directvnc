/*
 *  Copyright (C) 2000, 2001 Const Kaplinsky.  All Rights Reserved.
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

/* Type declarations for tight */

typedef void (*filterPtr)(int, void *, void *);

/* Prototypes for tight*/

int InitFilterCopy (int rw, int rh);
int InitFilterPalette (int rw, int rh);
int InitFilterGradient (int rw, int rh);
void FilterCopy (int numRows, void* srcBuffer, void *destBuffer);
void FilterPalette (int numRows, void* srcBuffer, void *destBuffer);
void FilterGradient (int numRows, void* srcBuffer, void *destBuffer);

int _handle_tight_encoded_message(rfbFramebufferUpdateRectHeader rectheader);
int _handle_zlib_encoded_message(rfbFramebufferUpdateRectHeader rectheader);

