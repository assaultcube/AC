// rendertext.cpp: based on Don's gl_text.cpp

#include "cube.h"

short char_coords[96][4] = 
{
    {0,0,25,64},        //!
    {25,0,54,64},       //"
    {54,0,107,64},      //#
    {107,0,148,64},     //$
    {148,0,217,64},     //%
    {217,0,263,64},     //&
    {263,0,280,64},     //'
    {280,0,309,64},     //(
    {309,0,338,64},     //)
    {338,0,379,64},     //*
    {379,0,432,64},     //+
    {432,0,455,64},     //,
    {455,0,484,64},     //-
    {0,64,21,128},      //.
    {23,64,52,128},     ///
    {52,64,93,128},     //0
    {93,64,133,128},    //1
    {133,64,174,128},   //2
    {174,64,215,128},   //3
    {215,64,256,128},   //4
    {256,64,296,128},   //5
    {296,64,337,128},   //6
    {337,64,378,128},   //7
    {378,64,419,128},   //8
    {419,64,459,128},   //9
    {459,64,488,128},   //:
    {0,128,29,192},     //;
    {29,128,81,192},    //<
    {81,128,134,192},   //=
    {134,128,186,192},  //>
    {186,128,221,192},  //?
    {221,128,285,192},  //@
    {285,128,329,192},  //A
    {329,128,373,192},  //B
    {373,128,418,192},  //C
    {418,128,467,192},  //D
    {0,192,40,256},     //E
    {40,192,77,256},    //F
    {77,192,127,256},   //G
    {127,192,175,256},  //H
    {175,192,202,256},  //I
    {202,192,231,256},  //J
    {231,192,275,256},  //K
    {275,192,311,256},  //L
    {311,192,365,256},  //M
    {365,192,413,256},  //N
    {413,192,463,256},  //O
    {1,256,38,320},     //P
    {38,256,89,320},    //Q
    {89,256,133,320},   //R
    {133,256,176,320},  //S
    {177,256,216,320},  //T
    {217,256,263,320},  //U
    {263,256,307,320},  //V
    {307,256,370,320},  //W
    {370,256,414,320},  //X
    {414,256,453,320},  //Y
    {453,256,497,320},  //Z
    {0,320,29,384},     //[
    {29,320,58,384},    //"\"
    {59,320,87,384},    //]
    {87,320,139,384},   //^
    {139,320,180,384},  //_
    {180,320,221,384},  //`
    {221,320,259,384},  //a
    {259,320,299,384},  //b
    {299,320,332,384},  //c
    {332,320,372,384},  //d
    {372,320,411,384},  //e
    {411,320,433,384},  //f
    {435,320,473,384},  //g
    {0,384,40,448},     //h
    {40,384,56,448},    //i
    {58,384,80,448},    //j
    {80,384,118,448},   //k
    {118,384,135,448},  //l
    {135,384,197,448},  //m
    {197,384,238,448},  //n
    {238,384,277,448},  //o
    {277,384,317,448},  //p
    {317,384,356,448},  //q
    {357,384,384,448},  //r
    {385,384,417,448},  //s
    {417,384,442,448},  //t
    {443,384,483,448},  //u
    {0,448,38,512},     //v
    {38,448,90,512},    //w
    {90,448,128,512},   //x
    {128,448,166,512},  //y
    {166,448,200,512},  //z
    {200,448,241,512},  //{
    {241,448,270,512},  //|
    {270,448,310,512},  //}
    {310,448,363,512},  //~
};

int text_width(char *str)
{
    int x = 0;
    for (int i = 0; str[i] != 0; i++)
    {
        int c = str[i];
        if(c=='\t') { x = (x+PIXELTAB)/PIXELTAB*PIXELTAB; continue; }; 
        if(c=='\f') continue; 
        if(c==' ') { x += FONTH/2; continue; };
        c -= 33;
        if(c<0 || c>=95) continue;
        int in_width = char_coords[c][2] - char_coords[c][0];
        x += in_width + 1;
    }
    return x;
}


void draw_textf(char *fstr, int left, int top, int gl_num, ...)
{
    sprintf_sdlv(str, gl_num, fstr);
    draw_text(str, left, top, gl_num);
};

