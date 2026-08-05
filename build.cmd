@echo off
rem Build RdpShadow.exe (portable MinGW-w64, no installation required).
rem Toolchain: https://winlibs.com -> extract to C:\CLAUDE\toolchains\mingw64
setlocal
set MINGW=C:\CLAUDE\toolchains\mingw64\bin
if not exist "%MINGW%\gcc.exe" echo MinGW not found at %MINGW% & exit /b 1
if not exist publish mkdir publish

rem ld auto-links this loose object, injecting a duplicate asInvoker manifest
if exist "%MINGW%\..\x86_64-w64-mingw32\lib\default-manifest.o" ^
  ren "%MINGW%\..\x86_64-w64-mingw32\lib\default-manifest.o" default-manifest.o.disabled

rem Regenerate the embedded app icon if it is missing (checked-in otherwise)
if not exist src\app.ico powershell -NoProfile -ExecutionPolicy Bypass -File tools\make-icon.ps1 || exit /b 1

"%MINGW%\windres.exe" src\RdpShadow.rc -O coff -o src\res.o || exit /b 1
"%MINGW%\gcc.exe" src\main.c src\res.o -o publish\RdpShadow.exe ^
  -municode -mwindows -std=gnu11 -Os -s -flto -fno-ident -fno-asynchronous-unwind-tables ^
  -Wall -Wextra -lgdiplus -lgdi32 -luser32 || exit /b 1
del src\res.o

for %%F in (publish\RdpShadow.exe) do echo Built publish\RdpShadow.exe (%%~zF bytes)
