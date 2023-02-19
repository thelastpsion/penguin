@echo off
echo *** CREATING INSTALL TREE ***
rem 
rem Create directory structure of the Install tree
rem
deltree /Y INSTALL > NUL:
mkdir INSTALL > NUL:
cd INSTALL > NUL:
mkdir PENGUIN > NUL:
mkdir APP > NUL:
mkdir APP\PENGUIN > NUL:
rem
rem Copy penguin
rem 
copy ..\PENGUIN.APP APP\PENGUIN > NUL:
copy ..\EDISASM.APP APP\PENGUIN > NUL:
rem 
rem And finish back in the root directory
cd ..
echo *** All Done ***
echo *** Now run INSTALL to push it across a COM2:19000 serial link
echo *** to a real Psion, or run 3A, SIENA or WRKA to copy it to
echo *** your emulator directories.

