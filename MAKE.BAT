@echo off
call checkvid
tsc /zq /m penguin.pr /smain=penguin /%jpivid%
copy penguin.img penguin.app > NUL:
del penguin.img
del penguin.exe
rem edump penguin.app
tsc /zq /m edisasm.pr /smain=edisasm /%jpivid%
copy edisasm.img edisasm.app > NUL:
del edisasm.img
del edisasm.exe
call makeins
