/*
 * Spinner control for the MESGUI library
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
          ggSpinner
****************************/

ggSpinner::ggSpinner(float *valuea) : ggControl()
{
	flags |= GGFLAG_MOUSEPRIORITY;
	value = valuea;
	increment = 1;
	use_minvalue=false;
	use_maxvalue=false;
}

ggSpinner::ggSpinner(float *valuea,float incrementa) : ggControl()
{
	flags |= GGFLAG_MOUSEPRIORITY;
	value = valuea;
	increment = incrementa;
	use_minvalue=false;
	use_maxvalue=false;
}

ggSpinner::ggSpinner(float *valuea,float incrementa,float minvaluea) : ggControl()
{
	flags |= GGFLAG_MOUSEPRIORITY;
	value = valuea;
	minvalue = minvaluea;
	increment = incrementa;
	use_minvalue=true;
	use_maxvalue=false;
}

ggSpinner::ggSpinner(float *valuea,float incrementa,float minvaluea,float maxvaluea) : ggControl()
{
	flags |= GGFLAG_MOUSEPRIORITY;
	value = valuea;
	minvalue = minvaluea;
	maxvalue = maxvaluea;
	increment = incrementa;
	use_minvalue=true;
	use_maxvalue=true;
}

void ggSpinner::Draw()
{
	BaseDraw();

	glColor4f(1,1,1,a);
	char s[64]="(null)";
	if (value) sprintf(s,"%.2f",*value);
	fontprint(x+TO_X,y+TO_Y,s);
}

void ggSpinner::Message(int msg,int arg1,int arg2,int arg3,int arg4)
{
	BaseMessage(msg,arg1,arg2,arg3,arg4);

	switch (msg)
	{
	case GGMSG_DOWN:
		start_mouse_x = mouse_x;
		start_mouse_y = mouse_y;
//		glutPostRedisplay();
		break;
	case GGMSG_UP:
//		glutPostRedisplay();
		break;
	case GGMSG_MOUSEMOTION:
		if (value) {
			*value -= (mouse_y - start_mouse_y)*increment;
			if (use_minvalue && *value<minvalue) *value=minvalue;
			if (use_maxvalue && *value>maxvalue) *value=maxvalue;
		}
		start_mouse_x = mouse_x;
		start_mouse_y = mouse_y;
//		glutPostRedisplay();
		break;
	}
}
