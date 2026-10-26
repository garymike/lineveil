@echo off
rem MSVC build, no CRT. Run from any prompt; sets up the VS environment if needed.
setlocal

where cl >nul 2>nul
if errorlevel 1 (
    for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
    if not defined VSDIR (
        echo Visual Studio with C++ tools not found.
        exit /b 1
    )
    call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul
)

if not exist build mkdir build

cl /nologo /O1 /Os /GS- /Gs9999999 /W4 /WX- /std:c11 ^
   /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /D_WIN32_WINNT=0x0A00 /DWINVER=0x0A00 ^
   /Fobuild\ /Fe:build\scanlite.exe src\*.c ^
   /link /NODEFAULTLIB /ENTRY:entry /SUBSYSTEM:WINDOWS ^
   /DYNAMICBASE /NXCOMPAT /HIGHENTROPYVA /CETCOMPAT /OPT:REF /OPT:ICF /EMITPOGOPHASEINFO /DEBUG:NONE ^
   kernel32.lib user32.lib gdi32.lib shell32.lib

if errorlevel 1 exit /b 1
for %%f in (build\scanlite.exe) do echo Built %%~ff (%%~zf bytes)