void draw_text(char *str, int left, int top, int gl_num)
{
    glBlendFunc(GL_ONE, GL_ONE);
    glBindTexture(GL_TEXTURE_2D, gl_num);
    glColor3ub(255,255,255);

    int x = left;
    int y = top;

    int i;
    float in_left, in_top, in_right, in_bottom;
    int in_width, in_height;

    for (i = 0; str[i] != 0; i++)
    {
        int c = str[i];
        if(c=='\t') { x = (x-left+PIXELTAB)/PIXELTAB*PIXELTAB+left; continue; }; 
        if(c=='\f') { glColor3ub(64,255,128); continue; };
        if(c==' ') { x += FONTH/2; continue; };
        c -= 33;
        if(c<0 || c>=95) continue;

        in_left    = ((float) char_coords[c][0])   / 512.0f;
        in_top     = ((float) char_coords[c][1]+2) / 512.0f;
        in_right   = ((float) char_coords[c][2])   / 512.0f;
        in_bottom  = ((float) char_coords[c][3]-2) / 512.0f;

        in_width   = char_coords[c][2] - char_coords[c][0];
        in_height  = char_coords[c][3] - char_coords[c][1];

        glBegin(GL_QUADS);
        glTexCoord2f(in_left,  in_top   ); glVertex2i(x,            y);
        glTexCoord2f(in_right, in_top   ); glVertex2i(x + in_width, y);
        glTexCoord2f(in_right, in_bottom); glVertex2i(x + in_width, y + in_height);
        glTexCoord2f(in_left,  in_bottom); glVertex2i(x,            y + in_height);
        glEnd();
        
        xtraverts += 4;
        x += in_width  + 1;
    }
}

// also Don's code, so goes in here too :)

void draw_envbox_aux(float s0, float t0, int x0, int y0, int z0,
                     float s1, float t1, int x1, int y1, int z1,
                     float s2, float t2, int x2, int y2, int z2,
                     float s3, float t3, int x3, int y3, int z3,
                     int texture)
{
    glBindTexture(GL_TEXTURE_2D, texture);
    glBegin(GL_QUADS);
    glTexCoord2f(s3, t3); glVertex3d(x3, y3, z3);
    glTexCoord2f(s2, t2); glVertex3d(x2, y2, z2);
    glTexCoord2f(s1, t1); glVertex3d(x1, y1, z1);
    glTexCoord2f(s0, t0); glVertex3d(x0, y0, z0);
    glEnd();
    xtraverts += 4;
}

void draw_envbox(int t, int w)
{
    glDepthMask(GL_FALSE);

    draw_envbox_aux(1.0f, 1.0f, -w, -w,  w,
                    0.0f, 1.0f,  w, -w,  w,
                    0.0f, 0.0f,  w, -w, -w,
                    1.0f, 0.0f, -w, -w, -w, t);

    draw_envbox_aux(1.0f, 1.0f, +w,  w,  w,
                    0.0f, 1.0f, -w,  w,  w,
                    0.0f, 0.0f, -w,  w, -w,
                    1.0f, 0.0f, +w,  w, -w, t+1);

    draw_envbox_aux(0.0f, 0.0f, -w, -w, -w,
                    1.0f, 0.0f, -w,  w, -w,
                    1.0f, 1.0f, -w,  w,  w,
                    0.0f, 1.0f, -w, -w,  w, t+2);

    draw_envbox_aux(1.0f, 1.0f, +w, -w,  w,
                    0.0f, 1.0f, +w,  w,  w,
                    0.0f, 0.0f, +w,  w, -w,
                    1.0f, 0.0f, +w, -w, -w, t+3);

    draw_envbox_aux(0.0f, 1.0f, -w,  w,  w,
                    0.0f, 0.0f, +w,  w,  w,
                    1.0f, 0.0f, +w, -w,  w,
                    1.0f, 1.0f, -w, -w,  w, t+4);

    draw_envbox_aux(0.0f, 1.0f, +w,  w, -w,
                    0.0f, 0.0f, -w,  w, -w,
                    1.0f, 0.0f, -w, -w, -w,
                    1.0f, 1.0f, +w, -w, -w, t+5);

    glDepthMask(GL_TRUE);
}
