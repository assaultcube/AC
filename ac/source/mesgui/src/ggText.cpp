/*
 * Text control for the MESGUI library
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
#include "scancodes.h"

/****************************
          ggText
****************************/

ggText::ggText() : ggControl()
{
	cursorloc=0;
	flags |= GGFLAG_DONT_DRAW_BORDER | GGFLAG_FOCUSABLE | GGFLAG_SIZETOCONTENT;
}

ggText::ggText(const char *s) : ggControl()
{
	cursorloc=0;
	*stringValP=s;
	flags |= GGFLAG_DONT_DRAW_BORDER | GGFLAG_FOCUSABLE | GGFLAG_SIZETOCONTENT;
}

void ggText::Draw()
{
	BaseDraw();

	glColor4f(1,1,1,a);

	bool draw_cursor = (parent && parent->focus==this);

	if (flags & GGFLAG_MULTILINE) {
		int cursorloc_line = cursorloc;
		int a=0,b;
		int pos=0;
		while ((b = stringValP->find('\n',a)) != string::npos) {
			fontprint(x+TO_X,y+TO_Y+pos,stringValP->substr(a,b-a).c_str());
			pos += COL_HEIGHT;
			a = b+1;
		}
		fontprint(x+TO_X,y+TO_Y+pos,stringValP->substr(a).c_str());
	} else {
		fontprint(x+TO_X,y+TO_Y,stringValP->c_str());
		if (draw_cursor) {
			static char cursor[]="_";
			string s = *stringValP;
			s += "a"; // make sure there's a place to put a null char
			s[cursorloc]=0;
			fontprint(x+font->getWidth(s.c_str())+TO_X,y+TO_Y,cursor);
		}
	}
	
}

void ggText::Message(int msg,int arg1,int arg2,int arg3,int arg4)
{
	BaseMessage(msg,arg1,arg2,arg3,arg4);

	switch (msg)
	{
	case GGMSG_GAINFOCUS:
		cursorloc = stringValP->length();
		break;
	case GGMSG_SKEYDOWN: // special keys.  uses scancodes?
		switch (arg1)
		{
		case SC_LEFT:
			if (--cursorloc<0) cursorloc=0;
//			glutPostRedisplay();
			return;

		case SC_RIGHT:
			int len;
			len = stringValP->length();
			if (++cursorloc>len) cursorloc=len;
//			glutPostRedisplay();
			return;

		case SC_HOME:
			cursorloc=0;
//			glutPostRedisplay();
			return;

		case SC_END:
			cursorloc = stringValP->length();
//			glutPostRedisplay();
			return;
		}
		return;

	case GGMSG_KEYDOWN: // normal keys
		if (arg1==13) arg1='\n';
		if (printable_key(arg1) || (arg1=='\n' && (flags & GGFLAG_MULTILINE))) {
			char s[2]={0,0};
			s[0]=(char)arg1;
			stringValP->insert(cursorloc++,s);
			if (flags & GGFLAG_SIZETOCONTENT) SizeToContent();
//			glutPostRedisplay();
			return;
		} else if (arg1=='\n') { // enter
			Message(GGMSG_LOSEFOCUS,0,0,0,0);
		} else if (arg1==27) { // escape
			Message(GGMSG_LOSEFOCUS,0,0,0,0);
		} else if (arg1==127) { // delete
			if (cursorloc<stringValP->length()) {
				stringValP->erase(cursorloc,1);
				if (flags & GGFLAG_SIZETOCONTENT) SizeToContent();
//				glutPostRedisplay();
			}
		} else if (arg1==8) { // backspace
			if (cursorloc>0) {
				stringValP->erase(--cursorloc,1);
				if (flags & GGFLAG_SIZETOCONTENT) SizeToContent();
//				glutPostRedisplay();
			}
		}
	}
}

void ggText::SetText(const char *s)
{
	*stringValP = s;
	if (flags & GGFLAG_SIZETOCONTENT) SizeToContent();
//	glutPostRedisplay();
}
