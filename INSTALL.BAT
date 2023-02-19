@echo off
echo *** PENGUIN PSION-INSTALL ***
echo *** Turn on the Psion link with Psion-L; 
echo *** Set it to 19200 baud...
pause
copy install.txt install
copy mclink.trm install
cd install
mclink @install.txt
cd ..
echo *** All Done ***
