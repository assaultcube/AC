/*
 * Button control for the MESGUI library
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

#include "control_common.h"

/****************************
          ggButton
****************************/

ggButton::ggButton(const char *s,void (*f)()) : ggControl()
{
	numargs=0;
	*stringValP = s;
	func = f;
	icon=0;
}

ggButton::ggButton(const char *s,void (*f)(int,int,int,int,int),int arg1a,int arg2a,int arg3a,int arg4a,int arg5a) : ggControl()
{
	numargs=5;
	sarg1=arg1a;
	sarg2=arg2a;
	sarg3=arg3a;
	sarg4=arg4a;
	sarg5=arg5a;
	*stringValP = s;
	func = (void(*)()) f;
	icon=0;
}

void ggButton::Draw()
{
	BaseDraw();

	if (flags & GGFLAG_TEXTURED) {
		
		// find the y offset, in the texture
		int ty;
		if ((flags & GGFLAG_DOWN) && (flags & GGFLAG_ROLLOVER)) ty=2;
		else if ((flags & GGFLAG_MOUSEOVER) && (flags & GGFLAG_ROLLOVER)) ty=1;
		else ty=0;

		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D,texture);
		glColor4f(1,1,1,1);
		glBegin(GL_QUADS);
			glTexCoord2f(0,			tex_sizey*(ty+0)); glVertex2d(x      ,y      );
			glTexCoord2f(tex_sizex,	tex_sizey*(ty+0)); glVertex2d(x+xsize,y      );
			glTexCoord2f(tex_sizex,	tex_sizey*(ty+1)); glVertex2d(x+xsize,y+ysize);
			glTexCoord2f(0,			tex_sizey*(ty+1)); glVertex2d(x      ,y+ysize);
		glEnd();
		glDisable(GL_TEXTURE_2D);
	}

	glColor4f(1,1,1,a);
	if (flags & GGFLAG_ICON) {
		switch (icon)
		{
		case GGICON_CLOSE:
			glLineWidth(2);
			glBegin(GL_LINES);
			glVertex2f(x+xsize-6*(COL_HEIGHT/20.0f)-4*(COL_HEIGHT/20.0f),y+6*(COL_HEIGHT/20.0f)-6*(COL_HEIGHT/20.0f));
			glVertex2f(x+xsize-6*(COL_HEIGHT/20.0f)+5*(COL_HEIGHT/20.0f),y+6*(COL_HEIGHT/20.0f)+5*(COL_HEIGHT/20.0f));
			glVertex2f(x+xsize-6*(COL_HEIGHT/20.0f)+5*(COL_HEIGHT/20.0f),y+6*(COL_HEIGHT/20.0f)-6*(COL_HEIGHT/20.0f));
			glVertex2f(x+xsize-6*(COL_HEIGHT/20.0f)-4*(COL_HEIGHT/20.0f),y+6*(COL_HEIGHT/20.0f)+5*(COL_HEIGHT/20.0f));
			glEnd();
			glLineWidth(1);
			break;
		}
	}

	fontprint(x+TO_X,y+TO_Y,stringValP->c_str());

/*	// draw the hotkey
	if (hotkey && !hotkey_extended) {
		char s[2]={0,0};
		s[0]=hotkey;
		fontprint(x+xsize-TO_X-7,y+TO_Y,s);
	}*/
}

void ggButton::Message(int msg,int arg1,int arg2,int arg3,int arg4)
{
	BaseMessage(msg,arg1,arg2,arg3,arg4);

	switch (msg)
	{
	case GGMSG_DOWN:
//		glutPostRedisplay();
		break;
	case GGMSG_UP:
		if (InControl(arg3,arg4)) {
			caller = this;
			CallFunc();
		}
//		glutPostRedisplay();
		break;
	case GGMSG_MOUSEMOTION:
		break;
	}
}

void ggButton::SetHotkey(int hotkeya,int hotkey_extendeda)
{
	hotkey = hotkeya;
	if (!hotkey_extendeda) hotkey = shiftize(hotkey);
	hotkey_extended = hotkey_extendeda;
	if (hotkey) flags |= GGFLAG_HASHOTKEY;
}

void ggButton::SetTexture(unsigned int tex,int tex_sizexa,int tex_sizeya, int isrollover, int imagesize)
{
	texture = tex;
	flags |= GGFLAG_TEXTURED;
	flags |= GGFLAG_DONT_DRAW_BORDER;
	if (isrollover)	flags |= GGFLAG_ROLLOVER;
	tex_sizex = tex_sizexa/(float)imagesize;
	tex_sizey = tex_sizeya/(float)imagesize;
}

void ggButton::CallFunc()
{
	if (!func) return;
	switch (numargs) {
	case 0:
		func();
		break;

	case 5:
		( (void(*)(int,int,int,int,int)) func ) (sarg1,sarg2,sarg3,sarg4,sarg5);
		break;
	}
}
