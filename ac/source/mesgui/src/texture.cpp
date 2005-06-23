/*
 * Texture functions for the MESGUI library
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

#include "texture.h"

#include <SDL.h>
#ifdef WIN32
#include <windows.h>
#define strcasecmp strcmpi
#endif
#include <GL/gl.h>
#include <GL/glu.h>
#include <string.h>

#define MAX_TEXTURES 200
unsigned int textureindices[MAX_TEXTURES];
char texturefilenames[MAX_TEXTURES][40];
int texturetypes[MAX_TEXTURES];
int numtextures=0;

void InvertY(void *pixels,int BytesPerPixel,int w,int h)
{
	int x,y;
	unsigned char *p = (unsigned char*)pixels;
	unsigned char temp[4];
	for (y=0; y<h/2; y++) {
		for (x=0; x<w; x++) {
			memcpy(temp,&p[(x+y*w)*BytesPerPixel],BytesPerPixel);
			memcpy(&p[(x+y*w*BytesPerPixel)],&p[(x+(h-1-y)*w)*BytesPerPixel],BytesPerPixel);
			memcpy(&p[(x+(h-1-y)*w)*BytesPerPixel],temp,BytesPerPixel);
		}
	}
}

unsigned int ReallyLoadTexture(const char *filename,int type)
{
	unsigned int texture=0;

	SDL_Surface *surface;
	surface=SDL_LoadBMP(filename);
	if (!surface) {
		printf("Could not load texture \"%s\"\n",filename);
		return 0;
	}
//	InvertY(surface->pixels,surface->format->BytesPerPixel,surface->w,surface->h);
	glGenTextures(1, &texture);

	int fmt=0;
	switch (surface->format->BytesPerPixel) {
		case 1: fmt=GL_LUMINANCE; break;
		case 3: fmt=GL_BGR_EXT; break;
		case 4: fmt=GL_BGRA_EXT; break;
	}
/*
	if (surface->format->BytesPerPixel>=3) {
		int x,y;
		byte r,g,b;
		for (y=0; y<surface->h; y++)
			for (x=0; x<surface->w; x++) {
				b = ((byte*)surface->pixels)[(x+y*surface->w)*surface->format->BytesPerPixel+0];
				g = ((byte*)surface->pixels)[(x+y*surface->w)*surface->format->BytesPerPixel+1];
				r = ((byte*)surface->pixels)[(x+y*surface->w)*surface->format->BytesPerPixel+2];
				((byte*)surface->pixels)[(x+y*surface->w)*surface->format->BytesPerPixel+0] = r;
				((byte*)surface->pixels)[(x+y*surface->w)*surface->format->BytesPerPixel+1] = g;
				((byte*)surface->pixels)[(x+y*surface->w)*surface->format->BytesPerPixel+2] = b;
			}
	}
*/

	switch (type) {
	case 0:
	{
		glBindTexture(GL_TEXTURE_2D, texture);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, surface->format->BytesPerPixel, surface->w, surface->h, 0, fmt, GL_UNSIGNED_BYTE, surface->pixels);
		printf("%s: %d\n",filename,surface->format->BytesPerPixel);
		break;
	}

	case 1:
		glBindTexture(GL_TEXTURE_2D, texture);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_NEAREST);
		gluBuild2DMipmaps(GL_TEXTURE_2D, surface->format->BytesPerPixel, surface->w, surface->h, fmt, GL_UNSIGNED_BYTE, surface->pixels);
		break;
	}

	SDL_FreeSurface(surface);

	printf("Loaded texture \"%s\".\n",filename);

	return texture;
}

unsigned int *LoadTexture(const char* filename,int type)
/* type
	0 = not
    1 = mipmapped
*/
{
	int i;

	for (i=0; i<numtextures; i++) {
		if (!strcasecmp(filename,texturefilenames[i])) {
			return &textureindices[i];
		}
	}

	if (numtextures >= MAX_TEXTURES) {
		printf("Too many textures.  I only have support for %d\n",MAX_TEXTURES);
		return 0;
	}

	textureindices[numtextures] = ReallyLoadTexture(filename,type);
	if (textureindices[numtextures]==0) return NULL;

	memcpy(texturefilenames[numtextures],filename,40);
	texturetypes[numtextures]=type;
	numtextures++;

	return &textureindices[numtextures-1];
}

void ReloadTextures()
{
	for (int i=0; i<numtextures; i++) {
		textureindices[i] = ReallyLoadTexture(texturefilenames[i],texturetypes[i]);
	}
}
