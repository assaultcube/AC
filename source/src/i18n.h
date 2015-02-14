// internationalization and localization

// localization manager
struct i18nmanager
{
    const char *domain;
    const char *basepath;
    char *locale;

    i18nmanager(const char *domain, const char *basepath); // initialize locale system

};

extern char *lang;
