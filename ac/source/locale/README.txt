==AC Localization==

Install gnu gettext to get the required tools for building translation files. There are UI tools to help you on this task, as example on Linux you could try gtranslator or kbabel. These tools allow you to manage the translation files without ever touching the command line.
However the following text describes the usage of the underlying tools so you get an understanding of the process.
More information can be found in the gnu gettext documentation/tutorials.


Steps:

1. Initial String Export

Run update_pot_template to extract all localizable strings from the C++ source and from cubescript. The AC.pot template will be updated. Edit AC.pot and fill out the header details, such as charset=UTF-8.

NOTE: This step will be done by a developer, so you can probably just skip this step.

2. Build language specific translation file from the template
2.a) Initial build

To create an initial translation file for your language, you need to run a command to initialize it based on the template.

Run:
msginit -l $lang -o $lang/LC_MESSAGES/AC.po -i AC.pot

Where $lang is the language token, such as "de" or "de_DE".

2.b) Subsequential update builds

Once the AC.pot template file gets updated, you can merge the new strings into your existing translation file:

Run:
msgmerge -s -U $lang/LC_MESSAGES/AC.po AC.pot

Where $lang is the language token, such as "de" or "de_DE".

3. Translation file compilation

Compile your translation file into a binary format which can then be read by AC.
