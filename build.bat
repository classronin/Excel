@echo off
rem Self-contained build: sources, third-party deps, resources and the exe
rem all live under this folder. Nothing above this directory is used.
setlocal
pushd "%~dp0"

set "HERE=%~dp0"
set "SQLITE=%HERE%third_party\sqlite"

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" goto novs
set "VSDIR="
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
if not defined VSDIR goto novs
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 goto fail

rem  Paths here are relative on purpose: the working directory is already
rem  %~dp0 (pushd above). Using "%HERE%" would end in a backslash, and the
rem  closing quote becomes an escaped \" for rc.exe -> RC1107.
if not exist icon.res (
    rc /nologo /i . /fo icon.res icon.rc
    if errorlevel 1 goto fail
)

rem sqlite3.c is stable: compile its object once, not on every UI edit.
if not exist "%HERE%sqlite3.obj" (
    cl /nologo /c /O2 /MT /EHsc /std:c++17 /utf-8 /W3 ^
       /DSQLITE_THREADSAFE=0 /DSQLITE_OMIT_LOAD_EXTENSION ^
       /I "%SQLITE%" /Fo:"%HERE%sqlite3.obj" "%SQLITE%\sqlite3.c"
    if errorlevel 1 goto fail
)

if not exist obj mkdir obj
del /q obj\*.obj >nul 2>&1

for %%f in (state p1_core p2_doc p3_formula p4_image p5_popup p6a_grid_paint p6b_grid_input p7_chrome p8a_commands p8b_app) do (
    cl /nologo /c /O2 /MT /EHsc /std:c++17 /utf-8 /W3 ^
       /I "%SQLITE%" /Fo:obj\%%f.obj src\%%f.cpp
    if errorlevel 1 goto fail
)

link /nologo /SUBSYSTEM:WINDOWS /MANIFEST:EMBED ^
     /OUT:"%HERE%Excel.exe" obj\*.obj "%HERE%sqlite3.obj" "%HERE%icon.res"
if errorlevel 1 goto fail

echo [Excel] build ok -^> "%HERE%Excel.exe"
popd
exit /b 0

:novs
echo [Excel] Visual Studio 2022 C++ toolchain not found.
popd
exit /b 1

:fail
echo [Excel] build failed.
popd
exit /b 1
