REM copy a selected subset of the files to the asset directory so that they are available for android
REM do not commit the assets folder to git to avoid redundant files

robocopy "../../bot/" "app/src/main/assets/bot/" /E
robocopy "../../config/" "app/src/main/assets/config/" /E
robocopy "../../packages/" "app/src/main/assets/packages/" /E

REM keep these empty directories
mkdir "app/src/main/assets/demos"
