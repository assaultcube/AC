// the model view is a view that renders a 3d model
struct modelview : view
{
    // model
    string mdl;
    int tex = 0;
    modelattach modelattach[2];

    // animation
    int anim = 0, animbasetime = 0;

    // position, orientation and scale
    float translatex = 0.0f, translatey = 0.0f, translatez = 0.0f;
    float rotatex = 0.0f, rotatey = 0.0f, rotatez = 0.0f;
    float yaw = 0.0f, pitch = 0.0f, roll = 0.0f;
    float scale = 0.0f;

    modelview(view *parent): view(parent){
    };

    ~modelview()
    {
    }

    void measure(int availablewidth, int availableheight)
    {
        // the model is always rendered to the center of the screen and does not participate in layout calculation
        width = 0, height = 0;
    }

    void render(int x, int y)
    {
        glPushMatrix();
        glEnable(GL_DEPTH_TEST);
        glLoadIdentity();
        glTranslatef(translatex + x/1000.0, translatey, translatez);
        glScalef(scale, scale, scale);
        glRotatef(rotatex, 1, 0, 0);
        glRotatef(rotatey, 0, 1, 0);
        glRotatef(rotatez, 0, 0, 1);
        vec pos(0.0f, 0.0f, 0.0f);
        rendermodel(mdl, anim, tex, -1, pos, roll, yaw, pitch, 0, animbasetime, NULL, modelattach, 1.0f);
        glDisable(GL_DEPTH_TEST);
        glPopMatrix();
    }
};