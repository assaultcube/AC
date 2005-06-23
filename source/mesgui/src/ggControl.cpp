/*
 * Base control class for the MESGUI library
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
          ggControl
****************************/

ggControl::ggControl()
{
	x = 0;
	y = 0;
	xsize = 50;
	ysize = COL_HEIGHT;
	r=0.2f;
	g=0.7f;
	b=0.5f;
	a=0.7f;
	flags = 0;
	parent=NULL;
//	text = &textstring;
//	*text="";
	stringValP = &stringVal;
	intValP = &intVal;
	floatValP = &floatVal;
	valType=0;
}

int ggControl::BaseMessage(int msg,int arg1,int arg2,int arg3,int arg4)
{
/*	if (msg != GGMSG_MOUSEMOTION) { // don't print motion messsages.  too many.
		int spaces=(int)this % 41;
		for (int i=0; i<spaces; i++)
			printf(" ");
		printf("%d %s(%d) %d %d %d %d\n",(int)this,MESSAGESTRING(msg),msg,arg1,arg2,arg3,arg4);
	}
*/
	switch (msg)
	{
	case GGMSG_GAINFOCUS:
		if (flags & GGFLAG_FOCUSABLE) {
			flags |= GGFLAG_FOCUSED;
			if (parent) {
				parent->focus = this;
				parent->MoveToTop(this);
			}
//			glutPostRedisplay();
		}
		return 1;
	case GGMSG_LOSEFOCUS:
		if (flags & GGFLAG_FOCUSABLE) {
			flags &= ~GGFLAG_FOCUSED;
			if (parent) parent->focus = NULL;
//			glutPostRedisplay();
		}
		return 1;
	case GGMSG_DOWN:
		flags |= GGFLAG_DOWN;
		return 1;
	case GGMSG_UP:
		flags &= ~GGFLAG_DOWN;
		return 1;
	}
	return 0;
}

void ggControl::BaseDraw()
{
	if (!(flags & GGFLAG_DONT_DRAW_BORDER)) {
		float mul = (flags & GGFLAG_DOWN) ? 0.7f : 1.0f;
		glBegin(GL_QUADS);
		glColor4f(r*0.6f*mul,g*0.6f*mul,b*0.6f*mul,a);	glVertex2d(x      ,y	  );
		glColor4f(r*0.9f*mul,g*0.9f*mul,b*0.9f*mul,a);	glVertex2d(x+xsize,y	  );
		glColor4f(r*1.0f*mul,g*1.0f*mul,b*1.0f*mul,a);	glVertex2d(x+xsize,y+ysize);
		glColor4f(r*0.7f*mul,g*0.7f*mul,b*0.7f*mul,a);	glVertex2d(x      ,y+ysize);
		glEnd();

		glColor4f(1,1,1,a);
		glBegin(GL_LINES);
		glVertex2d(x      ,y      );
		glVertex2d(x+xsize,y      );
		glVertex2d(x+xsize,y      );
		glVertex2d(x+xsize,y+ysize);
		glVertex2d(x+xsize,y+ysize);
		glVertex2d(x      ,y+ysize);
		glVertex2d(x      ,y+ysize);
		glVertex2d(x      ,y-1    );
		glEnd();
	}
}

bool ggControl::InControl(int xp,int yp)
{
	return IN_CONTROL(this,xp,yp);
}

void ggControl::SetColor(float ra,float ga,float ba)
{
	r = ra;
	g = ga;
	b = ba;
//	glutPostRedisplay();
}

void ggControl::SetColor(float ra,float ga,float ba,float aa)
{
	r = ra;
	g = ga;
	b = ba;
	a = aa;
//	glutPostRedisplay();
}

void ggControl::SizeToContent()
{
	xsize = (int)font->getWidth(stringValP->c_str())+8;
}

void ggControl::SetPos(int newx,int newy,int newxsize,int newysize)
{
	if (newx!=-1)		x=newx;
	if (newy!=-1)		y=newy;
	if (newxsize!=-1)	xsize=newxsize;
	if (newysize!=-1)	ysize=newysize;
}

void ggControl::SetFlags(int f)
{
	flags |= f;
}

void ggControl::RemoveFlags(int f)
{
	flags &= ~f;
}

void ggControl::AttachString(string *v)
{
	stringValP = v;
	valType=1;
}

void ggControl::AttachInt(int *v)
{
	intValP = v;
	valType=2;
}

void ggControl::AttachFloat(float *v)
{
	floatValP = v;
	valType=3;
}
