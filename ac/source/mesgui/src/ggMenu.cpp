/*
 * Menu control for the MESGUI library
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
          ggMenu
****************************/

ggMenu::ggMenu() : ggControl()
{
	nummenuitems=0;
	downcontrol=NULL;
	menuactive=0;
	frame.ysize=0; // increased as menu options are added
	frame.flags |= GGFLAG_DONT_DRAW_BORDER | GGFLAG_DONT_USE;
}

ggMenu::ggMenu(const char *s,int xs,int framexs) : ggControl()
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

void ggMenu::Draw()
{
	BaseDraw();

	glColor4f(1,1,1,a);
	fontprint(x+TO_X,y+TO_Y,stringValP->c_str());
	if (flags & GGFLAG_SUBMENU) {
		// draws a triangle facing right
		glBegin(GL_TRIANGLES);
		glVertex2d(x+xsize-12   ,y+COL_HEIGHT/2-5);
		glVertex2d(x+xsize-12   ,y+COL_HEIGHT/2+5);
		glVertex2d(x+xsize-12+10,y+COL_HEIGHT/2);
		glEnd();
	} else {
		// draws a triangle facing down
		glBegin(GL_TRIANGLES);
		glVertex2d(x+xsize-9-5,y+COL_HEIGHT/2-4);
		glVertex2d(x+xsize-9+5,y+COL_HEIGHT/2-4);
		glVertex2d(x+xsize-9  ,y+COL_HEIGHT/2+6);
		glEnd();
	}
}

void ggMenu::Message(int msg,int arg1,int arg2,int arg3,int arg4)
{
	int rx,ry;
	int i;
	BaseMessage(msg,arg1,arg2,arg3,arg4);

	switch (msg)
	{
	case GGMSG_DOWN:
		// add the pop-up menu
		parent->AddControl(&frame);
		Message(GGMSG_MOUSEMOTION,0,0,arg3,arg4);
		menuactive=1;
//		glutPostRedisplay();
		break;

	case GGMSG_UP:
		rx=arg3-frame.x; // mouse positions, relative to the frame
		ry=arg4-frame.y;
		
		// give all controls in the frame the message
		for (i=0; i<frame.controls.size(); i++)
			if (frame.controls[i]->flags & GGFLAG_DOWN)
				frame.controls[i]->Message(GGMSG_UP,0,0,rx,ry);

		parent->RemoveControl(&frame);
		menuactive=0;
		downcontrol=NULL;
//		glutPostRedisplay();
		break;

	case GGMSG_MOUSEMOTION:
		rx=arg3-frame.x; // mouse positions, relative to the frame
		ry=arg4-frame.y;
		
		for (i=0; i<frame.controls.size(); i++) {
			if (!(frame.controls[i]->flags & GGFLAG_DONT_USE)) {
				if (frame.controls[i]->InControl(rx,ry)) {
					if (!(frame.controls[i]->flags & GGFLAG_DOWN)) {
						frame.controls[i]->Message(GGMSG_DOWN,0,0,rx,ry);
						downcontrol = frame.controls[i];
					}
				} else {
					if (frame.controls[i]->flags & GGFLAG_DOWN) {
						frame.controls[i]->Message(GGMSG_UP,0,0,rx,ry);
						if (frame.controls[i] == downcontrol)
							downcontrol = NULL;
					}
				}
			}
		}

		if (downcontrol) downcontrol->Message(GGMSG_MOUSEMOTION,0,0,rx,ry);
		// check to see if mouse is on top of attached menus
		if (!downcontrol) {
			ggControl *found=NULL;
			for (int i=0; i<attached.size() && !found; i++)
				if (attached[i]->InControl(arg3,arg4))
					found = attached[i];
			if (found) {
				// make the found menu have mouse control, instead of this one
				parent->mousedowncontrol = found;
			}
		}
		break;
	}
}

void ggMenu::AddMenuOption(const char *s,void (*f)())
{
	ggButton *newc;
	newc = new ggButton(s,f);
	newc->SetPos(0,(nummenuitems++)*COL_HEIGHT,frame.xsize);
	frame.AddControl(newc);
	frame.ysize += COL_HEIGHT;
}

void ggMenu::AddMenuOption(const char *s,void (*f)(int,int,int,int,int),int arg1,int arg2,int arg3,int arg4,int arg5)
{
	ggButton *newc;
	newc = new ggButton(s,f,arg1,arg2,arg3,arg4,arg5);
	newc->SetPos(0,(nummenuitems++)*COL_HEIGHT,frame.xsize);
	frame.AddControl(newc);
	frame.ysize += COL_HEIGHT;
}

ggMenu *ggMenu::AddSubMenu(const char *s,int xs)
{
	ggMenu *newc;
	newc = new ggMenu(s,frame.xsize,xs);// use baseframe to make all the nested frames add to the same base frame (2-25-2002: what was i talking about?)
	newc->flags |= GGFLAG_SUBMENU;
	newc->SetPos(0,(nummenuitems++)*COL_HEIGHT,frame.xsize);
	newc->frame.SetPos(newc->x+newc->xsize,newc->y);
	frame.AddControl(newc);
	frame.ysize += COL_HEIGHT;

	return newc;
}

void ggMenu::Attach(ggMenu *menu)
{
	// attach everything together (yeah, there are duplicates, so what?)
	for (int i=0; i<attached.size(); i++) {
		((ggMenu*)attached[i])->attached.push_back(menu);
		menu->attached.push_back(attached[i]);
	}

	menu->attached.push_back(this);
	attached.push_back(menu);
}

// these override the ggControl functions to update the frame also.

bool ggMenu::InControl(int xp,int yp)
// overriden to include the pop-up menu frame, if the menu is activated
{
	if IN_CONTROL(this,xp,yp) return true;
	if (menuactive) {

		if (/*flags & GGFLAG_DOWN && */frame.InControl(xp,yp)) return true;

		for (int i=0; i<frame.controls.size(); i++)
			if (frame.controls[i]->InControl(xp-frame.x,yp-frame.y)) return true;
	}

	return false;
}

void ggMenu::SetPos(int newx,int newy,int newxsize,int newysize)
{
	if (newx!=-1)		x=newx;
	if (newy!=-1)		y=newy;
	if (newxsize!=-1)	xsize=newxsize;
	if (newysize!=-1)	ysize=newysize;
	if (flags & GGFLAG_SUBMENU) {
		frame.x = x+xsize;
		frame.y = y;
	} else {
		frame.x = x;
		frame.y = y+COL_HEIGHT;
	}
}
