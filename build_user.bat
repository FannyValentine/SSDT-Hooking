@echo off
setlocal enabledelayedexpansion

::==============================================================================
:: build_user.bat - сборка ssdtmon.exe (user-mode утилита)
:: Компилятор: MinGW GCC
::==============================================================================

title Building SSDT Monitor User Utility

set RED=[91m
set GREEN=[92m
set YELLOW=[93m
set BLUE=[94m
set RESET=[0m

echo %BLUE%========================================================================%RESET%
echo %GREEN%SSDT Integrity Monitor - User Utility Build Script%RESET%
echo %BLUE%========================================================================%RESET%
echo.

::==============================================================================
:: Проверка наличия компилятора
::==============================================================================

echo %YELLOW%[1/4] Checking build environment...%RESET%

:: Проверка наличия gcc
where gcc > nul 2>&1
if %errorlevel% neq 0 (
    echo %RED%[ERROR] GCC not found in PATH!%RESET%
    echo.
    echo Please install MinGW-w64 and add it to PATH:
    echo   1. Download from: https://www.mingw-w64.org/
    echo   2. Add C:\mingw64\bin to system PATH
    echo   3. Restart this script
    pause
    exit /b 1
)

for /f "tokens=*" %%i in ('gcc --version ^| find "gcc"') do set GCC_VERSION=%%i
echo %GREEN%[OK] GCC found: %GCC_VERSION%%RESET%

::==============================================================================
:: Настройка переменных
::==============================================================================

echo.
echo %YELLOW%[2/4] Setting up build variables...%RESET%

set SRC_DIR=user
set OBJ_DIR=obj
set BIN_DIR=bin
set TARGET=%BIN_DIR%\ssdtmon.exe

:: Создание директорий
if not exist %SRC_DIR% (
    echo %RED%[ERROR] Source directory %SRC_DIR% not found!%RESET%
    pause
    exit /b 1
)

if not exist %OBJ_DIR% mkdir %OBJ_DIR%
if not exist %BIN_DIR% mkdir %BIN_DIR%

:: Проверка наличия исходных файлов
if not exist "%SRC_DIR%\main.c" (
    echo %RED%[ERROR] main.c not found in %SRC_DIR%!%RESET%
    pause
    exit /b 1
)

if not exist "%SRC_DIR%\device.c" (
    echo %RED%[ERROR] device.c not found in %SRC_DIR%!%RESET%
    pause
    exit /b 1
)

if not exist "common.h" (
    echo %RED%[ERROR] common.h not found!%RESET%
    pause
    exit /b 1
)

echo   Source directory: %SRC_DIR%
echo   Object directory: %OBJ_DIR%
echo   Output directory: %BIN_DIR%
echo   Target: %TARGET%

::==============================================================================
:: Компиляция
::==============================================================================

echo.
echo %YELLOW%[3/4] Compiling...%RESET%

:: Компиляция main.c
echo   Compiling main.c...
gcc -c "%SRC_DIR%\main.c" -o "%OBJ_DIR%\main.o" -Wall -Wextra -O2 -s -static
if %errorlevel% neq 0 goto :error

:: Компиляция device.c
echo   Compiling device.c...
gcc -c "%SRC_DIR%\device.c" -o "%OBJ_DIR%\device.o" -Wall -Wextra -O2 -s -static
if %errorlevel% neq 0 goto :error

:: Линковка
echo   Linking...
gcc "%OBJ_DIR%\main.o" "%OBJ_DIR%\device.o" -o "%TARGET%" -lwinmm -ladvapi32 -static
if %errorlevel% neq 0 goto :error

::==============================================================================
:: Проверка результата
::==============================================================================

echo.
echo %YELLOW%[4/4] Verifying build...%RESET%

if not exist "%TARGET%" (
    echo %RED%[ERROR] Executable not created!%RESET%
    goto :error
)

:: Получение размера файла
for %%A in ("%TARGET%") do set FILE_SIZE=%%~zA
set /a FILE_SIZE_KB=%FILE_SIZE%/1024

echo %GREEN%[OK] Executable created successfully%RESET%
echo.
echo   File: %TARGET%
echo   Size: %FILE_SIZE_KB% KB

::==============================================================================
:: Завершение
::==============================================================================

echo.
echo %GREEN%========================================================================%RESET%
echo %GREEN%[SUCCESS] User utility build completed!%RESET%
echo %GREEN%========================================================================%RESET%
echo.
echo   Usage examples:
echo     %BIN_DIR%\ssdtmon.exe --help
echo     %BIN_DIR%\ssdtmon.exe --set-hook 0x7F 0xFFFFF80001234567
echo     %BIN_DIR%\ssdtmon.exe --monitor-start 200
echo     %BIN_DIR%\ssdtmon.exe --get-log
echo.
pause
exit /b 0

:error
echo.
echo %RED%========================================================================%RESET%
echo %RED%[ERROR] Build failed!%RESET%
echo %RED%========================================================================%RESET%
echo.
pause
exit /b 1