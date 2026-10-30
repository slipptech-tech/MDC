
@echo off
setlocal EnableExtensions
title MDC Build System

cd /d "%~dp0"

echo.
echo ==========================================
echo              MDC BUILD SYSTEM
echo ==========================================
echo.

if not exist "main.cpp" (
    echo [ERROR] main.cpp not found.
    pause
    exit /b 1
)

if not exist "mdc\theme.h" (
    echo [ERROR] mdc\theme.h not found.
    pause
    exit /b 1
)

if not exist "mdc\animation.h" (
    echo [ERROR] mdc\animation.h not found.
    pause
    exit /b 1
)

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"

if not exist "%VSWHERE%" (
    echo [ERROR] Visual Studio Installer not found.
    pause
    exit /b 1
)

for /f "usebackq delims=" %%I in (`"%VSWHERE%" -latest -products * -property installationPath`) do (
    set "VS_PATH=%%I"
)

if not defined VS_PATH (
    echo [ERROR] Visual Studio installation not found.
    pause
    exit /b 1
)

call "%VS_PATH%\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64

if errorlevel 1 (
    echo [ERROR] MSVC initialization failed.
    pause
    exit /b 1
)

where cl >nul 2>nul
if errorlevel 1 (
    echo [ERROR] C++ compiler not found.
    pause
    exit /b 1
)

set "RESOURCE="

if exist "MDC.ico" (
    echo #include ^<windows.h^> > MDC.rc
    echo 101 ICON "MDC.ico" >> MDC.rc

    rc /nologo /fo MDC.res MDC.rc

    if errorlevel 1 (
        echo [ERROR] Icon compilation failed.
        pause
        exit /b 1
    )

    set "RESOURCE=MDC.res"
)

if exist "MDC.exe" del /q "MDC.exe"
if exist "main.obj" del /q "main.obj"

echo.
echo [INFO] Building MDC...
echo.

cl /nologo /std:c++17 /EHsc /O2 /DUNICODE /D_UNICODE ^
    /I. ^
    main.cpp %RESOURCE% ^
    /link ^
    /SUBSYSTEM:WINDOWS ^
    user32.lib ^
    gdi32.lib ^
    gdiplus.lib ^
    shell32.lib ^
    comdlg32.lib ^
    comctl32.lib ^
    advapi32.lib ^
    ole32.lib ^
    oleaut32.lib ^
    uuid.lib ^
    /OUT:MDC.exe

if errorlevel 1 (
    echo.
    echo [ERROR] Build failed.
    pause
    exit /b 1
)

if not exist "MDC.exe" (
    echo [ERROR] MDC.exe was not created.
    pause
    exit /b 1
)

echo.
echo ==========================================
echo              BUILD SUCCESS
echo ==========================================
echo.
echo Output: %CD%\MDC.exe
echo.

choice /c YN /n /m "Launch MDC now? [Y/N]: "

if errorlevel 2 goto :finish

start "" "%CD%\MDC.exe"

:finish
echo.
pause
endlocal
