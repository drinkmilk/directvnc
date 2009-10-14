/*
 *  Copyright (C) 2001 Const Kaplinsky.  All Rights Reserved.
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
 * cursor.c - code to support cursor shape updates (XCursor, RichCursor).
 */


#include "directvnc.h"

#define OPER_SAVE     0
#define OPER_RESTORE  1

#define Bool int
#define True 1
#define False 0


/* Data kept for RichCursor encoding support. */
static Bool prevRichCursorSet = False;
static IDirectFBSurface *rcSavedArea = NULL;
static CARD8 *rcSource, *rcMask;
static int rcHotX, rcHotY, rcWidth, rcHeight;
static int rcCursorX = 0, rcCursorY = 0;
static int rcLockX, rcLockY, rcLockWidth, rcLockHeight;
static Bool rcCursorHidden, rcLockSet;

static Bool SoftCursorInLockedArea(void);
static void SoftCursorCopyArea(int oper);
static void SoftCursorDraw(void);
static void FreeCursors(Bool setDotCursor);


/*********************************************************************
 * HandleRichCursor(). RichCursor shape updates support. This
 * variation of cursor shape updates cannot be supported directly via
 * Xlib cursors so we have to emulate cursor operating on the frame
 * buffer (that is why we call it "software cursor").
 ********************************************************************/

Bool HandleRichCursor(int xhot, int yhot, int width, int height)
{
  size_t bytesPerRow, bytesMaskData;
  char *buf;
  CARD8 *ptr;
  int x, y, b;

  bytesPerRow = (width + 7) / 8;
  bytesMaskData = bytesPerRow * height;

  FreeCursors(True);

  if (width * height == 0)
    return True;

  /* Read cursor pixel data. */

  rcSource = malloc(width * height * (opt.client.bpp / 8));
  if (rcSource == NULL)
    return False;

  if (!read_from_rfb_server(sock, (char *)rcSource,
			 width * height * (opt.client.bpp / 8))) {
    free(rcSource);
    return False;
  }

  /* Read and decode mask data. */

  buf = malloc(bytesMaskData);
  if (buf == NULL) {
    free(rcSource);
    return False;
  }

  if (!read_from_rfb_server(sock, buf, bytesMaskData)) {
    free(rcSource);
    free(buf);
    return False;
  }

  rcMask = malloc(width * height);
  if (rcMask == NULL) {
    free(rcSource);
    free(buf);
    return False;
  }

  ptr = rcMask;
  for (y = 0; y < height; y++) {
    for (x = 0; x < width / 8; x++) {
      for (b = 7; b >= 0; b--) {
	*ptr++ = buf[y * bytesPerRow + x] >> b & 1;
      }
    }
    for (b = 7; b > 7 - width % 8; b--) {
      *ptr++ = buf[y * bytesPerRow + x] >> b & 1;
    }
  }

  free(buf);

  /* Set remaining data associated with cursor. */
  rcSavedArea = dfb_create_cursor_saved_area(width, height);
  if (!rcSavedArea) {
     return False;
  }
  
  rcHotX = xhot;
  rcHotY = yhot;
  rcWidth = width;
  rcHeight = height;

  SoftCursorCopyArea(OPER_SAVE);
  SoftCursorDraw();

  rcCursorHidden = False;
  rcLockSet = False;

  prevRichCursorSet = True;
  return True;
}


/*********************************************************************
 * SoftCursorLockArea(). This function should be used to prevent
 * collisions between simultaneous framebuffer update operations and
 * cursor drawing operations caused by movements of pointing device.
 * The parameters denote a rectangle where mouse cursor should not be
 * drawn. Every next call to this function expands locked area so
 * previous locks remain active.
 ********************************************************************/

void SoftCursorLockArea(int x, int y, int w, int h)
{
  int newX, newY;
  if (!prevRichCursorSet)
    return;

  if (!rcLockSet) {
    rcLockX = x;
    rcLockY = y;
    rcLockWidth = w;
    rcLockHeight = h;
    rcLockSet = True;
  } else {
    newX = (x < rcLockX) ? x : rcLockX;
    newY = (y < rcLockY) ? y : rcLockY;
    rcLockWidth = (x + w > rcLockX + rcLockWidth) ?
      (x + w - newX) : (rcLockX + rcLockWidth - newX);
    rcLockHeight = (y + h > rcLockY + rcLockHeight) ?
      (y + h - newY) : (rcLockY + rcLockHeight - newY);
    rcLockX = newX;
    rcLockY = newY;
  }

  if (!rcCursorHidden && SoftCursorInLockedArea()) {
    SoftCursorCopyArea(OPER_RESTORE);
    rcCursorHidden = True;
  }
}

