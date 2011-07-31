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
 * along with this program; if not, a copy can be downloaded from 
 * http://www.gnu.org/licenses/gpl.html, or obtained by writing to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, 
 * Boston, MA 02110-1301, USA.
 */
#include "directvnc.h"
#include <math.h>
#include "keysym.h"
#define KeySym int

/* DirectFB interfaces needed */
IDirectFB               *dfb          = NULL;
IDirectFBSurface        *primary      = NULL;
IDirectFBDisplayLayer   *layer        = NULL;
IDirectFBInputDevice    *keyboard     = NULL;
IDirectFBInputDevice    *mouse        = NULL;
IDirectFBEventBuffer    *input_buffer = NULL;
DFBResult err;
DFBSurfaceDescription dsc;
DFBGraphicsDeviceDescription caps;
DFBDisplayLayerConfig layer_config;
DFBRegion rect;
DFBRectangle scratch_rect;

static KeySym DirectFBTranslateSymbol (DFBInputDeviceKeymapEntry *entry, int index);

void
dfb_init(int argc, char *argv[])
{
     DFBCHECK(DirectFBInit( &argc, &argv ));

     /* quiet option is no longer supported in DFB 1.2 */
     /* DFBCHECK(DirectFBSetOption ("quiet", "")); */

     /* create the super interface */
     DFBCHECK(DirectFBCreate( &dfb ));

     dfb->SetCooperativeLevel(dfb, DFSCL_FULLSCREEN);

     DFBCHECK(dfb->GetDisplayLayer( dfb, DLID_PRIMARY, &layer ));
     layer->GetConfiguration (layer, &layer_config);

     /* get the primary surface, i.e. the surface of the primary layer we have
	exclusive access to */
     memset( &dsc, 0, sizeof(DFBSurfaceDescription) );     
     dsc.flags = 
	DSDESC_CAPS | DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT;
     dsc.width = layer_config.width;
     dsc.height = layer_config.height;

     dsc.caps = DSCAPS_PRIMARY | DSCAPS_SYSTEMONLY /*| DSCAPS_FLIPPING */;
     /* FIXME */
     dsc.pixelformat = DSPF_RGB16;
     DFBCHECK(dfb->CreateSurface(dfb, &dsc, &primary ));
     primary->GetSize (primary, &opt.client.width, &opt.client.height);

     DFBCHECK(dfb->GetInputDevice( dfb, DIDID_KEYBOARD, &keyboard ));
     DFBCHECK(dfb->GetInputDevice( dfb, DIDID_MOUSE, &mouse ));
     DFBCHECK (dfb->CreateInputEventBuffer (dfb, DICAPS_ALL, DFB_TRUE, &input_buffer));
}


/*
 * deinitializes resources and DirectFB
 */
void 
dfb_deinit()
{
    if ( primary )
         primary->Release( primary );
    if ( input_buffer )
        input_buffer->Release( input_buffer );
    if ( keyboard )
        keyboard->Release( keyboard );
    if ( mouse )
        mouse->Release( mouse );
    if ( layer )
        layer->Release( layer );
    if ( dfb )
        dfb->Release( dfb );
}
 
void
dfb_flip()
{
   primary->Flip(primary, NULL, DSFLIP_WAITFORSYNC);
}

void
dfb_flip_rect(x, y, w, h)
{
   rect.x1 = x+opt.h_offset;
   rect.y1 = y+opt.v_offset;
   rect.x2 = x+w;
   rect.y2 = y+h;
   primary->Flip(primary, &rect, DSFLIP_WAITFORSYNC);
}

int
dfb_write_data_to_screen(int x, int y, int w, int h, void *data)
{
 
   char *dst;
   int pitch;         /* number of bytes per row */
   int orig_src_pitch, src_pitch;     

   orig_src_pitch = w * opt.client.bpp/8; 
   /* make sure we dont exceed client dimensions */
   if (x > opt.client.width  || y > opt.client.height)
	   return 1; 
   if ( x+w > opt.client.width)
	   w = opt.client.width - x;
   if ( y+h > opt.client.height)
	   h = opt.client.height - y;
   
   src_pitch = w * opt.client.bpp/8; 
   
   if(primary->Lock(primary, DSLF_WRITE, (void**)(&dst), &pitch) ==DFB_OK)
   {
      int i;
      dst += opt.v_offset * pitch;
      dst += (y*pitch + ( (x+opt.h_offset) * opt.client.bpp/8) );
      for (i=0;i<h;i++)
      {
	 memcpy (dst, data, src_pitch);
	 data += orig_src_pitch;
	 dst += pitch ;
      }	
      primary->Unlock (primary);
   }
   dfb_flip_rect (x,y,w,h);
   return 1;
}

