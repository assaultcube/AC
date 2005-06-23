/*
 * Frame control for the MESGUI library
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
          ggFrame
****************************/

ggFrame::ggFrame() : ggControl()
{
	focus=NULL;
	flags |= GGFLAG_DONT_DRAW_TITLE | GGFLAG_FOCUSABLE | GGFLAG_FRAME;
	mousedowncontrol=NULL;
	dragstate=0;
	r=0.2f;
	g=0.7f;
	b=1.0f;
	a=0.4f;
	framer=1.0f;
	frameg=0.0f;
	frameb=0.0f;
	framea=0.7f;
	scale=1;
	offsetx=0;
	offsety=0;
	scrollbutton=NULL;
	passbackfunc=NULL;
	drawFunc=NULL;
}

ggFrame::ggFrame(const char *t) : ggControl()
{
	*stringValP = t;
	focus=NULL;
	flags |= GGFLAG_FOCUSABLE | GGFLAG_MOVABLE | GGFLAG_FRAME;
	mousedowncontrol=NULL;
	dragstate=0;
	r=0.2f;
	g=0.7f;
	b=1.0f;
	framer=1.0f;
	frameg=0.0f;
	frameb=0.0f;
	framea=0.5f;
	scale=1;
	offsetx=0;
	offsety=0;
	scrollbutton=NULL;
	passbackfunc=NULL;
	drawFunc=NULL;
}

ggFrame::~ggFrame()
{
	for (int i=0; i<controls.size(); i++)
		delete controls[i];
}

