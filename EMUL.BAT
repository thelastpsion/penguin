@echo off

echo Copying Penguin to %emulloc%
mkdir %emulloc%\app >NUL:
mkdir %emulloc%\app\penguin >NUL:
mkdir %emulloc%\penguin >NUL:
xcopy install\*.* %emulloc% /E >NUL:
echo Running emulator
cd %emulloc%
subst M: /D >NUL:
subst A: /D >NUL:
subst B: /D >NUL:
subst A: %emulloc% >NUL:
subst B: %emulloc% >NUL:
subst M: %emulloc% >NUL:
%emul%
subst M: /D >NUL:
subst A: /D >NUL:
subst B: /D >NUL:
rem
rem You may want to adjust this directory....
rem
cd \matt\slinux\penguin