int   
dfb_copy_rect(int src_x, int src_y, int dest_x, int dest_y, int w, int h)
{
   /* make sure we dont exceed client dimensions */
   if (   src_x > opt.client.width 
       || src_y > opt.client.height
       || dest_x > opt.client.width 
       || dest_y > opt.client.height)
	   return 1; 
   if ( src_x+w > opt.client.width)
	   w = opt.client.width - src_x;
   if ( src_y+h > opt.client.height)
	   h = opt.client.height - src_y;
   
   scratch_rect.x = src_x+opt.h_offset;
   scratch_rect.y = src_y+opt.v_offset;
   scratch_rect.w = w;
   scratch_rect.h = h;

   primary->Blit(primary, primary, &scratch_rect, 
	         dest_x+opt.h_offset, dest_y+opt.v_offset);
   dfb_flip_rect (src_x,src_y,w,h);
   return 1;
}

int
dfb_draw_rect_with_rgb(int x, int y, int w, int h, int r, int g, int b)
{
   /* make sure we dont exceed client dimensions */
   if (x > opt.client.width  || y > opt.client.height)
	   return 1; 
   if ( x+w > opt.client.width)
	   w = opt.client.width - x;
   if ( y+h > opt.client.height)
	   h = opt.client.height - y;
   

   primary->SetColor(primary, r,g,b,0xFF);
   primary->FillRectangle(primary, x+opt.h_offset,y+opt.v_offset,w,h);
   dfb_flip_rect (x,y,w,h);
   return 1;
}


IDirectFBSurface *
dfb_create_cursor_saved_area(int width, int height)
{
   IDirectFBSurface *surf;
   memset( &dsc, 0, sizeof(DFBSurfaceDescription) );     
   dsc.flags = DSDESC_CAPS | DSDESC_WIDTH;
   dsc.width = width;
   dsc.height = height;

   DFBCHECK(dfb->CreateSurface(dfb, &dsc, &surf));
   return surf;
}

void 
dfb_restore_cursor_rect( IDirectFBSurface *surf, int x, int y, int w, int h)
{
   int surf_w, surf_h;
   surf->GetSize(surf, &surf_w, &surf_h);
   scratch_rect.x = surf_w-w;
   scratch_rect.y = surf_h-h;
   scratch_rect.w = w;
   scratch_rect.h = h;
   primary->Blit(primary, surf, &scratch_rect, x+opt.h_offset, y+opt.v_offset);
}

void 
dfb_save_cursor_rect( IDirectFBSurface *surf, int x, int y, int w, int h)
{
   int surf_w, surf_h;
   scratch_rect.x = x + opt.h_offset;
   scratch_rect.y = y + opt.v_offset;
   scratch_rect.w = w;
   scratch_rect.h = h;
   surf->GetSize(surf, &surf_w, &surf_h);
   surf->Blit(surf, primary, &scratch_rect, surf_w-w, surf_h -h);
}

static KeySym
_translate_with_modmap (DFBInputDeviceKeymapEntry *entry, int index, DFBInputDeviceLockState lkst, int ctrl) {
   if (opt.modmapfile != NULL && !ctrl) {
       KeySym ks = modmap_translate_code(entry->code, lkst, index & 1);
       if (ks != XK_VoidSymbol) return ks;
   }
   return DirectFBTranslateSymbol(entry, index);
}


