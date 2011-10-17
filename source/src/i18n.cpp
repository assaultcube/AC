// internationalization and localization

#include "cube.h"
#include <locale.h>

i18nmanager::i18nmanager(const char *domain, const char *basepath) : domain(domain), basepath(basepath)
{
    locale = setlocale(LC_ALL, ""); // use current default locale
    // C forbids multiple locales, sireus - the line you put it messed up the locale var completely - flowtron got this output:
    // current locale: packages/locale (pa)
    // see this mail: http://www.sourceware.org/ml/binutils/2000-06/msg00024.html
    // I noticed that problem with e.g. zoom-factor - my workaround was to put it into a division-call, instead of as a constant; works regardless of seperator
    // but this won't necessarily work in all cases, the mail I linked talks about implementing replacement functions that mimick the C-locale.
    // so .. this gets marked: FIXME
    //setlocale(LC_NUMERIC, "C");     // make sure numeric is consistent (very important for float usage in scripts)
    bindtextdomain(domain, basepath); // set base path
    textdomain(domain);
    bind_textdomain_codeset(domain, "UTF-8"); // we use the utf-8 charset only

    char lang[3];
#ifdef WIN32
    GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_SISO639LANGNAME, lang, 3);
#else
    copystring(lang, locale, 3);
#endif
    filterlang(lang, lang);
    alias("LANG", lang);

    printf("current locale: %s (%s)\n", locale, lang);
}

// export gettext to cubescript
// this way we can provide localization of strings within cubescript
void script_gettext(char *msgid)
{
    const char *translated = _gettext(msgid);
    result(translated);
}

COMMANDN(gettext, script_gettext, ARG_1STR);
