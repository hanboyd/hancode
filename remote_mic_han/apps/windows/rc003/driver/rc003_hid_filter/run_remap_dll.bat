@echo off
rem Build the production remap.c as a bare CRT-free DLL for the Python
rem fixture test (tests/remap_fixtures.py loads bin/remap.dll via ctypes).
rem The EWDK ships a kernel-only header/lib set, so no stdio/CRT anywhere:
rem remap.c is pure logic by design.
setlocal
cd /d %~dp0
call "C:\Users\hanboyd\hanboyd-code\remote_mic_han\work\ewdk\BuildEnv\SetupBuildEnv.cmd" >nul 2>&1
call SetupVSEnv >nul 2>&1

rem Use the x64-targeting host tool explicitly; the default cl on PATH in
rem the bare build prompt emits x86 objects.
set CL_X64=%VCToolsInstallDir%bin\Hostx64\x64\cl.exe
set LINK_X64=%VCToolsInstallDir%bin\Hostx64\x64\link.exe

if not exist bin mkdir bin
"%CL_X64%" /nologo /W4 /WX /c /Fo:bin\remap.obj src\remap.c || exit /b 1
"%LINK_X64%" /NOLOGO /DLL /NOENTRY /NODEFAULTLIB /MACHINE:X64 /EXPORT:Rc003RemapReport ^
  /OUT:bin\remap.dll bin\remap.obj || exit /b 1
echo remap.dll built: %~dp0bin\remap.dll
exit /b 0