static void
_dfb_handle_key_event(DFBInputEvent evt, int press_or_release)
{	
   int keysym;
   int level = 0;
   int ctrl = 0;
   char keytype = 0; /* '',A,N,K */
   DFBInputDeviceLockState lkst = 0;
   DFBInputDeviceKeymapEntry entry;
   keyboard->GetKeymapEntry(keyboard, evt.key_code, &entry);
   /* OK according to the RFB specs we need to modify
      any key_codes to thier symbols. This includes
      caps lock and numlock, as they are ignored by the
      server.
      It might be good to back through this code sometime
      to make is execute faster. */
   /*What type of key do we have ALPHA,NUMBER,KEYPAD,OTHER */
   if ((evt.key_id >= DIKI_KP_DIV) && (evt.key_id<= DIKI_KP_9))
      keytype='K';
   if ((evt.key_id >= DIKI_A) && (evt.key_id<= DIKI_Z))
      keytype='A';
   if ((evt.key_id >= DIKI_0) && (evt.key_id<= DIKI_9))
      keytype='N';

   /*CASES: */
   /* SHIFT=shifton for all */
   if (evt.modifiers & DIMM_SHIFT) {
      level = 1; /*do the shift*/
      /* SHIFT NUMLOCK CAPSLOCK == ALPHA-Normal, Numbers-Shifted, and KP-Normal */
      if ((evt.locks & DILS_CAPS) && (evt.locks & DILS_NUM) &&
         ((keytype=='K')||(keytype=='A')) ) {
           level = 0;
      } else {
         /* SHIFT CAPSLOCK == ALPHA-Normal, Numbers-Shifted, and KP-Shifted */
         if ((evt.locks & DILS_CAPS) && (keytype=='A')) level = 0;
         /* SHIFT NUMLOCK == ALPHA-Shifted, Numbers-Shifted, and KP-Normal */
         if ((evt.locks & DILS_NUM) && (keytype=='K')) level = 0;
      }

   } else {
      /* CAPSLOCK == ALPHA-Shifted, Numbers-Normal, and KP-Normal */
      if ((evt.locks & DILS_CAPS) && (keytype=='A')) level = 1;
      /* NUMLOCK == ALPHA-Normal, Numbers-Normal, and KP-Shifted */
      if ((evt.locks & DILS_NUM) && (keytype=='K')) level = 1;
   }

   if (evt.modifiers & DIMM_ALTGR) 
      level = level + 2;
   if (evt.modifiers & DIMM_CONTROL)
      ctrl = 1;
   keysym = _translate_with_modmap(&entry, level, lkst, ctrl);
   rfb_send_key_event(keysym, press_or_release); 	     
}

int
dfb_wait_for_event_with_timeout(int milliseconds)
{
   return input_buffer->WaitForEventWithTimeout(input_buffer, 0, milliseconds);
}

int
dfb_wait_for_event()
{
   return input_buffer->WaitForEvent(input_buffer);
}


int
dfb_process_events()
{
   DFBInputEvent evt;

   /* we need to check whether the dfb ressources have been set up because
    * this might be called during handshaking when dfb_init has not been
    * called yet. This is the disadvantage of processing client events
    * whenever the socket would block. The other option would be to initialize
    * the dfb ressources (input input_buffer) before everything else, but then the
    * screen gets blanked for every unsuccessful connect (wrong password)
    * which is not pretty either. I think I prefer checking here for the time
    * being. */
   if (!dfb)
      return 0;
   
   while(input_buffer->GetEvent(input_buffer, DFB_EVENT(&evt)) == DFB_OK)
   {
      switch (evt.type)
      {
	 case DIET_KEYPRESS:
	    /* quit on ctrl-q FIXME make this configurable*/
	    if (evt.key_id == DIKI_Q && evt.modifiers & DIMM_CONTROL)
	    {
	       /* Ugh.
		* The control key is still pressed when we disconnect, so it 
		* is still pressed when the next session attaches. Since DFB
		* doesnt discern the two control keys, we send release events
		* for both. X just ignores releases for unpressed keys, 
		* luckily */
	       rfb_send_key_event(XK_Control_L, 0); 	     
	       rfb_send_key_event(XK_Control_R, 0); 	     
	       dfb_deinit();
	       exit(1);
	    }
	    _dfb_handle_key_event(evt, 1);
	    break;
	 case DIET_KEYRELEASE:
	    _dfb_handle_key_event(evt, 0);
	    break;
	 case DIET_AXISMOTION:
	    if (evt.flags & DIEF_AXISREL)
	    {
	       switch (evt.axis)
	       {
		  case DIAI_X:
		     mousestate.x += evt.axisrel;	
		     break;
		  case DIAI_Y:
		     mousestate.y += evt.axisrel;
		     break;
		  default:
		     break;
	       }
	       rfb_update_mouse();
	    }
	    break;
	 case DIET_BUTTONPRESS:
	    switch (evt.button)
	    {
	       case DIBI_LEFT:	  
		  mousestate.buttonmask |= rfbButton1Mask;
		  break;
	       case DIBI_MIDDLE:
		  mousestate.buttonmask |= rfbButton2Mask;
		  break;
	       case DIBI_RIGHT:
		  mousestate.buttonmask |= rfbButton3Mask;
		  break;
	       default:
		  //fprintf(stdout, "Are we capturing the wheel here? %d\n", evt.button);
		  break;
	    }
	    rfb_update_mouse();
	    break;
	 case DIET_BUTTONRELEASE:
	    switch (evt.button)
	    {
	       case DIBI_LEFT: 	
		  mousestate.buttonmask &= ~rfbButton1Mask;
		  break;
	       case DIBI_MIDDLE:
		  mousestate.buttonmask &= ~rfbButton2Mask;
		  break;
	       case DIBI_RIGHT:
		  mousestate.buttonmask &= ~rfbButton3Mask;
		  break;
	       default:
		  break;
	    }
	    rfb_update_mouse();
	    break;
   
	 case DIET_UNKNOWN:  /* fallthrough */
	 default:
	    fprintf(stderr, "Unknown event: %d\n", evt.type);
	    break;
      }
   }
   return 1;
   
}


