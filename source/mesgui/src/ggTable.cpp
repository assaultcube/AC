/*
 * Table control for the MESGUI library
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
          ggTable
****************************/

ggTable::ggTable(float *tablea,int elementsa,float mina,float maxa,int shifta) : ggControl()
{
	flags |= GGFLAG_DONT_DRAW_BORDER;
	table = tablea;
	elements = elementsa;
	min = mina;
	max = maxa;
	shift = shifta;
	xsize = elements+1;
	ysize = COL_HEIGHT*2;
}

void ggTable::Draw()
{
	BaseDraw();

	float val;
	glColor4f(1,1,1,a);
	glBegin(GL_LINES);
	for (int i=0; i<elements; i++) {
		val = (table[i*shift]-min)/(max-min)*ysize;
		if (val<0) val=0;
		if (val>ysize) val=ysize;
		glVertex2d(x+1+i,y+ysize-val);
		glVertex2d(x+1+i,y+ysize-1);
	}
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

void ggTable::Message(int msg,int arg1,int arg2,int arg3,int arg4)
{
	BaseMessage(msg,arg1,arg2,arg3,arg4);

	switch (msg)
	{
	case GGMSG_DOWN:
		prev_rel_x=mouse_x-x-parent->x;
		prev_rel_y=mouse_y-y-parent->y;
		if (!(parent->flags & GGFLAG_DONT_DRAW_TITLE)) {
			// subtract a COL_HEIGHT if there's a title
			prev_rel_y -= COL_HEIGHT;
		}
//		glutPostRedisplay();
		break;

	case GGMSG_UP:
//		glutPostRedisplay();
		break;

	case GGMSG_MOUSEMOTION:
		int rel_x = (int)mouse_x-x-parent->x;
		int rel_y = (int)mouse_y-y-parent->y;
		if (!(parent->flags & GGFLAG_DONT_DRAW_TITLE)) {
			// subtract a COL_HEIGHT if there's a title
			rel_y -= COL_HEIGHT;
		}
		int i = (int)prev_rel_x;
		bool done=false;
		do {
			if (i>=0 && i<elements) {
				float val1=(ysize-prev_rel_y)/(float)ysize*(max-min)+min;
				float val2=(ysize-rel_y)/(float)ysize*(max-min)+min;
				float w2=(i-prev_rel_x)/(rel_x-prev_rel_x);
				float w1=1-w2;
				float val=w1*val1+w2*val2;
				if (val<min) val=min;
				if (val>max) val=max;
				table[i*shift]=val;
			}
			if (prev_rel_x<rel_x) {
				i+=1;
				if (i>rel_x) done=true;
			} else{
				i+=-1;
				if (i<rel_x) done=true;
			}
		} while (!done);
		prev_rel_x=rel_x;
		prev_rel_y=rel_y;
//		glutPostRedisplay();
		break;
	}
}
