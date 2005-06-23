/*
 * Main functions for the MESGUI library
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

int width,height;
ggFrame *mainframe=NULL;
ggFrame *focusedframe=NULL;
void (*AdditionalDrawFunc)()=NULL;
ggControl *caller=NULL; // the control that is calling the callback function
Font *font=NULL;
float mouse_y=0;
float mouse_x=0;
int *p_xres=NULL;
int *p_yres=NULL;
#define xres (*p_xres)
#define yres (*p_yres)
unsigned int *cursor_texture=NULL;
void (*InputCallbackFunc)(int,int,int,int,int)=NULL;
ggButton *hotkeys[256],*hotkeys_extended[256];
void (*passbackfunc)(int x,int y,int b,int state) = NULL;// set in PassBackNextMouseClick
ggControl *control_to_delete=NULL;
int COL_HEIGHT=20;

ggText statusbar("Ready");

void fontprint(float x,float y,const char *s)
{
	glEnable(GL_TEXTURE_2D);
	font->output(x,y,s);
	glDisable(GL_TEXTURE_2D);
}

void GG_SetMouseState(float x, float y)
{
	// i'm making these x,y values relative to last call, which is not what glut does.
	mouse_x = x;
	mouse_y = y;
	if (mouse_x < 0) mouse_x = 0;
	if (mouse_y < 0) mouse_y = 0;
	if (mouse_x >= xres) mouse_x = xres-1;
	if (mouse_y >= yres) mouse_y = yres-1;
	mainframe->Message(GGMSG_MOUSEMOTION,0,0,mouse_x,mouse_y);
}

void GG_MouseButton(int button, int state)
{
	if (passbackfunc && state==STATE_PRESSED) {
		void (*temp)(int x,int y,int b,int state) = passbackfunc;
		passbackfunc=NULL;
		temp((int)mouse_x,(int)mouse_y,button,state);
	}

	mainframe->Message(GGMSG_MOUSEBUTTON,button,state,(int)mouse_x,(int)mouse_y);
}

void DrawMESGUI()
{
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glOrtho(0.0f, xres, yres, 0.0f, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	
	if (control_to_delete) {
		control_to_delete->parent->DeleteControl(control_to_delete);
		control_to_delete=NULL;
	}

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_TEXTURE_2D);

	mainframe->Draw();
	if (AdditionalDrawFunc) AdditionalDrawFunc();

	// draw the cursor
	if (cursor_texture) {
		glColor4f(1,1,1,1);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, *cursor_texture);
		glBegin(GL_QUADS);
		glTexCoord2d(0,0);	glVertex2d(mouse_x-16+0, mouse_y-16+0);
		glTexCoord2d(1,0);	glVertex2d(mouse_x-16+31,mouse_y-16+0);
		glTexCoord2d(1,1);	glVertex2d(mouse_x-16+31,mouse_y-16+31);
		glTexCoord2d(0,1);	glVertex2d(mouse_x-16+0, mouse_y-16+31);
		glEnd();
		glDisable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
	}

	// restore the matrices
	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
}

void GG_KeyboardEvent(char ch, int state)
{
	if (state==STATE_PRESSED) {
		mainframe->Message(GGMSG_KEYDOWN,ch,0,0,0);
	} else if (state==STATE_RELEASED) {
		mainframe->Message(GGMSG_KEYUP,ch,0,0,0);
	}
}

void GG_SKeyboardEvent(int key, int state)
{
	if (state==STATE_PRESSED) {
		mainframe->Message(GGMSG_SKEYDOWN,key,0,0,0);
	} else if (state==STATE_RELEASED) {
		mainframe->Message(GGMSG_SKEYUP,key,0,0,0);
	}
}

void ggCreateMainFrame(int w,int h)
{
	mainframe = new ggFrame();
	mainframe->SetPos(0,0,width,height);
	mainframe->SetFlags(GGFLAG_DONT_DRAW_BORDER);
	// statusbar has a border, is not modifiable, and doesn't resize to content
	focusedframe = mainframe;
}

void InitMESGUI(int *p_xresa,int *p_yresa,const char *fontFilename,int fontsize,int colheight)
{
	p_xres=p_xresa;
	p_yres=p_yresa;

	ggCreateMainFrame(xres,yres);
	font = new Font(fontFilename,fontsize);

	COL_HEIGHT = colheight;

	for (int i=0; i<256; i++) {
		hotkeys[i]=NULL;
		hotkeys_extended[i]=NULL;
	}
}

void SetCursorTexture(string filename) {
	cursor_texture = LoadTexture(filename.c_str());
}

void SetFont(string filename) {
	if (font) delete font;
	font = new Font(filename.c_str(),12);
}

void SetAdditionalDrawFunc(void (*func)())
{
	AdditionalDrawFunc = func;
}

void SetInputCallbackFunc(void (*func)(int,int,int,int,int))
{
	InputCallbackFunc = func;
}

void PassBackNextMouseClick(void (*f)(int x,int y,int b,int state))
{
	passbackfunc = f;
}
