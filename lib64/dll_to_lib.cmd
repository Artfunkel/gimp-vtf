@echo off
if not defined VSINSTALLDIR (
	echo ERROR: this script must be run from the Visual Studio Command Prompt.
	pause
	exit /b 1
)
if not exist *.def (
	echo ERROR: no DEF files found. Wrong folder?
	pause
	exit /b 1
)
for %%i in (*.def) do (
	lib /def:%%~ni.def /out:%%~ni.lib /machine:x64 /NOLOGO
	del %%~ni.exp
)
pause