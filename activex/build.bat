@echo off
REM ═══════════════════════════════════════════════════════════════
REM  slop3d ActiveX Control — Build Script for Visual C++ 6.0
REM ═══════════════════════════════════════════════════════════════

REM Set up VC6 environment (adjust path if needed)
if exist "C:\Program Files\Microsoft Visual Studio\VC98\Bin\vcvars32.bat" (
    call "C:\Program Files\Microsoft Visual Studio\VC98\Bin\vcvars32.bat"
)

echo.
echo Building slop3d ActiveX Control...
echo.

REM Compile all source files
cl /c /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_USRDLL" ^
   slop3d_vc6.cpp slop3d_activex.cpp slop3d_dll.cpp

if errorlevel 1 (
    echo.
    echo *** COMPILE FAILED ***
    goto :end
)

REM Link into DLL
link /DLL /OUT:slop3d.dll /DEF:slop3d_dll.def ^
     slop3d_vc6.obj slop3d_activex.obj slop3d_dll.obj ^
     ole32.lib oleaut32.lib uuid.lib gdi32.lib user32.lib advapi32.lib kernel32.lib

if errorlevel 1 (
    echo.
    echo *** LINK FAILED ***
    goto :end
)

echo.
echo ===============================================================
echo  Build successful! Output: slop3d.dll
echo.
echo  To register:   regsvr32 slop3d.dll
echo  To unregister: regsvr32 /u slop3d.dll
echo ===============================================================

:end
