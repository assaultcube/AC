// internationalization and localization

#include "pch.h"
#include "cube.h"

i18nmanager::i18nmanager(const char *domain, const char *basepath) : domain(domain), basepath(basepath)
{
	locale = setlocale(LC_ALL, ""); // use current default locale
	bindtextdomain(domain, basepath); // set base path
	textdomain(domain);

	printf("current locale: %s\n", locale);

	printf(_("hello world\n")); // test output
}