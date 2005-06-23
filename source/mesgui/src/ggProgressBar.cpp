/*
 * Progress bar control for the MESGUI library
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
          ggProgressBar
****************************/

ggProgressBar::ggProgressBar(const char *s) : ggControl()
{
	*stringValP = s;
	flags |= GGFLAG_DONT_DRAW_BORDER;

	progress = &progress_data;
	*progress=0;
	progress_max = &progress_max_data;
	*progress_max = 1;
}

void ggProgressBar::Draw()
{
	BaseDraw();

	glBegin(GL_QUADS);
	float prog = *progress / *progress_max;
	glColor4f(r,g,b,a);
	glVertex2d(x	       ,y      );
	glVertex2d(x+xsize*prog,y      );
	glVertex2d(x+xsize*prog,y+ysize);
	glVertex2d(x           ,y+ysize);

	glColor4f(0,0,0,a);
	glVertex2d(x+xsize*prog,y      );
	glVertex2d(x+xsize     ,y      );
	glVertex2d(x+xsize     ,y+ysize);
	glVertex2d(x+xsize*prog,y+ysize);
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

	float textpos = xsize/2 - font->getWidth(stringValP->c_str())/2;
	fontprint(x+textpos+TO_X,y+TO_Y,(*stringValP).c_str());
}

void ggProgressBar::Message(int msg,int arg1,int arg2,int arg3,int arg4)
{
	BaseMessage(msg,arg1,arg2,arg3,arg4);
}

void ggProgressBar::SetProgress(float p)
{
	*progress = p;
//	glutPostRedisplay();
}

void ggProgressBar::SetProgressPointer(float *p)
{
	progress = p;
//	glutPostRedisplay();
}

void ggProgressBar::SetProgressMax(float p)
{
	*progress_max = p;
//	glutPostRedisplay();
}

void ggProgressBar::SetProgressMaxPointer(float *p)
{
	progress_max = p;
//	glutPostRedisplay();
}
