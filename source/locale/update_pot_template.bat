REM search C++ source files
xgettext -d AC -o AC.pot --sort-by-file -k_ -C -D ..\src -f translate_cpp_files.txt

REM add script translations to it
xgettext -d AC -o AC.pot --sort-by-file -k_ -j -L Lisp -D ..\..\config -f translate_cubescript_files.txt

pause
