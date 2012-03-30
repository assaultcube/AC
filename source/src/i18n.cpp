// internationalization and localization

#include "cube.h"
#include <locale.h>

SVARP(lang, "");

i18nmanager::i18nmanager(const char *domain, const char *basepath) : domain(domain), basepath(basepath)
{
    locale = setlocale(LC_ALL, ""); // use current default locale
    bindtextdomain(domain, basepath); // set base path
    textdomain(domain);
    bind_textdomain_codeset(domain, "UTF-8"); // we use the utf-8 charset only


    char localelang[3];
    if(!lang[0])
    {
#ifdef WIN32
        GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_SISO639LANGNAME, localelang, 3);
#else
        copystring(localelang, locale, 3);
#endif
        filterlang(localelang, localelang);
        copystring(lang, localelang, 3);
    }
    printf("current locale: %s (%s)\n", locale, lang);

    setlocale(LC_NUMERIC, "C"); // make sure numeric is consistent (very important for float usage in scripts)
                                // Note to self: only do this _after_ using the return value from the previous
                                // setlocale call - this one will overwrite it
}

// export gettext to cubescript
// this way we can provide localization of strings within cubescript
void script_gettext(char *msgid)
{
    const char *translated = _gettext(msgid);
    result(translated);
}

COMMANDN(gettext, script_gettext, ARG_1STR);
