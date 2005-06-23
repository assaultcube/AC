/*
 * Test program for the MESGUI library
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

#include <SDL.h>
#ifdef WIN32
#include <windows.h>
#endif
#include <GL/gl.h>
#include <stdlib.h>
#include "mesgui.h"
#include "misc.h"
#include "scancodes.h"
#include <string>
using namespace std;

int xres=800,yres=600,bpp=32;
bool fullscreen=false;

float progress=0,inc=0.001f;
float slide=0;
string text;

void CallbackFunc(int a,int b,int c,int d,int e)
{
	char s[64];
	sprintf(s,"CallbackFunc: %d %d %d %d %d\n",a,b,c,d,e);
	text += s;
	printf(s);
}

void CallbackFunc2(bool a)
{
	char s[64];
	sprintf(s,"CallbackFunc: %s\n",a?"true":"false");
	text += s;
	printf(s);
}

void CreateTestControls()
{
	ggFrame *f = new ggFrame("Drag me");
	mainframe->AddControl(f,10,10,400,370);

	int pos=0;
	
	ggMenu *m = new ggMenu("Menu",80,100);
	m->AddMenuOption("option 1",CallbackFunc,1,0,0,0,0);
	ggMenu *sub = m->AddSubMenu("SubMenu",90);
	sub->AddMenuOption("option 2",CallbackFunc,2,0,0,0,0);
	sub->AddMenuOption("option 3",CallbackFunc,3,0,0,0,0);
	f->AddControl(m,4,pos);

	ggMenu *m2 = new ggMenu("Attached Menu",160,150);
	m2->AddMenuOption("option 4",CallbackFunc,4,0,0,0,0);
	f->AddControl(m2,4+80,pos);
	m->Attach(m2);
	
	pos += COL_HEIGHT+4;

	f->AddControl(new ggButton("Click Me!",CallbackFunc,0,0,0,0,0) ,4,pos,120);
	pos += COL_HEIGHT+4;

	f->AddControl(new ggText("type here"),4,pos,120);

	ggText *t = new ggText("or type here");
	t->RemoveFlags(GGFLAG_DONT_DRAW_BORDER);
	f->AddControl(t,200,pos,120);
	pos += COL_HEIGHT+4;

	ggDDMenu *dm = new ggDDMenu("DDMenu",90,90);
	dm->AddMenuOption("stuff",NULL);
	dm->AddMenuOption("thing",NULL);
	f->AddControl(dm,4,pos);
	pos += COL_HEIGHT+4;

	f->AddControl(new ggCheckBox("CheckBox",CallbackFunc2),4,pos,120);
	pos += COL_HEIGHT+4;

	ggRadioBox *rb1 = new ggRadioBox("RadioBox 1",CallbackFunc2);
	ggRadioBox *rb2 = new ggRadioBox("RadioBox 2",CallbackFunc2);
	rb1->Attach(rb2);
	f->AddControl(rb1,4,pos,140);
	f->AddControl(rb2,4+140+4,pos,140);
	pos += COL_HEIGHT+4;

	ggProgressBar *p = new ggProgressBar("ProgressBar");
	p->SetProgressPointer(&progress);
	f->AddControl(p,4,pos,150);
	pos += COL_HEIGHT+4;

	f->AddControl(new ggSpinner(&slide),4,pos,80);
	pos += COL_HEIGHT+4;

	static float vals[100];
	for (int i=0; i<100; i++) vals[i]=(i/100.0f)*(i/100.0f);
	f->AddControl(new ggTable(vals,100,0,1),4,pos,100);
	pos += COL_HEIGHT*2+4;

	ggText *t2 = new ggText("Controls can be any color\nand have any alpha value\nThis is a multiline text control\nTry drawing in the graph above");
	t2->SetFlags(GGFLAG_MULTILINE);
	t2->RemoveFlags(GGFLAG_DONT_DRAW_BORDER | GGFLAG_SIZETOCONTENT);
	t2->SetColor(0.2f,0.1f,0.4f);
	f->AddControl(t2,4,pos,400-4-4,370-pos-4-COL_HEIGHT);
	pos += COL_HEIGHT*5+4;


	ggFrame *f2 = new ggFrame("Another Window");
	mainframe->AddControl(f2,430,80,320,490);
	f2->SetFrameColor(0,0,0,0);
	f2->AddCloseButton();

	ggText *t3 = new ggText();
	t3->AttachString(&text);
	t3->SetFlags(GGFLAG_MULTILINE);
	t3->RemoveFlags(GGFLAG_DONT_DRAW_BORDER | GGFLAG_SIZETOCONTENT | GGFLAG_FOCUSABLE);
	f2->AddControl(t3,10,10,320-10-10,490-10-10-COL_HEIGHT);
}

int main(int argc, char *argv[])
{
	if ( SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO) < 0 ) {
		printf("Unable to init SDL: %s\n", SDL_GetError());
		exit(1);
	}
	SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );
	SDL_Surface *screen = SDL_SetVideoMode(
		xres,yres,fullscreen?bpp:0,
		SDL_HWSURFACE | (fullscreen?SDL_FULLSCREEN:0) | SDL_OPENGL);
	if (!screen) {
		printf("Unable to set video mode to %dx%dx%d%s: %s\n",xres,yres,bpp,
			fullscreen?" fullscreen":"",
			SDL_GetError());
		exit(1);
	}
	glViewport(0,0,xres,yres);

	InitMESGUI(&xres,&yres,"Arial.ttf",7,15);
//	InitMESGUI(&xres,&yres,"Impact.ttf",12,20);
//	SetCursorTexture("Images/cursor.bmp");
	CreateTestControls();

	SDL_Event event;
	bool quitapp=false;
	while (!quitapp) {
		while (SDL_PollEvent(&event)) {
			switch(event.type)
			{
			case SDL_QUIT:
				quitapp=true;
				break;

			case SDL_ACTIVEEVENT:
//				active=event.active.gain?true:false;
				break;
			
			case SDL_VIDEORESIZE:
				xres = event.resize.w;
				yres = event.resize.h;
				if (yres==0) yres=1;
				glViewport(0,0,xres,yres);
				break;
			
			case SDL_KEYDOWN:
			case SDL_KEYUP:
				if (event.key.keysym.sym < 128) {
					char c = event.key.keysym.sym;
					if (event.key.keysym.mod & KMOD_LSHIFT || event.key.keysym.mod & KMOD_RSHIFT)
						c = shiftize(c);
					GG_KeyboardEvent(c, event.key.state);
				} else {
					switch (event.key.keysym.sym) {
					case SDLK_RIGHT:
						GG_SKeyboardEvent(SC_RIGHT, event.key.state);
						break;
					case SDLK_LEFT:
						GG_SKeyboardEvent(SC_LEFT, event.key.state);
						break;
					}
				}
				break;
			
			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP:
				GG_MouseButton(event.button.button,event.button.state);
				break;
			}
		}

		// update the progress bar
		progress += inc;
		if (progress>1) inc=-0.001f;
		if (progress<0) inc=0.001f;

		// tell MESGUI the mouse position
		int tx,ty;
		SDL_GetMouseState(&tx, &ty);
		GG_SetMouseState(tx,ty);
		
		glClear(GL_COLOR_BUFFER_BIT);
		DrawMESGUI();
		SDL_GL_SwapBuffers();
	}

	SDL_Quit();
	return 0;
}
