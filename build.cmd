@echo off
rem build.cmd - builds GUICy's test programs into build\ with clang (MinGW target).
rem   build.cmd          gytest.exe and probe_dwrite.exe, plus build\guicy.o (the library alone)
rem   build.cmd crt      the same with GUICY_USE_CRT (GUICy uses the C library's memset/memcpy)
rem The compiler comes from ..\tools\llvm\env.cmd beside this repo, or set GUICY_LLVM_ENV to another env.cmd.
setlocal
if not defined GUICY_LLVM_ENV set "GUICY_LLVM_ENV=%~dp0..\tools\llvm\env.cmd"
if not exist "%GUICY_LLVM_ENV%" (echo build.cmd: no compiler env at %GUICY_LLVM_ENV% & exit /b 1)
call "%GUICY_LLVM_ENV%"
cd /d "%~dp0"
if not exist build mkdir build
set "TMP=%~dp0work\tmp" & set "TEMP=%~dp0work\tmp"
if not exist work\tmp mkdir work\tmp
set W=-std=c11 -O2 -Wall -Wextra -Werror -Wshadow -Wstrict-prototypes
set D=
if /i "%1"=="crt" set D=-DGUICY_USE_CRT
rem the library alone, so its imports and size can be read
clang %CLANG_FLAGS% %W% %D% -x c -DGUICY_IMPLEMENTATION -c -o build\guicy.o guicy.h || exit /b 1
clang %CLANG_FLAGS% %W% %D% -static -o build\gytest.exe tests\gytest.c -lgdi32 || exit /b 1
clang %CLANG_FLAGS% %W% -static -o build\probe_dwrite.exe tests\probe_dwrite.c || exit /b 1
clang %CLANG_FLAGS% %W% %D% -static -mwindows -o build\demo.exe examples\demo.c -lgdi32 -luser32 || exit /b 1
clang %CLANG_FLAGS% %W% -static -o build\gybench.exe bench\gybench.c -lgdi32 -luser32 || exit /b 1
echo built build\gytest.exe build\probe_dwrite.exe build\demo.exe build\gybench.exe build\guicy.o
