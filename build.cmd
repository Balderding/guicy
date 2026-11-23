@echo off
rem build.cmd - builds GUICy's test programs into build\ with clang (MinGW target).
rem   build.cmd          gytest.exe and probe_dwrite.exe, plus build\guicy.o (the library alone)
rem   build.cmd crt      the same with GUICY_USE_CRT (GUICy uses the C library's memset/memcpy)
rem   build.cmd examples examples\hello.c and fifteen.c from a bare folder, with clang and with MinGW gcc
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
if /i "%1"=="examples" goto examples
rem the library alone, so its imports and size can be read
clang %CLANG_FLAGS% %W% %D% -x c -DGUICY_IMPLEMENTATION -c -o build\guicy.o guicy.h || exit /b 1
clang %CLANG_FLAGS% %W% %D% -DGUICY_TESTS -static -o build\gytest.exe tests\gytest.c -lgdi32 -luser32 || exit /b 1
clang %CLANG_FLAGS% %W% -static -o build\probe_dwrite.exe tests\probe_dwrite.c || exit /b 1
clang %CLANG_FLAGS% %W% %D% -static -mwindows -o build\demo.exe examples\demo.c -lgdi32 -luser32 || exit /b 1
clang %CLANG_FLAGS% %W% -static -o build\gybench.exe bench\gybench.c -lgdi32 -luser32 || exit /b 1
clang %CLANG_FLAGS% %W% -static -o build\gystress.exe bench\gystress.c -lgdi32 -luser32 -lpsapi || exit /b 1
clang %CLANG_FLAGS% %W% -static -o build\gyspike.exe bench\gyspike.c -ld3d11 -ldxgi -ldxguid -lgdi32 -luser32 || exit /b 1
echo built build\gytest.exe build\probe_dwrite.exe build\demo.exe build\gybench.exe build\gystress.exe build\gyspike.exe build\guicy.o
exit /b 0

:examples
rem each example in a folder with nothing but guicy.h and the .c, so nothing else of this repo can help it build
for %%C in (clang gcc) do (
  if exist work\exbuild\%%C rmdir /s /q work\exbuild\%%C
  mkdir work\exbuild\%%C
  copy /y guicy.h work\exbuild\%%C\ >nul
  copy /y examples\hello.c work\exbuild\%%C\ >nul
  copy /y examples\fifteen.c work\exbuild\%%C\ >nul
)
pushd work\exbuild\clang
for %%F in (hello fifteen) do clang %CLANG_FLAGS% %W% -static -mwindows -o %%F.exe %%F.c -lgdi32 -luser32 || (popd & exit /b 1)
popd
pushd work\exbuild\gcc
for %%F in (hello fifteen) do "%TOOLS_HOME%\gcc\mingw64\bin\gcc.exe" -std=c11 -O2 -Wall -Wextra -Werror -static -mwindows -o %%F.exe %%F.c -lgdi32 -luser32 || (popd & exit /b 1)
popd
echo built work\exbuild\clang and work\exbuild\gcc: hello.exe fifteen.exe
