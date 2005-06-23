/*
 * Drop down menu control for the MESGUI library
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
          ggDDMenu
****************************/

ggDDMenu::ggDDMenu(const char *s,int xs,int framexs) : ggMenu()
{
	*stringValP = s;
	xsize = xs;
	frame.xsize=framexs;
	nummenuitems=0;
	downcontrol=NULL;
	menuactive=0;
	frame.ysize=0; // increased as menu options are added
	frame.flags |= GGFLAG_DONT_DRAW_BORDER | GGFLAG_DONT_USE;
}

void ggDDMenu::Message(int msg,int arg1,int arg2,int arg3,int arg4)
{
	switch (msg)
	{
	case GGMSG_UP:
		int rx=arg3-frame.x; // mouse positions, relative to the frame
		int ry=arg4-frame.y;
		// give all controls in the frame the message
		for (int i=0; i<frame.controls.size(); i++) {
			if (frame.controls[i]->flags & GGFLAG_DOWN)
				frame.controls[i]->Message(GGMSG_UP,0,0,rx,ry);
		}
		ggControl *clickedon = frame.FindControl(rx,ry);
		if (clickedon) *stringValP = *clickedon->stringValP;
		parent->RemoveControl(&frame);
		menuactive=0;
		downcontrol=NULL;
//		glutPostRedisplay();
		break;

	}
	ggMenu::Message(msg,arg1,arg2,arg3,arg4);
}
