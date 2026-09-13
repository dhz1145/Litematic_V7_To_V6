@echo off
setlocal enabledelayedexpansion

if "%~1"=="" (
    echo Error: Missing architecture argument. Usage: %~0 [x64^|arm64]
    exit /b 1
)

set "ARCH=%~1"

if /I "%ARCH%"=="x64" (
    set "PLATFORM=x64"
) else if /I "%ARCH%"=="arm64" (
    set "PLATFORM=ARM64"
) else (
    echo Error: Unsupported architecture "%ARCH%". Supported: x64, arm64.
    exit /b 1
)

set "OUTPUT_DIR=.\artifacts\windows-%ARCH%"

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "!VSWHERE!" (
    echo Error: vswhere.exe not found. Please install Visual Studio or add msbuild to PATH.
    exit /b 1
)

set "MSBUILD="
for /f "usebackq delims=" %%i in (`"!VSWHERE!" -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do (
    set "MSBUILD=%%i"
    goto :MSBuildFound
)
:MSBuildFound
if not defined MSBUILD (
    echo Error: No MSBuild found by vswhere.
    exit /b 1
)
echo MSBuild path "%MSBUILD%"

set "DEVENV="
for /f "usebackq delims=" %%i in (`"!VSWHERE!" -latest -find **\devenv.com`) do (
    set "DEVENV=%%i"
    goto :DevenvFound
)
:DevenvFound
if not defined DEVENV (
    echo Error: No devenv.com found by vswhere. Cannot upgrade solution.
    exit /b 1
)
echo Upgrading solution with "%DEVENV%"

"%DEVENV%" Litematic_V7_To_V6.sln /Upgrade
del /Q UpgradeLog*.htm 2>nul

"%MSBUILD%" Litematic_V7_To_V6.sln /p:Configuration=Release /p:Platform=%PLATFORM% /m /t:zlib;xxhash;Litematic_V7_To_V6;NBT_Compare;NBT_Print
if %errorlevel% neq 0 exit /b %errorlevel%

REM 可选：若设置了 QTDIR，再编 GUI（需本机安装 Qt MSVC）
if defined QTDIR (
  "%MSBUILD%" Litematic_V7_To_V6.sln /p:Configuration=Release /p:Platform=%PLATFORM% /m /t:Litematic_GUI
  if !errorlevel! equ 0 (
    if exist ".\%PLATFORM%\Release\Litematic_GUI.exe" (
      if exist "%QTDIR%\bin\windeployqt.exe" (
        "%QTDIR%\bin\windeployqt.exe" --release ".\%PLATFORM%\Release\Litematic_GUI.exe"
      )
    )
  ) else (
    echo Warning: Litematic_GUI build failed, skip GUI artifact.
  )
)

mkdir "%OUTPUT_DIR%"
copy /Y ".\%PLATFORM%\Release\Litematic_V7_To_V6.exe" "%OUTPUT_DIR%\"
copy /Y ".\%PLATFORM%\Release\NBT_Compare.exe" "%OUTPUT_DIR%\"
copy /Y ".\%PLATFORM%\Release\NBT_Print.exe" "%OUTPUT_DIR%\"
if exist ".\%PLATFORM%\Release\Litematic_GUI.exe" (
  xcopy /E /I /Y ".\%PLATFORM%\Release\*.dll" "%OUTPUT_DIR%\" >nul 2>nul
  xcopy /E /I /Y ".\%PLATFORM%\Release\platforms" "%OUTPUT_DIR%\platforms\" >nul 2>nul
  xcopy /E /I /Y ".\%PLATFORM%\Release\styles" "%OUTPUT_DIR%\styles\" >nul 2>nul
  copy /Y ".\%PLATFORM%\Release\Litematic_GUI.exe" "%OUTPUT_DIR%\"
)

