// UDIinfo: 2d &xres,&yres

/*
 * Header file for the MESGUI library
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

#ifndef MESGUI_H
#define MESGUI_H

#include <string>
#include <vector>
#include <algorithm>
using namespace std;

//#define COL_HEIGHT		20	// column height
extern int COL_HEIGHT;

// ggControl flags
#define GGFLAG_DONT_DRAW_BORDER	0x00000001	// don't draw inside polygon and border
#define GGFLAG_DONT_DRAW_TITLE	0x00000002	// don't draw title (for ggFrames only)
#define GGFLAG_MOVABLE			0x00000004	// control can be dragged by the mouse
#define GGFLAG_SELECTABLE		0x00000008	// control can be selected
#define GGFLAG_SELECTED			0x00000010	// control is currently selected
#define GGFLAG_FOCUSABLE		0x00000020	// control can be focused
#define GGFLAG_FOCUSED			0x00000040	// control is currently focused
#define GGFLAG_DRAWGRID			0x00000080	// for ggFrames.  draws a grid
#define GGFLAG_DOWN				0x00000100	// for ggButtons and ggMenus.  indicates if the control is depressed
#define GGFLAG_DONT_USE			0x00000200	// for ggMenus.  used for the MOUSEMOTION event handler
#define GGFLAG_SUBMENU			0x00000400	// for ggMenus.  indicates a submenu, so triangle is drawn facing right, not down
#define GGFLAG_CHECKED			0x00000800	// for ggCheckBoxes and ggRadioBoxes
#define GGFLAG_ICON				0x00001000	// for ggText.  The text is drawn next to an icon, specified in the icon variable
#define GGFLAG_PASSBACK			0x00002000	// don't set manually.  callbacking for mouse clicks.  used in PassBackNextMouseClick.
#define GGFLAG_SIZETOCONTENT	0x00004000	// for ggText.  makes xsize the size of the text (updates whenever modified)
#define GGFLAG_HASHOTKEY		0x00008000	// control is a ggButton, and has a hotkey
#define GGFLAG_TEXTURED			0x00010000	// is texturize
#define GGFLAG_MOUSEOVER		0x00020000	// for ggButtons.  the mouse is over this control
#define GGFLAG_ROLLOVER			0x00040000	// for ggButtons.  the texture rolls on this control
#define GGFLAG_MOUSEPRIORITY	0x00080000	// the control always has control of the mouse.
#define GGFLAG_DELETEME			0x00100000	// set this, to delete the control, on the next draw
#define GGFLAG_FRAME			0x00200000  // specifies that it's a ggFrame.  i had to do it :P
#define GGFLAG_MULTILINE		0x00400000  // for ggText.  \n is recognized

// icons
#define GGICON_CLOSE			0
#define GGICON_SCROLL			1 // no scrolling yet

// mouse buttons.  same as SDL
#define LEFT_BUTTON		1
#define MIDDLE_BUTTON	2
#define RIGHT_BUTTON	3

#define STATE_RELEASED	0
#define STATE_PRESSED	1

class ggControl
{
public:
	int x,y;				// location in pixels, relative to frame
	int xsize,ysize;		// size in pixels
	float r,g,b,a;
	int flags;				// GGFLAG_* bitflags
	class ggFrame *parent;	// the parent frame of this one (NULL if this is the main frame)
	string stringVal,*stringValP;
	int intVal,*intValP;
	float floatVal,*floatValP;
	int valType; // which value is dominant.  0=none  1=string  2=int  3=float

	virtual void Message(int msg,int arg1,int arg2,int arg3,int arg4)=0;
	virtual void Draw()=0;
	int BaseMessage(int msg,int arg1,int arg2,int arg3,int arg4);	// returns 1 if uses message
	void BaseDraw();
	virtual bool InControl(int xp,int yp);	// returns true if xp,yp is in the control

public:
	ggControl();
	virtual void SetPos(int newx=-1,int newy=-1,int newxsize=-1,int newysize=-1);
	void SetColor(float r,float g,float b);
	void SetColor(float r,float g,float b,float a);
	void SizeToContent();
	void SetFlags(int f);
	void RemoveFlags(int f);
	void AttachString(string *v);
	void AttachInt(int *v);
	void AttachFloat(float *v);
};

class ggFrame : public ggControl
{
	public:
	float framer,frameg,frameb,framea; // frame color values
	vector<ggControl*> controls;	// controls within frame
	ggControl *focus;				// which control has focus in this frame
	ggControl *mousedowncontrol;	// which control the mouse last pressed a button over
	int dragstate;					// 0=not dragging  1=below threshold  2=dragging
	int dragx,dragy;				// where the drag started or last moved stuff
	float scale;					// scale the window and its contents
	float offsetx,offsety;			// for scrolling
	class ggButton *scrollbutton;
	void (*passbackfunc)(int x,int y,int b,int state);// set in PassBackNextMouseClick
	void (*drawFunc)();

	void Message(int msg,int arg1,int arg2,int arg3,int arg4);
	void Draw();
	ggControl *FindControl(int xpos,int ypos);

public:
	ggFrame();
	ggFrame(const char *t);
	~ggFrame();
	void AddControl(ggControl *newcontrol,int newx=-1,int newy=-1,int newxsize=-1,int newysize=-1);
	void RemoveControl(ggControl *c); // doesn't de-allocate anything
	void DeleteControl(ggControl *c); // de-allocates itself, and all stuff nested within
	void MoveToTop(ggControl *c);
	void SetScale(float newscale);
	void SetOffset(int ox,int oy);
	void PassBackNextMouseClick(void (*f)(int x,int y,int b,int state));
	void AddCloseButton();
	void SetDrawFunc(void (*f)());
	void SetFrameColor(float r,float g,float b);
	void SetFrameColor(float r,float g,float b,float a);
};

class ggText : public ggControl
{
	public:
	int cursorloc;

	void Message(int msg,int arg1,int arg2,int arg3,int arg4);
	void Draw();

public:
	ggText();
	ggText(const char *s);
	void SetText(const char *s);
};

class ggButton : public ggControl
{
	public:
	int icon; // used if flags has GGFLAG_ICON.  specifies GGICON_*
	void (*func)();
	int numargs;
	int sarg1,sarg2,sarg3,sarg4,sarg5;
	int hotkey; // ascii value.  0=no hotkey
	int hotkey_extended; // boolean, if the hotkey is extended
	unsigned int texture;
	float tex_sizex,tex_sizey;

	void Message(int msg,int arg1,int arg2,int arg3,int arg4);
	void Draw();

public:
	ggButton(const char *s,void (*f)());
	ggButton(const char *s,void (*f)(int,int,int,int,int),int arg1,int arg2,int arg3,int arg4,int arg5);
	void SetHotkey(int hotkey,int hotkey_extended); // i don't think this works
	void SetTexture(unsigned int tex,int tex_sizex,int tex_sizey, int isrollover, int imagesize=128);
	void CallFunc();
};

class ggMenu : public ggControl
{
public:
	ggFrame frame;
	int nummenuitems;
	ggControl *downcontrol;
	int menuactive; // boolean
	ggFrame *baseframe;
	vector<ggControl*> attached; // these other menus are attached to this one, for normal window menus

	void Message(int msg,int arg1,int arg2,int arg3,int arg4);
	void Draw();
	bool InControl(int xp,int yp);	// overriden to include the pop-up menu frame

	ggMenu();
	ggMenu(const char *s,int xs,int framexs);
	void AddMenuOption(const char *s,void (*f)(int,int,int,int,int),int arg1,int arg2,int arg3,int arg4,int arg5);
	void AddMenuOption(const char *s,void (*f)());
	ggMenu *AddSubMenu(const char *s,int xs);
	void Attach(ggMenu *menu); // attaches all things together
	void SetPos(int newx=-1,int newy=-1,int newxsize=-1,int newysize=-1);	// overriden to include the pop-up menu frame
};

class ggDDMenu : public ggMenu
{
public:
	void Message(int msg,int arg1,int arg2,int arg3,int arg4);

	ggDDMenu(const char *s,int xs,int framexs);
};

class ggCheckBox : public ggControl
{
public:
	void (*func)(bool checked);

	void Message(int msg,int arg1,int arg2,int arg3,int arg4);
	void Draw();

	ggCheckBox(const char *s,void (*f)(bool checked));
	void SetCheck(bool checked);
};

class ggRadioBox : public ggControl
{
public:
	void (*func)(bool checked);
	vector<ggControl*> attached; // atached radio boxes

	void Message(int msg,int arg1,int arg2,int arg3,int arg4);
	void Draw();

	ggRadioBox(const char *s,void (*f)(bool checked));
	void Attach(ggRadioBox *radiobox);
	void SetCheck(); // checks this one, and de-checks the others(naturally)
};

class ggProgressBar : public ggControl
{
public:
	float progress_data;
	float progress_max_data;
	float *progress;
	float *progress_max;

	void Draw();
	void Message(int msg,int arg1,int arg2,int arg3,int arg4);

	ggProgressBar(const char *s);
	void SetProgress(float p);
	void SetProgressPointer(float *p);
	void SetProgressMax(float p);
	void SetProgressMaxPointer(float *p);
};

class ggSpinner : public ggControl
{
public:
	bool use_minvalue,use_maxvalue;
	float *value,minvalue,maxvalue,increment;
	float start_mouse_x,start_mouse_y;

	void Message(int msg,int arg1,int arg2,int arg3,int arg4);
	void Draw();

	ggSpinner(float *value);
	ggSpinner(float *value,float increment);
	ggSpinner(float *value,float increment,float minvalue);
	ggSpinner(float *value,float increment,float minvalue,float maxvalue);
};

class ggTable : public ggControl
{
public:
	float *table;
	int elements;
	float min,max;
	int shift;
	float prev_rel_x;
	float prev_rel_y;

	void Message(int msg,int arg1,int arg2,int arg3,int arg4);
	void Draw();

	ggTable(float *table,int elements,float min=0,float max=1,int shift=1);
};

// global externs
extern ggFrame *mainframe;
extern ggText statusbar;
extern ggControl *caller; // the control that is calling the callback function
extern ggButton *hotkeys[256],*hotkeys_extended[256];
extern bool gui_active; // console_command gui_active
extern ggControl *control_to_delete;

extern int *p_xres;
extern int *p_yres;

// global function defs
void InitMESGUI(int *p_xresa,int *p_yresa,const char *font="Impact.ttf",int fontsize=9,int colheight=20);
void DrawMESGUI();

void GG_SetMouseState(float x, float y);
void GG_MouseButton(int button, int state);
void GG_KeyboardEvent(char ch, int state);	// ch=ascii
void GG_SKeyboardEvent(int key, int state);	// key=scancode

void GUIKeyboardMessage(int c,int extended);
void SetInputCallbackFunc(void (*func)(int,int,int,int,int));
void PassBackNextMouseClick(void (*f)(int x,int y,int b,int state));
void SetAdditionalDrawFunc(void (*func)());

void SetCursorTexture(string filename);
void SetFont(string filename);

#endif
