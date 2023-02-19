@echo off
call clean
cd ..
pkzip -urPex penguin.zip -x@penguin\exclude penguin\*.*
echo Stick a ZIP disk in
pause
copy penguin.zip d:

