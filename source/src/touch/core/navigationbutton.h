// the navigation button is an arrow shaped button to navigate forwards or backwards
struct navigationbutton : view
{
    int padding, contentsize;

    enum type {
        NEXT,
        PREV
    };
    type buttontype;

    navigationbutton(view *parent, type buttontype)
            : view(parent), buttontype(buttontype) {
    };

    ~navigationbutton()
    {
    }

    void measure(int availablewidth, int availableheight)
    {
        width = height = min(availablewidth, availableheight);
        padding = width/10;
        contentsize = width-2*padding;
    }

    void render(int x, int y)
    {
        if(!visible) return;

        int xcenter = x+padding+contentsize/2;
        int ycenter = y+padding+contentsize/2;
        int outercircleradius = contentsize/2;
        int innnercircleradius = contentsize/2-padding;
        int trianglesize = innnercircleradius*3/4;
        glColor3f(0.0f, 0.0f, 0.0f);
        circle(-1, xcenter, ycenter, outercircleradius, 0, 0, 0, 64);
        glColor3f(1.0f, 1.0f, 1.0f);
        circle(-1, xcenter, ycenter, innnercircleradius, 0, 0, 0, 64);
        glColor3f(0.0f, 0.0f, 0.0f);

        float xoffset = 0.0f, yoffset = 0.0f;
        int direction = NEXT;
        switch(buttontype)
        {
            case NEXT:
                direction = 2;
                xoffset = xcenter-trianglesize/3.0f;
                yoffset = ycenter-trianglesize/2.0f;
                break;
            case PREV:
                direction = 3;
                xoffset = xcenter-trianglesize*2.0/3.0f;
                yoffset = ycenter-trianglesize/2.0f;
                break;
        }
        arrow(-1, direction, xoffset, yoffset, trianglesize, 0, 0);

        bbox.x1 = x;
        bbox.x2 = x + width;
        bbox.y1 = y;
        bbox.y2 = y + height;
    }
};