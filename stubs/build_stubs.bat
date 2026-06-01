@echo off
REM Build the four stub audio DLLs for Carnivores 2 Modder's Edition
REM Requires MSVC x86 (32-bit) toolchain on PATH

set STUBDIR=%~dp0
set OUTDIR=%STUBDIR%..\build\stubs
if not exist "%OUTDIR%" mkdir "%OUTDIR%"

REM ---- Compile once ----
cl /nologo /c /MT /O2 /Fo"%OUTDIR%\audio_stub.obj" "%STUBDIR%audio_stub.c"

if errorlevel 1 goto :error

REM ---- Link four times with different DLL names ----
set DLLS=a_soft a_ds3d a_a3d a_eax

for %%D in (%DLLS%) do (
    echo Building %%D.dll ...
    link /nologo /dll /machine:I386 ^
         /out:"%OUTDIR%\%%D.dll" ^
         /def:"%STUBDIR%audio_stub.def" ^
         "%OUTDIR%\audio_stub.obj"
    if errorlevel 1 goto :error
)

echo.
echo === All stubs built successfully ===
echo Output in: %OUTDIR%
echo.
dir /b "%OUTDIR%\*.dll"
goto :end

:error
echo.
echo *** BUILD FAILED ***
exit /b 1

:end
