// the credit scene gives basic information and credit where credit is due
struct creditscene : view
{
    textview *title = NULL;
    int textwidth = 0;
    int creationmillis = 0;

    const char *text =
            "AssaultCube has been created and nurtured by an international\n"
            "community of artists and developers since July 2004. We are\n"
            "people who love building fun games.\n\n"
            "\f2We are looking for lead developers and lead artists to help us build\n"
            " the next generation of AssaultCube for Windows, Mac, Linux and Mobile.\n\n"
            "\f5Contributors in alphabetical order:\n\n"
            "=== Artists ===\n\n"
            "Antiklimax, Archangel, BenWasHere, Brett, daMfr0, DaylixX,\n"
            "DES Clan, DogDancing, ExodusS, fundog, Halo, HitmanDaz[MT],\n"
            "Humus, JCDPC, Jiba, Kothic, Lady NightHawk, leileilol,\n"
            "Lewis Communications, makkE, Matthew Welch, MitaMAN, Nieb,\n"
            "optus, Protox (PrimoTurbo), R4zor, RatBoy, RaZgRiZ, Ruthless,\n"
            "Sanzo, Shadow, sitters, socksky, Steini, Toca, Topher, Undead,\n"
            "Wotwot\n\n"
            "=== Developers ===\n\n"
            "absinth, Arghvark, arkefiende, Brahma, Bukz, driAn, eihrul,\n"
            "flowtron, GeneralDisarray, grenadier, KanslozeClown, Luc@s,\n"
            "RandumKiwi, Ronald_Reagan, SKB, stef, tempest, V-Man,\n"
            "VonDrakula, wahnfred\n\n"
            "=== Platform & Community ===\n\n"
            "Apollo, dtd, jamz, Medusa, X-Ray_Dog\n\n"
            "=== Technology ===\n\n"
            "Cube Engine by aardappel, eihrul and others\n"
            "ENet Networking Library by eihrul\n"
            "GL4ES OpenGL translation by ptitSeb and others\n"
            "SDL2, SDL_Image, OggVorbis, OpenAL-Soft, zlib\n\n"
            "=== Textures ===\n\n"
            "3D Cafe, Articool, Boeck, Chris Zastrow, Craig Fortune,\n"
            "Digital Flux, DrunkenM, Golgotha team, GRsites, John Solo,\n"
            "Kurt Keslar, Lemog 3D, NOCTUA graphics, Rohrschach,\n"
            "www.afflict.net, www.imageafter.com, www.mayang.com,\n"
            "www.openfootage.net\n\n"
            "=== Sounds ===\n\n"
            "DCP, acclivity, Bahutan, cameronmusic, dommygee, droboide,\n"
            "ermine, fonogeno, fresco, ignotus, livindead,\n"
            "Lukas Zapletal & Ludek Horacek (Music), mich3d, mwl500,\n"
            "nicStage, nofeedbak, NoiseCollector, ReWired, Rhedcerulean,\n"
            "Syna Max, Tremulous team, vuzz, wildweasel, WIM,\n"
            "www.soundsnap.com\n\n"
            "=== Special Thanks===\n\n"
            "Chris Robinson, Rick Helmus, Verbal_, gibnum1k\n\n"
            "Please find more detailed credits and licensing information at\n"
            "https://assault.cubers.net/docs/team.html\n"
            "https://assault.cubers.net/docs/license.html\n\n"
            "\f2We are looking for lead developers and lead artists to help us build\n"
            "the next generation of AssaultCube for Windows, Mac, Linux and Mobile.\n\n\n"
            "\f5[Tap to dismiss]\n\n";

    creditscene(view *parent) : view(parent)
    {
        title = new textview(this, text, false);
        children.add(title);
    };

    ~creditscene()
    {
        DELETEP(title);
    }

    virtual void oncreate()
    {
        view::oncreate();
        creationmillis = lastmillis;
    }

    virtual void measure(int availablewidth, int availableheight)
    {
        title->measure(availablewidth/4, availableheight/4);
        textwidth = text_width(text);
        width = availablewidth;
        height = availableheight;
    }

    void render(int x, int y)
    {
        blendbox(0, 0, VIRTW, VIRTH, false, -1, &config.black);

        int childheights = title->height;
        int yoffset = height/2 - childheights - (lastmillis-creationmillis)*VIRTH/1000/30;
        title->render(x + (VIRTW-textwidth)/2, y + yoffset);

        bbox.x1 = x;
        bbox.x2 = x + width;
        bbox.y1 = y;
        bbox.y2 = y + height;
    }

    virtual void captureevent(SDL_Event *event)
    {
        if(event->type == SDL_FINGERDOWN)
        {
            delete viewstack.pop();
            return;
        }
        view::captureevent(event);
    }
};