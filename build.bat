@echo off
rem MSVC build, no C/C++ runtime.
rem   build.bat            -> build\lineveil-x64.exe
rem   build.bat x86        -> build\lineveil-x86.exe
rem   build.bat arm64      -> build\lineveil-arm64.exe
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

set "OBJ=build\obj-%ARCH%"
if not exist "%OBJ%" mkdir "%OBJ%"
del /q "%OBJ%\*.obj" "%OBJ%\*.res" 2>nul

set "COMMON=/nologo /c /O1 /Os /GS- /Gs9999999 /W4 /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /D_WIN32_WINNT=0x0A00 /DWINVER=0x0A00 /Fo%OBJ%\"

cl %COMMON% /std:c11 src\*.c || exit /b 1
rem C++ only for the DComp renderer (dcomp.h is C++ only). No exceptions,
rem no RTTI, no thread-safe statics: nothing that needs the C++ runtime.
cl %COMMON% /std:c++14 /GR- /EHs-c- /Zc:threadSafeInit- src\*.cpp || exit /b 1
rc /nologo /i res /fo "%OBJ%\lineveil.res" res\lineveil.rc || exit /b 1

rem /SAFESEH:NO on x86: no exception handlers and no CRT, so there is no SEH
rem table to register. CET and ASLR still apply.
link /nologo "%OBJ%\*.obj" "%OBJ%\lineveil.res" /OUT:build\lineveil-%ARCH%.exe ^
   /NODEFAULTLIB /ENTRY:entry /SUBSYSTEM:WINDOWS ^
   /DYNAMICBASE /NXCOMPAT %ARCH_LINK% /OPT:REF /OPT:ICF /DEBUG:NONE ^
   kernel32.lib user32.lib gdi32.lib shell32.lib || exit /b 1

for %%f in (build\lineveil-%ARCH%.exe) do echo Built %%~ff (%%~zf bytes)
