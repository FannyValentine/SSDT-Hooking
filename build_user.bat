@echo off
:: build_user.bat - сборка консольной утилиты через MinGW

echo ========================================
echo Building ssdtmon.exe (User Mode Utility)
echo ========================================
echo.

:: Проверка наличия gcc
where gcc >nul 2>nul
if %errorlevel% neq 0 (
    echo [ERROR] gcc not found! Make sure MinGW is installed and in PATH.
    pause
    exit /b 1
)

:: Сборка
echo Compiling user/main.c...
gcc -o ssdtmon.exe user/main.c -lwinmm -static

if %errorlevel% equ 0 (
    echo.
    echo [SUCCESS] ssdtmon.exe built successfully!
    echo Output: ssdtmon.exe
) else (
    echo.
    echo [FAILED] Build failed with error code %errorlevel%
)

pause