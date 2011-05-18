#if !defined(WINCOMPAT_INCLUDED) && !defined(PLATFORM_WINDOWS) && !defined(WIN32) && !defined(WINDOWS) && !defined(__WIN32__)
#define WINCOMPAT_INCLUDED

/**
 *
 * Author: Magnus Naeslund (mag@fbab.net, mag@bahnhof.se)
 * (c) 2000 Magnus Naeslund, all rights reserved
 * Copyright (c) 2000-2011 Magnus Naeslund (magnus.naslund@mucrosolutions.se,
 * mag@fbab.net, mag@bahnhof.se)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef TRUE
  #define TRUE 1
#endif
#ifndef FALSE
  #define FALSE 0
#endif

#define _kbhit kbhit
#define stricmp strcasecmp
#define strnicmp strncasecmp

#define Sleep(x) usleep((x)*1000)

static int            inited=0;
static struct termios ori;

static void tcatexit(){
   tcsetattr(0,0,&ori);
}

static void init_terminal(){
   struct termios t;
   tcgetattr(0,&t);
   tcgetattr(0,&ori);
   t.c_lflag &= ~(ICANON);
   tcsetattr(0,0,&t);
   atexit(tcatexit);
}

static inline int kbhit(){
  fd_set rfds;
  struct timeval tv;

   if (!inited){
	  inited=1;
	  init_terminal();
   }
   
   FD_ZERO(&rfds);
   FD_SET(0, &rfds);
   tv.tv_sec = 0;
   tv.tv_usec = 10*1000;
   return select(1, &rfds, NULL, NULL, &tv)>0;
}

static inline int getch(){
   fd_set rfds;
   
   if (!inited){
	  inited=1;
	  init_terminal();
   }
   
   FD_ZERO(&rfds);
   FD_SET(0, &rfds);
   if (select(1, &rfds, NULL, NULL, NULL)>0)
	 return getchar();
   else{
	  printf("wincompat.h: select() on fd 0 failed\n");
	  return 0xDeadBeef;
   }	 
}

#endif
