// internationalization and localization

// localization manager
struct i18nmanager
{
	const char *basepath;
	const char *domain;
	char *locale;

	i18nmanager(const char *domain, const char *basepath); // initialize locale system
	
};
