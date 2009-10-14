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


#ifndef DEBUG_H
#define DEBUG_H

#define DEBUG

#ifdef WITH_DMALLOC
#include <dmalloc.h>
#define emalloc(a) malloc(a)
#define estrdup(a) strdup(a)
#define erealloc(a,b) realloc(a,b)
#else
#define emalloc(a) _emalloc(a)
#define estrdup(a) _estrdup(a)
#define erealloc(a,b) _erealloc(a,b)
#endif

#endif
