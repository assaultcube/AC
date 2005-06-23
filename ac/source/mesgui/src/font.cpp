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

#include "font.h"

#include <ft2build.h>
#include <freetype/freetype.h>	//FT_FREETYPE_H
#include <freetype/ftglyph.h>	//FT_GLYPH_H

#pragma warning(disable:4786)
#ifdef WIN32
#include <windows.h>
#endif
#include <stdio.h>
#include <string.h>
#include <map>
#include <string>
#include <vector>
#include <GL/gl.h>
#include "misc.h"
using namespace std;

FT_Library freetype;

Font::Font(const char *fontFilename,int fontsizea)
{
	fontsize = fontsizea;

	FT_Face face;
	int tex_x=0,tex_y=0;
	char tex[256*256];
	memset(tex,0,256*256);
	maxheight=0;
	memset(chars,0,sizeof(chars));

	// a #define to make error handling cleaner
	FT_Error error;
#define SAFE_DO(stuff_,s_) \
	error = stuff_; \
	if (error) { \
		printf("Error initializing font: %s (%d)\n",s_,error); \
		return; \
	}
	SAFE_DO(FT_Init_FreeType(&freetype),"Could not initialize FreeType");

	// make a vector of paths to search for fonts
	vector<string> fontSearchPath;
	fontSearchPath.push_back("");
	fontSearchPath.push_back("/usr/share/fonts/truetype/");
	char *windir = getenv("windir");
	if (windir) {
		fontSearchPath.push_back(windir+(string)"/fonts/");
	}

	// create a new face
	bool worked=false;
	for (int i=0; i<fontSearchPath.size() && !worked; i++) {
		string fontPath = fontSearchPath[i]+fontFilename;
		if (!FT_New_Face(freetype,fontPath.c_str(),0,&face)) worked=true;
	}
	if (!worked) {
		printf("Error loading %s\n",fontFilename);
		return;
	}

	SAFE_DO(FT_Set_Char_Size(face,0,fontsize*64,96,96),"Could not set char size");

	//note:  i'd like to use unicode here, but fonts seem to be missing 'b'-'z' with that encoding
	SAFE_DO(FT_Select_Charmap(face,ft_encoding_apple_roman),"Error selecting charmap");

	// it doesn't seem to map directly to ASCII, so i had to hack a map
	map<char,FT_ULong> ASCIIToAppleRoman;
	int c;
	for (c=0; c<256; c++) if (printable_key(c)) {
		ASCIIToAppleRoman[c] = c+29;
		if (c>'a') ASCIIToAppleRoman[c]++; // there is an extra char between 'a' and 'b'
	}

	for (c=0; c<256; c++) if (printable_key(c)) {

		FT_UInt glyph_index = FT_Get_Char_Index(face,ASCIIToAppleRoman[c]);
		SAFE_DO(FT_Load_Char(face, glyph_index, FT_LOAD_DEFAULT), "Could not load glyph");
		FT_Glyph glyph;
		SAFE_DO(FT_Get_Glyph(face->glyph, &glyph), "Could not get glyph");
		FT_Matrix matrix;
		matrix.xx = long(1.2 * 0x10000);
		matrix.xy = 0;
		matrix.yx = 0;
		matrix.yy = long(1.2 * 0x10000);
		SAFE_DO(FT_Glyph_Transform(glyph, &matrix, 0), "Could not scale glyph");
		SAFE_DO(FT_Glyph_To_Bitmap(&glyph, ft_render_mode_normal, NULL, 1), "Could not create bitmap");
		FT_BitmapGlyph glyph_bitmap = (FT_BitmapGlyph)glyph;
		if (glyph_bitmap->bitmap.pixel_mode != ft_pixel_mode_grays) {
			printf("Font must have a greyscale pixel format");
			return;
		}

		int width  = glyph_bitmap->bitmap.width;
		int height = glyph_bitmap->bitmap.rows;

		if (height>maxheight) maxheight=height;
		if (tex_x+width>=256) {
			tex_y += maxheight+1;
			tex_x=0;
		}

		chars[c].x_texture = tex_x/256.0f;
		chars[c].y_texture = tex_y/256.0f;
		chars[c].x_size = (float)width;
		chars[c].y_size = (float)height;
		chars[c].advance = glyph->advance.x / (float)0x10000;
		chars[c].top  = -glyph_bitmap->top;
		chars[c].left = glyph_bitmap->left;

		int x,y;
		for (y=0; y<glyph_bitmap->bitmap.rows; y++) {
			for (x=0; x<glyph_bitmap->bitmap.width; x++) {
				tex[(tex_x+x)+(tex_y+y)*256] = glyph_bitmap->bitmap.buffer[x+y*glyph_bitmap->bitmap.pitch];
			}
		}
		
		tex_x += width+1;

		FT_Done_Glyph(glyph);
	}

	glGenTextures(1, &texture);

	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, 1, 256, 256, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, tex);

	FT_Done_FreeType(freetype);

	float cx,cy,wx,wy,xs,ys,extra,px,py;

	displaylist=glGenLists(256);							// Creating 256 Display Lists

	for (int loop=0; loop<256; loop++) if (printable_key(loop))
	{
		cx=chars[loop].x_texture;
		cy=chars[loop].y_texture;
		wx=chars[loop].x_size/256.0f;
		wy=chars[loop].y_size/256.0f;
		xs=(float)chars[loop].x_size;
		ys=(float)chars[loop].y_size;
		px=chars[loop].left;
		py=chars[loop].top;
		float a=-2;

		extra=(loop==' ')?4.0f:0;

		glNewList(displaylist+loop,GL_COMPILE);
			glBegin(GL_QUADS);
				glTexCoord2f(cx,	cy+wy);	glVertex2f(px,   py+ys);
				glTexCoord2f(cx+wx,	cy+wy);	glVertex2f(px+xs,py+ys);
				glTexCoord2f(cx+wx,	cy);	glVertex2f(px+xs,py);
				glTexCoord2f(cx,	cy);	glVertex2f(px,   py);
			glEnd();
			glTranslatef(chars[loop].advance,0,0);
		glEndList();
	}
}

void Font::output( float x, float y, const char* text )
{
	int i;
	short s[128];
	int len = strlen(text);
	for (i=0; i<len; i++)
		s[i]=text[i]+displaylist;
	glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	glBindTexture(GL_TEXTURE_2D, texture);

	glPushMatrix();
	glTranslatef(x,y+maxheight*0.8f,0);
	glCallLists(len,GL_SHORT,s);
	glPopMatrix();

	glPopAttrib();
}

float Font::getWidth( const char* text )
{
	float width=0;
	for (char *p=(char*)text; *p; p++)
		width += chars[*p].advance;
	return width;
}

int Font::getHeight() const
{
	return maxheight;
}
