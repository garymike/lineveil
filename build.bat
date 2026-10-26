@echo off
rem MSVC build, no CRT.
rem   build.bat            -> build\scanlite-x64.exe
rem   build.bat x86        -> build\scanlite-x86.exe
rem   build.bat arm64      -> build\scanlite-arm64.exe
rem Sets up the Visual Studio environment for the target automatically.
setlocal

set "ARCH=%~1"
if "%ARCH%"=="" set "ARCH=x64"

if /i "%ARCH%"=="x64" (
    set "VCARGS=x64"
    set "ARCH_LINK=/HIGHENTROPYVA /CETCOMPAT"
) else if /i "%ARCH%"=="x86" (
    set "VCARGS=x64_x86"
    set "ARCH_LINK=/LARGEADDRESSAWARE /SAFESEH:NO /CETCOMPAT"
) else if /i "%ARCH%"=="arm64" (
    set "VCARGS=x64_arm64"
    set "ARCH_LINK=/HIGHENTROPYVA"
) else (
    echo Unknown arch "%ARCH%". Use x64, x86, or arm64.
    exit /b 1
)

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
if not defined VSDIR (
    echo Visual Studio with C++ tools not found.
    exit /b 1
)
call "%VSDIR%\VC\Auxiliary\Build\vcvarsall.bat" %VCARGS% >nul || exit /b 1

if not exist build\obj-%ARCH% mkdir build\obj-%ARCH%

rem /SAFESEH:NO on x86: we have no exception handlers and no CRT, so there is
rem no SEH table to register. CET and ASLR still apply.
cl /nologo /O1 /Os /GS- /Gs9999999 /W4 /std:c11 ^
   /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /D_WIN32_WINNT=0x0A00 /DWINVER=0x0A00 ^
   /Fobuild\obj-%ARCH%\ /Fe:build\scanlite-%ARCH%.exe src\*.c ^
   /link /NODEFAULTLIB /ENTRY:entry /SUBSYSTEM:WINDOWS ^
   /DYNAMICBASE /NXCOMPAT %ARCH_LINK% /OPT:REF /OPT:ICF /DEBUG:NONE ^
   kernel32.lib user32.lib gdi32.lib shell32.lib

if errorlevel 1 exit /b 1
for %%f in (build\scanlite-%ARCH%.exe) do echo Built %%~ff (%%~zf bytes)
