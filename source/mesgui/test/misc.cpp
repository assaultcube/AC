/*
 * Misc. functions for the MESGUI library
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

#include "misc.h"

char shiftize(char c)
{
	if ((c>='a') && (c<'z')) return c + 'A'-'a';
	switch (c) {
		case '`': return '~';
		case '1': return '!';
		case '2': return '@';
		case '3': return '#';
		case '4': return '$';
		case '5': return '%';
		case '6': return '^';
		case '7': return '&';
		case '8': return '*';
		case '9': return '(';
		case '0': return ')';
		case '-': return '_';
		case '=': return '+';
		case '\\': return '|';
		case '[': return '{';
		case ']': return '}';
		case ';': return ':';
		case '\'': return '"';
		case ',': return '<';
		case '.': return '>';
		case '/': return '?';
	}
	return c;
}

bool printable_key(char c)
// returns true if c is a printable key
{
	return (
		(c>='a' && c<='z') ||
		(c>='A' && c<='Z') ||
		c=='`' ||
		c=='1' ||
		c=='2' ||
		c=='3' ||
		c=='4' ||
		c=='5' ||
		c=='6' ||
		c=='7' ||
		c=='8' ||
		c=='9' ||
		c=='0' ||
		c=='-' ||
		c=='=' ||
		c=='\\' ||
		c=='[' ||
		c==']' ||
		c==';' ||
		c=='\'' ||
		c==',' ||
		c=='.' ||
		c=='/' ||
		c=='~' ||
		c=='!' ||
		c=='@' ||
		c=='#' ||
		c=='$' ||
		c=='%' ||
		c=='^' ||
		c=='&' ||
		c=='*' ||
		c=='(' ||
		c==')' ||
		c=='_' ||
		c=='+' ||
		c=='|' ||
		c=='{' ||
		c=='}' ||
		c==':' ||
		c=='"' ||
		c=='<' ||
		c=='>' ||
		c=='?' ||
		c==' ');
}
