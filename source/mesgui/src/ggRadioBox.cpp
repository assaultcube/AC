/*
 * RadioBox control for the MESGUI library
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
          ggRadioBox
****************************/

ggRadioBox::ggRadioBox(const char *s,void (*f)(bool checked)) : ggControl()
{
	*stringValP = s;
	func = f;
	flags |= GGFLAG_DONT_DRAW_BORDER;
}

void ggRadioBox::Draw()
{
	BaseDraw();

	int ccx=x+10;	// check center
	int ccy=y+COL_HEIGHT/2;
	glColor4f(1,1,1,a);
	glLineWidth(2);
	glBegin(GL_LINES);

	// left side
	glVertex2d(ccx-2-2,ccy-7);
	glVertex2d(ccx-2-4,ccy-5);
	glVertex2d(ccx-2-4,ccy-5);
	glVertex2d(ccx-2-5,ccy-2);
	glVertex2d(ccx-2-5,ccy-2);
	glVertex2d(ccx-2-5,ccy+2);
	glVertex2d(ccx-2-5,ccy+2);
	glVertex2d(ccx-2-4,ccy+5);
	glVertex2d(ccx-2-4,ccy+5);
	glVertex2d(ccx-2-2,ccy+7);
	// right side
	glVertex2d(ccx+2+2,ccy-7);
	glVertex2d(ccx+2+4,ccy-5);
	glVertex2d(ccx+2+4,ccy-5);
	glVertex2d(ccx+2+5,ccy-2);
	glVertex2d(ccx+2+5,ccy-2);
	glVertex2d(ccx+2+5,ccy+2);
	glVertex2d(ccx+2+5,ccy+2);
	glVertex2d(ccx+2+4,ccy+5);
	glVertex2d(ccx+2+4,ccy+5);
	glVertex2d(ccx+2+2,ccy+7);
	// right side
/*	glVertex2d(ccx+2+2,ccy-7);
	glVertex2d(ccx+2+4,ccy-5);
	glVertex2d(ccx+2+4,ccy-5);
	glVertex2d(ccx+2+5,ccy-1);
	glVertex2d(ccx+2+6,ccy-1);
	glVertex2d(ccx+2+6,ccy+1);
	glVertex2d(ccx+2+5,ccy+1);
	glVertex2d(ccx+2+4,ccy+5);
	glVertex2d(ccx+2+4,ccy+5);
	glVertex2d(ccx+2+2,ccy+7);
*/	
	glEnd();	
	glLineWidth(1);

	if (flags & GGFLAG_DOWN) {
		// draw it grey, if it's being pressed
		glColor4f(0.5f,0.5f,0.5f,a);
		glBegin(GL_POLYGON);
		glVertex2d(ccx-0,ccy-3);
		glVertex2d(ccx+2,ccy-2);
		glVertex2d(ccx+3,ccy-0);
		glVertex2d(ccx+2,ccy+2);
		glVertex2d(ccx+0,ccy+3);
		glVertex2d(ccx-2,ccy+2);
		glVertex2d(ccx-3,ccy+0);
		glVertex2d(ccx-2,ccy-2);
		glEnd();
	} else if (flags & GGFLAG_CHECKED) {
		// otherwise, draw it white, only if it's checked
		glBegin(GL_POLYGON);
		glVertex2d(ccx-0,ccy-3);
		glVertex2d(ccx+2,ccy-2);
		glVertex2d(ccx+3,ccy-0);
		glVertex2d(ccx+2,ccy+2);
		glVertex2d(ccx+0,ccy+3);
		glVertex2d(ccx-2,ccy+2);
		glVertex2d(ccx-3,ccy+0);
		glVertex2d(ccx-2,ccy-2);
		glEnd();
	}
	
	glColor4f(1,1,1,a);
	fontprint(x+20+TO_X,y+TO_Y,stringValP->c_str());
}

void ggRadioBox::Message(int msg,int arg1,int arg2,int arg3,int arg4)
{
	BaseMessage(msg,arg1,arg2,arg3,arg4);

	switch (msg)
	{
	case GGMSG_DOWN:
//		glutPostRedisplay();
		break;
	case GGMSG_UP:
		if (InControl(arg3,arg4)) {
			SetCheck();
			if (func) func(true);
		}
//		glutPostRedisplay();
		break;
	}
}

void ggRadioBox::Attach(ggRadioBox *radiobox)
{
	// attach everything together (yeah, there are duplicates, so what?)
	for (int i=0; i<attached.size(); i++) {
		((ggRadioBox*)attached[i])->attached.push_back(radiobox);
		radiobox->attached.push_back(attached[i]);
	}

	radiobox->attached.push_back(this);
	attached.push_back(radiobox);
}

void ggRadioBox::SetCheck()
{
	for (int i=0; i<attached.size(); i++)
		if (attached[i]->flags & GGFLAG_CHECKED) {
			attached[i]->flags &= ~GGFLAG_CHECKED;
			if (((ggRadioBox*)attached[i])->func) ((ggRadioBox*)attached[i])->func(false);
		}

	flags |= GGFLAG_CHECKED;
}
