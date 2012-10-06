// internationalization and localization

#include "cube.h"
#include <locale.h>

SVARFP(lang, "en", filterlang(lang, lang));

i18nmanager::i18nmanager(const char *domain, const char *basepath) : domain(domain), basepath(basepath)
{
    locale = setlocale(LC_ALL, ""); // use current default locale
    bindtextdomain(domain, basepath); // set base path
    textdomain(domain);
    bind_textdomain_codeset(domain, "UTF-8"); // we use the utf-8 charset only

    setlocale(LC_NUMERIC, "C"); // make sure numeric is consistent (very important for float usage in scripts)
}

// export gettext to cubescript
// this way we can provide localization of strings within cubescript
void script_gettext(char *msgid)
{
    const char *translated = _gettext(msgid);
    result(translated);
}

COMMANDN(gettext, script_gettext, "s");
