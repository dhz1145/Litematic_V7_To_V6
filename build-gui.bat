@echo off
setlocal

set ROOT=%~dp0
set MINGW=C:\Qt\Tools\mingw1310_64\bin
set QT=C:\Qt\6.7.3\mingw_64
set CMAKE=C:\Program Files\CMake\bin\cmake.exe
set BUILD=%ROOT%build-gui

if not exist "%MINGW%\g++.exe" (
  echo [ERROR] MinGW not found: %MINGW%
  echo Install via: aqt install-tool windows desktop tools_mingw1310 --outputdir C:\Qt
  exit /b 1
)
if not exist "%QT%\lib\cmake\Qt6" (
  echo [ERROR] Qt not found: %QT%
  echo Install via: aqt install-qt windows desktop 6.7.3 win64_mingw --outputdir C:\Qt
  exit /b 1
)

set PATH=%MINGW%;%QT%\bin;%PATH%

"%CMAKE%" -S "%ROOT%." -B "%BUILD%" -G Ninja ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_C_COMPILER="%MINGW%\gcc.exe" ^
  -DCMAKE_CXX_COMPILER="%MINGW%\g++.exe" ^
  -DCMAKE_PREFIX_PATH="%QT%" ^
  -DBUILD_GUI=ON
if errorlevel 1 exit /b 1

"%CMAKE%" --build "%BUILD%" --target Litematic_GUI Litematic_V7_To_V6 -j
if errorlevel 1 exit /b 1

"%QT%\bin\windeployqt.exe" --compiler-runtime --release "%BUILD%\Litematic_GUI\Litematic_GUI.exe"
if errorlevel 1 exit /b 1

copy /Y "%MINGW%\libgcc_s_seh-1.dll" "%BUILD%\Litematic_V7_To_V6\" >nul
copy /Y "%MINGW%\libstdc++-6.dll" "%BUILD%\Litematic_V7_To_V6\" >nul
copy /Y "%MINGW%\libwinpthread-1.dll" "%BUILD%\Litematic_V7_To_V6\" >nul

echo.
echo Build OK:
echo   GUI: %BUILD%\Litematic_GUI\Litematic_GUI.exe
echo   CLI: %BUILD%\Litematic_V7_To_V6\Litematic_V7_To_V6.exe
endlocal
