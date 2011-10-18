#!/bin/bash
if [ $# -eq 1 ]; then
	LC=$1
	# "assume" LC is valid .. maybe someone "bored" wants to write some checks, but hey, this isn't a user-interface :-P
	msgfmt -c -v -o $LC/LC_MESSAGES/AC.mo $LC/LC_MESSAGES/AC.po
	cp -f $LC/LC_MESSAGES/AC.mo ../../packages/locale/$LC/LC_MESSAGES/AC.mo
else
	echo "pass me a language-code (LC) to compile for (de, it, ...)"
	#echo "you should have prepared ../../packages/locale/__LC__/LC_MESSAGES - where __LC__ is your language code!"
fi