void ggFrame::Draw()
{
	if (!(flags & GGFLAG_DONT_DRAW_BORDER)) {
		if (!(flags & GGFLAG_DONT_DRAW_TITLE)) {
			glBegin(GL_QUADS);
			glColor4f(framer,frameg,frameb,framea);
			glVertex2d(x      ,y+COL_HEIGHT);
			glVertex2d(x+xsize,y+COL_HEIGHT);
			glVertex2d(x+xsize,y+ysize);
			glVertex2d(x      ,y+ysize);
			float mul = (flags & GGFLAG_FOCUSED) ? 0.7f : 1.0f;
			glColor4f(r*0.6f*mul,g*0.6f*mul,b*0.6f*mul,a);	glVertex2d(x      ,y);
			glColor4f(r*0.9f*mul,g*0.9f*mul,b*0.9f*mul,a);	glVertex2d(x+xsize,y);
			glColor4f(r*1.0f*mul,g*1.0f*mul,b*1.0f*mul,a);	glVertex2d(x+xsize,y+COL_HEIGHT);
			glColor4f(r*0.7f*mul,g*0.7f*mul,b*0.7f*mul,a);	glVertex2d(x      ,y+COL_HEIGHT);
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
			glVertex2d(x      ,y+COL_HEIGHT);
			glVertex2d(x+xsize,y+COL_HEIGHT);
			glEnd();
			fontprint(x+TO_X,y+TO_Y,stringValP->c_str());
		} else {
			glBegin(GL_QUADS);
			glColor4f(framer,frameg,frameb,framea);
			glVertex2d(x      ,y);
			glVertex2d(x+xsize,y);
			glVertex2d(x+xsize,y+ysize);
			glVertex2d(x      ,y+ysize);
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
	glPushMatrix();
	if (!(flags & GGFLAG_DONT_DRAW_TITLE)) {
		// add a COL_HEIGHT if there's a title
		glTranslated(x+offsetx,y+offsety+COL_HEIGHT,0);
	} else {
		glTranslated(x+offsetx,y+offsety,0);
	}
	if (scale!=1)
		glScalef(scale,scale,1);

#define GRIDSIZE 10
	if (flags & GGFLAG_DRAWGRID) {
		int xl,yl,xs,ys;
		xs = xsize / GRIDSIZE;
		ys = ysize / GRIDSIZE;
		glColor4f(0.3f,0.3f,0.3f,1.0f);
		glBegin(GL_LINES);
		for (xl=0; xl<xs; xl++) {
			glVertex2d(x+xl*GRIDSIZE,y);
			glVertex2d(x+xl*GRIDSIZE,y+ysize);
		}
		for (yl=0; yl<ys; yl++) {
			glVertex2d(x,      y+yl*GRIDSIZE);
			glVertex2d(x+xsize,y+yl*GRIDSIZE);
		}
		glEnd();
	}

	for (int i=0; i<controls.size(); i++)
		controls[i]->Draw();

	if (drawFunc) drawFunc();

	glPopMatrix();
}

void ggFrame::Message(int msg,int arg1,int arg2,int arg3,int arg4)
{
	// must take care of these, even if BaseMessage does too.
	if (msg==GGMSG_GAINFOCUS) focusedframe = this;
	if (msg==GGMSG_LOSEFOCUS) {
		if (focus) focus->Message(GGMSG_LOSEFOCUS,0,0,0,0);	// tell the focused control in here, to lose focus also
		if (parent) focusedframe = parent;
	}

	BaseMessage(msg,arg1,arg2,arg3,arg4);

	switch (msg)
	{
	case GGMSG_KEYDOWN:
	case GGMSG_KEYUP:
	case GGMSG_SKEYDOWN:
	case GGMSG_SKEYUP:
		if (focus) focus->Message(msg,arg1,arg2,arg3,arg4);
		return;
	case GGMSG_MOUSEBUTTON:
		// callback stuff (PassBackNextMouseClick) only if not on a control while pressing down
		if (flags & GGFLAG_PASSBACK && (arg2==STATE_RELEASED || !FindControl(arg3,arg4))) {
			flags &= ~GGFLAG_PASSBACK;
			if (passbackfunc) passbackfunc(arg3,arg4,arg1,arg2);
			if (arg2!=STATE_RELEASED) return;
		}

		if (arg1 != LEFT_BUTTON) return; // I only want LMB clicks for now.

		arg3 -= x;	// set it to be relative to the frame
		arg4 -= y;
		if (!(flags & GGFLAG_DONT_DRAW_BORDER)) arg4 -= COL_HEIGHT; // add a COL_HEIGHT, if there's a title.

		if (arg2==STATE_RELEASED) {
			if (mousedowncontrol) {
				if (dragstate==1) {
        			mousedowncontrol->Message(GGMSG_CLICKED,0,0,arg3,arg4);
					if (focus) focus->Message(GGMSG_LOSEFOCUS,0,0,0,0);
					mousedowncontrol->Message(GGMSG_GAINFOCUS,0,0,0,0);
				} else if (dragstate==0) {
					mousedowncontrol->Message(msg,arg1,arg2,arg3,arg4);
				}
				mousedowncontrol->Message(GGMSG_UP,0,0,arg3,arg4);
				mousedowncontrol=NULL;
			} else {
				// pass message to a callback func that API-user specifies
				if (this == mainframe && InputCallbackFunc) InputCallbackFunc(msg,arg1,arg2,arg3,arg4);
				// selection box (maybe later)
			}
			dragstate=0;
			return;
		} else { // STATE_PRESSED
			ggControl *mouseover = FindControl(arg3,arg4);
			if (mouseover) {
				mousedowncontrol = mouseover;
				mousedowncontrol->Message(GGMSG_DOWN,0,0,arg3,arg4);

				// only drag a frame, if the mouse isn't on a control
				bool on_control=false;
				if (mouseover->flags&GGFLAG_FRAME) {
					float rel_x = arg3-mouseover->x;
					float rel_y = arg4-mouseover->y;
					if (!(mouseover->flags & GGFLAG_DONT_DRAW_BORDER)) rel_y -= COL_HEIGHT; // add a COL_HEIGHT, if there's a title.
					on_control = !((ggFrame*)mouseover)->FindControl(rel_x,rel_y)?true:false;
				}

				// new code
/*
				//not on a control AND we are over a focused frame.. 
				if(!on_control && mouseover == focus) 
				{ 
					if (arg1 == LEFT_BUTTON)     //begin frame-drag.. 
					{ 
						dragstate=1; 
						dragx = arg3; 
						dragy = arg4; 
					} 
				} 
				
				//not on a control or we are over the frame with focus. 
				if(!on_control || mouseover == focus)
				{ 
					if(mouseover != focus) 
					{ 
						if(focus) //unfocus whatever has focus 
							focus->Message(GGMSG_LOSEFOCUS,0,0,0,0); 
						
						//give focus to whatever is under the mouse 
						mouseover->Message(GGMSG_GAINFOCUS,0,0,0,0); 

						//begin frame-drag for when the frame is already selected
						if (arg1 == LEFT_BUTTON)
						{ 
							dragstate=1;
							dragx = arg3; 
							dragy = arg4; 
						}
					} 
					//forward the message to whatever was under the mouse 
					mouseover->Message(msg,arg1,arg2,arg3,arg4);
				} 
*/
				if (!on_control || mouseover == focus) { // give it the message, if it's focused
					if (focus) focus->Message(GGMSG_LOSEFOCUS,0,0,0,0);
					mouseover->Message(GGMSG_GAINFOCUS,0,0,0,0);
					mouseover->Message(msg,arg1,arg2,arg3,arg4);
				} else {
					if (arg1 == LEFT_BUTTON) {
						dragstate=1;
						dragx = arg3;
						dragy = arg4;
					}
				}

			} else {
				if (focus) focus->Message(GGMSG_LOSEFOCUS,0,0,0,0);
				// pass message to a callback func that programmer specifies
				if (this == mainframe && InputCallbackFunc) InputCallbackFunc(msg,arg1,arg2,arg3,arg4);
				// selection box (maybe later)
			}
			
		}
		break;

	case GGMSG_MOUSEMOTION:
		arg3 -= x;	// set it to be relative to the frame
		arg4 -= y;
		if (!(flags & GGFLAG_DONT_DRAW_BORDER)) arg4 -= COL_HEIGHT; // add a COL_HEIGHT, if there's a title.

		if (!mousedowncontrol) {
			// mouseover events
			if (this == mainframe) {
				static ggControl *mouseover=NULL;
				if (mouseover) mouseover->flags &= ~GGFLAG_MOUSEOVER;
				mouseover=FindControl(arg3,arg4);
				if (mouseover) mouseover->flags |= GGFLAG_MOUSEOVER;
			}
			
			// pass message to a callback func that programmer specifies
			if (this == mainframe && InputCallbackFunc) InputCallbackFunc(msg,arg1,arg2,arg3,arg4);
			break;
		}
		int dragxrel = arg3-dragx;
		int dragyrel = arg4-dragy;
		if (dragstate==1) {
			if (abs(dragxrel)+abs(dragyrel) >= DRAG_THRESHOLD)
				dragstate=2;
		} else if (dragstate==2) {
			if (mousedowncontrol->flags & GGFLAG_MOVABLE) {
				mousedowncontrol->x += dragxrel;
				mousedowncontrol->y += dragyrel;
				dragx = arg3;
				dragy = arg4;
//				glutPostRedisplay();
			}
		}
		if (!(mousedowncontrol->flags & GGFLAG_MOUSEPRIORITY)) {
			if (mousedowncontrol->InControl(arg3,arg4)) {
				if (!(mousedowncontrol->flags & GGFLAG_DOWN))
					mousedowncontrol->Message(GGMSG_DOWN,0,0,arg3,arg4);
			} else {
				if (mousedowncontrol->flags & GGFLAG_DOWN)
					mousedowncontrol->Message(GGMSG_UP,0,0,arg3,arg4);
			}
		}

		// must also pass message.
		mousedowncontrol->Message(GGMSG_MOUSEMOTION,0,0,arg3,arg4);
		break;
	}
}

void ggFrame::AddControl(ggControl *newcontrol,int newx,int newy,int newxsize,int newysize)
{
	if (!newcontrol) return;

	newcontrol->SetPos(newx,newy,newxsize,newysize);
	newcontrol->parent = this;

	controls.push_back(newcontrol);

	// keep track of hotkey info
	if (newcontrol->flags & GGFLAG_HASHOTKEY && this==mainframe) {
		ggButton *b = (ggButton*)newcontrol;
		if (((ggButton*)newcontrol)->hotkey_extended) {
			hotkeys_extended[b->hotkey] = b;
		} else {
			hotkeys[shiftize(b->hotkey)] = b;
		}
	}

//	glutPostRedisplay();
}

void ggFrame::RemoveControl(ggControl *c)
// doesn't de-allocate the ggControl
{
	if (!c) return;

	if (c==mousedowncontrol) mousedowncontrol=NULL;
	if (c==focus) focus=NULL;

	vector<ggControl*>::iterator it = find(controls.begin(),controls.end(),c);
	if (it!=controls.end()) controls.erase(it);

	// keep track of hotkey info
	if (c->flags & GGFLAG_HASHOTKEY && this==mainframe) {
		ggButton *b = (ggButton*)c;
		if (((ggButton*)c)->hotkey_extended) {
			hotkeys_extended[b->hotkey] = NULL;
		} else {
			hotkeys[b->hotkey] = NULL;
		}
	}

//	glutPostRedisplay();
}

void ggFrame::DeleteControl(ggControl *c)
// de-allocates itself and all stuff nested within
{
	if (!c) return;

	RemoveControl(c);
	delete c;
//	glutPostRedisplay();
}

ggControl *ggFrame::FindControl(int xpos,int ypos) 
{ 
     ggControl *ret; 
 
     ret = NULL; 
 
     for (int i=0; i<controls.size(); i++) 
          if (controls[i]->InControl(xpos,ypos)) 
               ret = controls[i]; 
 
     return ret; 
} 
/*
ggControl *ggFrame::FindControl(int xpos,int ypos)
{
	for (int i=0; i<controls.size(); i++)
		if (controls[i]->InControl(xpos,ypos)) return controls[i];
	return NULL;
}
*/
void ggFrame::MoveToTop(ggControl *c)
{
	// RemoveControl messes up mousedowncontrol and focus
	bool re_set_mousedowncontrol = (c==mousedowncontrol);
	bool re_set_focus = (c==focus);
	RemoveControl(c);
	AddControl(c);
	if (re_set_mousedowncontrol) mousedowncontrol = c;
	if (re_set_focus) focus = c;
}

void ggFrame::SetScale(float newscale)
{
	scale = newscale;
//	glutPostRedisplay();
}

void ggFrame::SetOffset(int ox,int oy)
{
	offsetx = ox;
	offsety = oy;
//	glutPostRedisplay();
}

void ggFrame::PassBackNextMouseClick(void (*f)(int x,int y,int b,int state))
{
	if (!f) {
		flags &= ~GGFLAG_PASSBACK;
	} else {
		flags |= GGFLAG_PASSBACK;
	}
	passbackfunc = f;
}

void OnClose(int arg1,int arg2,int arg3,int arg4,int arg5)
{
//	((ggFrame*)arg1)->flags |= GGFLAG_DELETEME;
	control_to_delete = (ggControl*)arg1;
}

void ggFrame::AddCloseButton()
{
	ggButton *button = new ggButton("",OnClose,(int)this,0,0,0,0);
	button->flags |= GGFLAG_ICON;
	button->icon = GGICON_CLOSE;
	button->SetPos(xsize-COL_HEIGHT+3,-COL_HEIGHT+3,COL_HEIGHT-6,COL_HEIGHT-6);
	AddControl(button);
}

void ggFrame::SetDrawFunc(void (*f)())
{
	drawFunc = f;
}

void ggFrame::SetFrameColor(float ra,float ga,float ba)
{
	framer = ra;
	frameg = ga;
	frameb = ba;
//	glutPostRedisplay();
}

void ggFrame::SetFrameColor(float ra,float ga,float ba,float aa)
{
	framer = ra;
	frameg = ga;
	frameb = ba;
	framea = aa;
//	glutPostRedisplay();
}
