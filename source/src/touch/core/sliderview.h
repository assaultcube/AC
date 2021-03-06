// the slider allows picking a value from a value range
struct sliderview : view
{
    int padding, contentwidth, contentheight;
    string text; // determines label at lefthand side of the line
    int minval, maxval, currentval; // determines valid value range
    int linex1, linex2; // determines horizontal dimension of line excluding the label
    int buttony1, buttony2; // determines vertical dimension of the slider button that is dran on top of the line
    vector<char *> labels; // determines the labels to show as current value
    int mintextwidth = 0;

    sliderview(view *parent, const char *text, int minval, int maxval, int currentval, vector<char*> *labels)
            : view(parent), minval(minval), maxval(maxval), currentval(currentval) {
        copystring(this->text, text);
        if(labels) loopv(*labels) this->labels.add(newstring((*labels)[i]));
    };

    ~sliderview()
    {
        labels.deletearrays();
    }

    void measure(int availablewidth, int availableheight)
    {
        padding = FONTH/2;
        width = availablewidth;
        contentwidth = width-2*padding;
        contentheight = FONTH;
        height = contentheight + padding;
    }

    void render(int x, int y)
    {
        if(!visible) return;

        int tw = max(mintextwidth, text_width(text));

        // draw slider line
        color white(1.0f, 1.0f, 1.0f);
        int fh = FONTH*8/10;
        linex1 = (x + 2*padding + tw);
        linex2 = (x + contentwidth);
        int linewidth = linex2 - linex1;
        blendbox(linex1, y + padding + fh*2/6, linex2, y + padding + fh*4/6, false, -1, &white);

        // draw slider button on top of line
        int sliderbuttonwidth = fh*2/6;
        float sliderperc = float(currentval)/(maxval-minval);
        int buttonx1 = x + 2*padding + tw + sliderperc * linewidth;
        int buttonx2 = x + 2*padding + tw + sliderperc * linewidth + sliderbuttonwidth;
        buttony1 = y + padding;
        buttony2 = y + padding + fh;
        blendbox(buttonx1, buttony1, buttonx2, buttony2, false, -1, &white);

        // draw label and value
        glEnable(GL_BLEND);
        draw_text(text, x + padding, y + padding);
        char *currentvaluestr = NULL;
        if(currentval >= 0 && currentval < labels.length())
            currentvaluestr = labels[currentval];
        else
        {
            string value;
            itoa(value, currentval);
            currentvaluestr = value;
        }
        draw_text(currentvaluestr, linex2 + padding, y + padding);
        glDisable(GL_BLEND);

        bbox.x1 = x;
        bbox.x2 = x + width;
        bbox.y1 = y;
        bbox.y2 = y + height;
    }

    virtual void captureevent(SDL_Event *event)
    {
        view::captureevent(event);
        switch(event->type)
        {
            case SDL_FINGERDOWN:
            case SDL_FINGERMOTION:
            {
                // we want to apply the new value immediately whenever a valid finger motion happens
                int val = event->tfinger.x * VIRTW - linex1;
                float perc = clamp(float(val) / (linex2 - linex1), 0.0f, 1.0f);
                currentval = perc * (maxval - minval);
                break;
            }
            default:
                break;
        }
    }
};