/*
   (c) Copyright 2002  Denis Oliver Kropp <dok@directfb.org>
   All rights reserved.

===========================================================================

 An X keyCode must be in the range XkbMinLegalKeyCode (8) to
 XkbMaxLegalKeyCode(255).

 The keyCodes we get from the kernel range from 0 to 127, so we need to
 offset the range before passing the keyCode to X.

 An X KeySym is an extended ascii code that is device independent.

 The modifier map is accessed by the keyCode, but the normal map is
 accessed by keyCode - MIN_KEYCODE.  Sigh.

===========================================================================
*/


/* This table assumes an iso8859_1 encoding for the characters 
 * > 80, as returned by pccons */
static KeySym latin1_to_x[256] = {
	XK_VoidSymbol,	XK_VoidSymbol,	XK_VoidSymbol,	XK_VoidSymbol,
	XK_VoidSymbol,	XK_VoidSymbol,	XK_VoidSymbol,	XK_VoidSymbol,
	XK_BackSpace,	XK_Tab,		XK_Linefeed,	XK_VoidSymbol,
	XK_VoidSymbol,	XK_Return,	XK_VoidSymbol,	XK_VoidSymbol,
	XK_VoidSymbol,	XK_VoidSymbol,	XK_VoidSymbol,	XK_VoidSymbol,
	XK_VoidSymbol,	XK_VoidSymbol,	XK_VoidSymbol,	XK_VoidSymbol,
	XK_Cancel,	XK_VoidSymbol,	XK_VoidSymbol,	XK_Escape,
	XK_VoidSymbol,	XK_VoidSymbol,	XK_VoidSymbol,	XK_VoidSymbol,
	XK_space,	XK_exclam,	XK_quotedbl,	XK_numbersign,
	XK_dollar,	XK_percent,	XK_ampersand,	XK_apostrophe,
	XK_parenleft,	XK_parenright,	XK_asterisk,	XK_plus,
	XK_comma,	XK_minus,	XK_period,	XK_slash,
	XK_0,		XK_1,		XK_2,		XK_3,
	XK_4,		XK_5,		XK_6,		XK_7,
	XK_8,		XK_9,		XK_colon,	XK_semicolon,
	XK_less,	XK_equal,	XK_greater,	XK_question,
	XK_at,		XK_A,		XK_B,		XK_C,
	XK_D,		XK_E,		XK_F,		XK_G,
	XK_H,		XK_I,		XK_J,		XK_K,
	XK_L,		XK_M,		XK_N,		XK_O,
	XK_P,		XK_Q,		XK_R,		XK_S,
	XK_T,		XK_U,		XK_V,		XK_W,
	XK_X,		XK_Y,		XK_Z,		XK_bracketleft,
	XK_backslash,	XK_bracketright,XK_asciicircum,	XK_underscore,
	XK_grave,	XK_a,		XK_b,		XK_c,
	XK_d,		XK_e,		XK_f,		XK_g,
	XK_h,		XK_i,		XK_j,		XK_k,
	XK_l,		XK_m,		XK_n,		XK_o,
	XK_p,		XK_q,		XK_r,		XK_s,
	XK_t,		XK_u,		XK_v,		XK_w,
	XK_x,		XK_y,		XK_z,		XK_braceleft,
	XK_bar,		XK_braceright,	XK_asciitilde,	XK_Delete,
	XK_VoidSymbol,	XK_VoidSymbol,	XK_VoidSymbol,	XK_VoidSymbol,
	XK_VoidSymbol,	XK_VoidSymbol,	XK_VoidSymbol,	XK_VoidSymbol,
	XK_VoidSymbol,	XK_VoidSymbol,	XK_VoidSymbol,	XK_VoidSymbol,
	XK_VoidSymbol,	XK_VoidSymbol,	XK_VoidSymbol,	XK_VoidSymbol,
	XK_VoidSymbol,	XK_VoidSymbol,	XK_VoidSymbol,	XK_VoidSymbol,
	XK_VoidSymbol,	XK_VoidSymbol,	XK_VoidSymbol,	XK_VoidSymbol,
	XK_VoidSymbol,	XK_VoidSymbol,	XK_VoidSymbol,	XK_VoidSymbol,
	XK_VoidSymbol,	XK_VoidSymbol,	XK_VoidSymbol,	XK_VoidSymbol,
	XK_nobreakspace,XK_exclamdown,	XK_cent,	XK_sterling,
	XK_currency,	XK_yen,		XK_brokenbar,	XK_section,
	XK_diaeresis,	XK_copyright,	XK_ordfeminine,	XK_guillemotleft,
	XK_notsign,	XK_hyphen,	XK_registered,	XK_macron,
	XK_degree,	XK_plusminus,	XK_twosuperior,	XK_threesuperior,
	XK_acute,	XK_mu,		XK_paragraph,	XK_periodcentered,
	XK_cedilla,	XK_onesuperior,	XK_masculine,	XK_guillemotright,
	XK_onequarter,	XK_onehalf,	XK_threequarters,XK_questiondown,
	XK_Agrave,	XK_Aacute,	XK_Acircumflex,	XK_Atilde,
	XK_Adiaeresis,	XK_Aring,	XK_AE,		XK_Ccedilla,
	XK_Egrave,	XK_Eacute,	XK_Ecircumflex,	XK_Ediaeresis,
	XK_Igrave,	XK_Iacute,	XK_Icircumflex,	XK_Idiaeresis,
	XK_ETH,		XK_Ntilde,	XK_Ograve,	XK_Oacute,
	XK_Ocircumflex,	XK_Otilde,	XK_Odiaeresis,	XK_multiply,
	XK_Ooblique,	XK_Ugrave,	XK_Uacute,	XK_Ucircumflex,
	XK_Udiaeresis,	XK_Yacute,	XK_THORN,	XK_ssharp,
	XK_agrave,	XK_aacute,	XK_acircumflex,	XK_atilde,
	XK_adiaeresis,	XK_aring,	XK_ae,	        XK_ccedilla,
	XK_egrave,	XK_eacute,	XK_ecircumflex,	XK_ediaeresis,
	XK_igrave,	XK_iacute,	XK_icircumflex, XK_idiaeresis,
	XK_eth,		XK_ntilde,	XK_ograve, 	XK_oacute,
	XK_ocircumflex,	XK_otilde,	XK_odiaeresis,	XK_division,
	XK_oslash,	XK_ugrave,	XK_uacute,	XK_ucircumflex,
	XK_udiaeresis,	XK_yacute,	XK_thorn, 	XK_ydiaeresis
      };