/*********************************************************************
 * SoftCursorUnlockScreen(). This function discards all locks
 * performed since previous SoftCursorUnlockScreen() call.
 ********************************************************************/

void SoftCursorUnlockScreen(void)
{
  if (!prevRichCursorSet)
    return;

  if (rcCursorHidden) {
    SoftCursorCopyArea(OPER_SAVE);
    SoftCursorDraw();
    rcCursorHidden = False;
  }
  rcLockSet = False;
}

/*********************************************************************
 * SoftCursorMove(). Moves soft cursor in particular location. This
 * function respects locking of screen areas so when the cursor is
 * moved in the locked area, it becomes invisible until
 * SoftCursorUnlock() functions is called.
 ********************************************************************/

void SoftCursorMove(int x, int y)
{
  if (prevRichCursorSet && !rcCursorHidden) {
    SoftCursorCopyArea(OPER_RESTORE);
    rcCursorHidden = True;
  }

  rcCursorX = x;
  rcCursorY = y;

  if (prevRichCursorSet && !(rcLockSet && SoftCursorInLockedArea())) {
    SoftCursorCopyArea(OPER_SAVE);
    SoftCursorDraw();
    rcCursorHidden = False;
  }
}


/*********************************************************************
 * Internal (static) low-level functions.
 ********************************************************************/

static Bool SoftCursorInLockedArea(void)
{
  return (rcLockX < rcCursorX - rcHotX + rcWidth &&
	  rcLockY < rcCursorY - rcHotY + rcHeight &&
	  rcLockX + rcLockWidth > rcCursorX - rcHotX &&
	  rcLockY + rcLockHeight > rcCursorY - rcHotY);
}

static void SoftCursorCopyArea(int oper)
{
  int x, y, w, h;

  x = rcCursorX - rcHotX;
  y = rcCursorY - rcHotY;
  if (x >= opt.server.width || y >= opt.server.height)
    return;

  w = rcWidth;
  h = rcHeight;
  if (x < 0) {
    w += x;
    x = 0;
  } else if (x + w > opt.server.width) {
    w = opt.server.width - x;
  }
  if (y < 0) {
    h += y;
    y = 0;
  } else if (y + h > opt.server.height) {
    h = opt.server.height - y;
  }

  if (oper == OPER_SAVE) {
    /* Save screen area in memory. */
    dfb_save_cursor_rect(rcSavedArea, x,y,w,h);
  } else {
    /* Restore screen area. */
    dfb_restore_cursor_rect(rcSavedArea, x,y,w,h);
  }
}

static void SoftCursorDraw(void)
{
  int x, y, x0, y0;
  int offset, bytesPerPixel;
  char *pos;
  int r,g,b;

  bytesPerPixel = opt.client.bpp / 8;

  /* FIXME: Speed optimization is possible. */
  for (y = 0; y < rcHeight; y++) {
    y0 = rcCursorY - rcHotY + y;
    if (y0 >= 0 && y0 < opt.server.height) {
      for (x = 0; x < rcWidth; x++) {
	x0 = rcCursorX - rcHotX + x;
	if (x0 >= 0 && x0 < opt.server.width) {
	  offset = y * rcWidth + x;
	  if (rcMask[offset]) {
	    pos = (char *)&rcSource[offset * bytesPerPixel];
	    rfb_get_rgb_from_data(&r, &g, &b, pos);
	    dfb_draw_rect_with_rgb(x0, y0, 1, 1, r,g,b);
	  }
	}
      }
    }
  }
}

static void FreeCursors(Bool setDotCursor)
{
 
  if (prevRichCursorSet) {
    SoftCursorCopyArea(OPER_RESTORE);
    free(rcSource);
    free(rcMask);
    prevRichCursorSet = False;
  }
}

