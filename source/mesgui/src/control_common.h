/*
 * Common header file included from gg*.cpp for the MESGUI library
 * Copyright (C) 2002  Eric Maxey <em32@mail.csuchico.edu>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#ifdef WIN32
#include <windows.h>
#endif
#include <GL/gl.h>
#include <stdio.h>
#include <stdlib.h>
#include "mesgui.h"
#include "texture.h"
#include "font.h"
#include "misc.h"

#define TO_X			4	// text offset
#define TO_Y			2
#define DRAG_THRESHOLD	4	// how much the mouse moves before it starts dragging

// messages
#define GGMSG_KEYDOWN		1	// (ascii value,0,0,0) key is pressed or is repeating due to holding key down
#define GGMSG_KEYUP			2	// (ascii value,0,0,0)
#define GGMSG_SKEYDOWN		3	// (GLUT_KEY_* special key value,0,0,0) special key messages
#define GGMSG_SKEYUP		4	// (GLUT_KEY_* special key value,0,0,0)
#define GGMSG_MOUSEBUTTON	5	// (button,state,x,y) any button up or down (position is relative to frame) only called for ggFrames
#define GGMSG_MOUSEMOTION	6	// (0,0,x,y)
#define GGMSG_GAINFOCUS		7	// (0,0,0,0) must set focus stuff i.e. parent->focus
#define GGMSG_LOSEFOCUS		8	// (0,0,0,0) must set focus stuff i.e. parent->focus
#define GGMSG_CLICKED		9	// (0,0,x,y)
#define GGMSG_DOWN			10	// (0,0,x,y) for ggButtons and ggMenus
#define GGMSG_UP			11	// (0,0,x,y)

const char messagestring[][20]={ "GGMSG_NONE"
								,"GGMSG_KEYDOWN"
								,"GGMSG_KEYUP"
								,"GGMSG_SKEYDOWN"
								,"GGMSG_SKEYUP"
								,"GGMSG_MOUSEBUTTON"
								,"GGMSG_MOUSEMOTION"
								,"GGMSG_GAINFOCUS"
								,"GGMSG_LOSEFOCUS"
								,"GGMSG_CLICKED"
								,"GGMSG_DOWN"
								,"GGMSG_UP"
								};
#define MESSAGESTRING(msg) (msg>=0 && msg<=11) ? messagestring[msg] : messagestring[0]

#define IN_CONTROL(c_,x_,y_)			\
	((c_)->x				<= x_ &&	\
	 (c_)->x+(c_)->xsize	>  x_ &&	\
	 (c_)->y				<= y_ &&	\
	 (c_)->y+(c_)->ysize	>  y_)

void fontprint(float x,float y,const char *s);

// global externs
extern float mouse_y;
extern float mouse_x;
extern Font *font;
extern ggFrame *focusedframe;
extern void (*InputCallbackFunc)(int,int,int,int,int);