static DFBInputDeviceKeymapSymbolIndex diksi[4] = {
  DIKSI_BASE,
  DIKSI_BASE_SHIFT,
  DIKSI_ALT,
  DIKSI_ALT_SHIFT
};

static KeySym
DirectFBTranslateSymbol (DFBInputDeviceKeymapEntry *entry, int index)
{
  DFBInputDeviceKeySymbol     symbol = entry->symbols[diksi[index]];
  DFBInputDeviceKeyIdentifier id     = entry->identifier;

  if (id >= DIKI_KP_DIV && id <= DIKI_KP_9)
    {
      if (symbol >= DIKS_0 && symbol <= DIKS_9)
        return XK_KP_0 + symbol - DIKS_0;

      switch (symbol)
        {
        case DIKS_HOME:
          return XK_KP_Home;

        case DIKS_CURSOR_LEFT:
          return XK_KP_Left;

        case DIKS_CURSOR_UP:
          return XK_KP_Up;

        case DIKS_CURSOR_RIGHT:
          return XK_KP_Right;

        case DIKS_CURSOR_DOWN:
          return XK_KP_Down;

        case DIKS_PAGE_UP:
          return XK_KP_Page_Up;

        case DIKS_PAGE_DOWN:
          return XK_KP_Page_Down;

        case DIKS_END:
          return XK_KP_End;

        case DIKS_BEGIN:
          return XK_KP_Begin;

        case DIKS_INSERT:
          return XK_KP_Insert;

        case DIKS_DELETE:
          return XK_KP_Delete;

        default:
          ;
        }

      switch (id)
        {
        case DIKI_KP_DIV:
          return XK_KP_Divide;

        case DIKI_KP_MULT:
          return XK_KP_Multiply;

        case DIKI_KP_MINUS:
          return XK_KP_Subtract;

        case DIKI_KP_PLUS:
          return XK_KP_Add;

        case DIKI_KP_ENTER:
          return XK_KP_Enter;

        case DIKI_KP_SPACE:
          return XK_KP_Space;

        case DIKI_KP_TAB:
          return XK_KP_Tab;

        case DIKI_KP_F1:
          return XK_KP_F1;

        case DIKI_KP_F2:
          return XK_KP_F2;

        case DIKI_KP_F3:
          return XK_KP_F3;

        case DIKI_KP_F4:
          return XK_KP_F4;

        case DIKI_KP_EQUAL:
          return XK_KP_Equal;

        case DIKI_KP_DECIMAL:
          return XK_KP_Decimal;

        case DIKI_KP_SEPARATOR:
          return XK_KP_Separator;

        default:
          ;
        }
    }

  if (symbol == DIKS_TAB && (index & 1))
    return XK_ISO_Left_Tab;

  if (symbol > 0 && symbol < 256)
    return latin1_to_x[symbol];

  if (DFB_KEY_TYPE (symbol) == DIKT_FUNCTION && symbol < DFB_FUNCTION_KEY(36))
    return XK_F1 + symbol - DIKS_F1;

  switch (id)
    {
    case DIKI_SHIFT_L:
      return XK_Shift_L;

    case DIKI_SHIFT_R:
      return XK_Shift_R;

    case DIKI_CONTROL_L:
      return XK_Control_L;

    case DIKI_CONTROL_R:
      return XK_Control_R;

    default:
      ;
    }


  switch (symbol)
    {
    case DIKS_CURSOR_LEFT:
      return XK_Left;

    case DIKS_CURSOR_RIGHT:
      return XK_Right;

    case DIKS_CURSOR_UP:
      return XK_Up;

    case DIKS_CURSOR_DOWN:
      return XK_Down;

    case DIKS_INSERT:
      return XK_Insert;

    case DIKS_HOME:
      return XK_Home;

    case DIKS_END:
      return XK_End;

    case DIKS_PAGE_UP:
      return XK_Page_Up;

    case DIKS_PAGE_DOWN:
      return XK_Page_Down;

    case DIKS_PRINT:
      return XK_Print;

    case DIKS_PAUSE:
      return XK_Pause;

    case DIKS_OK:
      return XK_Return;

    case DIKS_SELECT:
      return XK_Select;

    case DIKS_CLEAR:
      return XK_Clear;

    case DIKS_MENU:
      return XK_Menu;

    case DIKS_HELP:
      return XK_Help;


    case DIKS_ALT:
      return XK_Alt_L;

    case DIKS_ALTGR:
      return XK_Mode_switch;

    case DIKS_META:
      return XK_Meta_L;

    case DIKS_SUPER:
      return XK_Super_L;

    case DIKS_HYPER:
      return XK_Hyper_L;


    case DIKS_CAPS_LOCK:
      return XK_Caps_Lock;

    case DIKS_NUM_LOCK:
      return XK_Num_Lock;

    case DIKS_SCROLL_LOCK:
      return XK_Scroll_Lock;


    case DIKS_DEAD_GRAVE:
      return XK_dead_grave;

    case DIKS_DEAD_ACUTE:
      return XK_dead_acute;

    case DIKS_DEAD_CIRCUMFLEX:
      return XK_dead_circumflex;

    case DIKS_DEAD_TILDE:
      return XK_dead_tilde;

    case DIKS_DEAD_DIAERESIS:
      return XK_dead_diaeresis;

    case DIKS_DEAD_CEDILLA:
      return XK_dead_cedilla;

    default:
      ;
    }

  return 0;
}
