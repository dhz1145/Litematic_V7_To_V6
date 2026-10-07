@echo off
setlocal
set ROOT=%~dp0
set ENIGMA=D:\Enigma Virtual Box\enigmavbconsole.exe
set EVB=%ROOT%project.evb
set GUI_DIR=%ROOT%build-gui\Litematic_GUI

if not exist "%ENIGMA%" (
  echo [ERROR] Enigma console not found: %ENIGMA%
  exit /b 1
)
if not exist "%GUI_DIR%\Litematic_GUI.exe" (
  echo [ERROR] Build GUI first: build-gui.bat
  exit /b 1
)

echo Packing with Enigma Virtual Box...
"%ENIGMA%" "%EVB%"
if errorlevel 1 (
  echo [ERROR] Pack failed
  exit /b 1
)

echo.
echo Output: %ROOT%build-gui\Litematic_GUI_packed.exe
endlocal
