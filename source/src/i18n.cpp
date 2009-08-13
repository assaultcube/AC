// internationalization and localization

#include "pch.h"
#include "cube.h"
#include <locale.h>

i18nmanager::i18nmanager(const char *domain, const char *basepath) : domain(domain), basepath(basepath)
{
	locale = setlocale(LC_ALL, ""); // use current default locale
	bindtextdomain(domain, basepath); // set base path
	textdomain(domain);

	printf("current locale: %s\n", locale);
}

// export gettext to cubescript
void script_gettext(char *msgid)
{
	const char *translated = _gettext(msgid);
	result(translated);
}

COMMANDN(gettext, script_gettext, ARG_1STR);
