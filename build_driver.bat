@echo off
:: build_driver.bat - сборка драйвера через WDK

echo ========================================
echo Building SsdtMon Driver for Windows 10 x64
echo ========================================
echo.

:: Путь к WDK (измените под свою установку)
set WDK_PATH=C:\Program Files (x86)\Windows Kits\10

:: Путь к Visual Studio 2026
set VS_PATH=C:\Program Files\Microsoft Visual Studio\2026\Community

:: Настройка окружения для x64
call "%VS_PATH%\VC\Auxiliary\Build\vcvarsall.bat" x64

:: Сборка драйвера через MSBuild
echo Building driver...
msbuild driver\SsdtMon.vcxproj /p:Configuration=Debug /p:Platform=x64

if %errorlevel% equ 0 (
    echo.
    echo [SUCCESS] Driver built successfully!
    echo Output: driver\x64\Debug\SsdtMon.sys
) else (
    echo.
    echo [FAILED] Build failed with error code %errorlevel%
)

pause