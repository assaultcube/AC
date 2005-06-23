/*
 * CheckBox control for the MESGUI library
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
          ggCheckBox
****************************/

ggCheckBox::ggCheckBox(const char *s,void (*f)(bool checked)) : ggControl()
{
	*stringValP = s;
	func = f;
	flags |= GGFLAG_DONT_DRAW_BORDER;
}

void ggCheckBox::Draw()
{
	BaseDraw();

	glColor4f(1,1,1,a);
	int ccx=x+10;	// check center
	int ccy=y+COL_HEIGHT/2;

	glColor4f(1,0,0,0.3f);
	glBegin(GL_QUADS);
	glVertex2d(ccx-5,ccy-5);
	glVertex2d(ccx+5,ccy-5);
	glVertex2d(ccx+5,ccy+5);
	glVertex2d(ccx-5,ccy+5);
	glEnd();
	glColor4f(1,1,1,1);

	glBegin(GL_LINES);
	glVertex2d(ccx-5,ccy-5);
	glVertex2d(ccx+5,ccy-5);
	glVertex2d(ccx+5,ccy-5);
	glVertex2d(ccx+5,ccy+5);
	glVertex2d(ccx+5,ccy+5);
	glVertex2d(ccx-5,ccy+5);
	glVertex2d(ccx-5,ccy+5);
	glVertex2d(ccx-5,ccy-5);
	if (flags & GGFLAG_DOWN) {
		// draw it grey, if it's being pressed
		glColor4f(0.92f,0.92f,0.92f,a);
		glVertex2d(ccx-5,ccy-5);
		glVertex2d(ccx+5,ccy+5);
		glVertex2d(ccx-5,ccy+5);
		glVertex2d(ccx+5,ccy-5);
	} else if (flags & GGFLAG_CHECKED) {
		// otherwise, draw it white, only if it's checked
		glVertex2d(ccx-5,ccy-5);
		glVertex2d(ccx+5,ccy+5);
		glVertex2d(ccx-5,ccy+5);
		glVertex2d(ccx+5,ccy-5);
	}
	glEnd();
	
	glColor4f(1,1,1,a);
	fontprint(x+20+TO_X,y+TO_Y,stringValP->c_str());
}

void ggCheckBox::Message(int msg,int arg1,int arg2,int arg3,int arg4)
{
	BaseMessage(msg,arg1,arg2,arg3,arg4);

	switch (msg)
	{
	case GGMSG_DOWN:
//		glutPostRedisplay();
		break;
	case GGMSG_UP:
		if (InControl(arg3,arg4)) {
			if (flags & GGFLAG_CHECKED) {
				// clicked off
				flags &= ~GGFLAG_CHECKED;
				if (func) func(false);
			} else {
				// clicked on
				flags |= GGFLAG_CHECKED;
				if (func) func(true);
			}
		}
//		glutPostRedisplay();
		break;
	}
}

void ggCheckBox::SetCheck(bool checked)
{
	if (flags & GGFLAG_CHECKED && !checked) {
		// clicked off
		flags &= ~GGFLAG_CHECKED;
//		glutPostRedisplay();
	}
	if (!(flags & GGFLAG_CHECKED) && checked) {
		// clicked on
		flags |= GGFLAG_CHECKED;
//		glutPostRedisplay();
	}
}

