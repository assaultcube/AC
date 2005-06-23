/*
 * TrueType font loader for the MESGUI library
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

#ifndef FONT_H
#define FONT_H

class FontCharacter
{
public:
	float x_texture,y_texture;
	float x_size,y_size;
	float advance;
	int top,left;
};

class Font {
private:
	FontCharacter chars[256];
	unsigned int texture;
	unsigned int displaylist;
	int maxheight;
	int fontsize;

public:
	Font(const char *filename,int fontsize);
	void output( float x, float y, const char* text );
	float getWidth( const char* text );
	int getHeight() const;
};

#endif
