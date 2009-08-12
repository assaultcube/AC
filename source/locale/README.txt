Info:

1. Run update_pot_template to extract all localizable strings from the C++ source and from cubescript. The AC.pot template will be updated.
2. Run de_update_po to migrate those changes into the existing german translation file de/lc_messages/AC.po , edit it to translate new strings
3. Finally run de_compile_mo to compile the po file into a binary format, it will be copied to the packages/locale folder
4. Run AC to see if the strings look okay in the UI
