@echo off
echo ========================================
echo Building SsdtMon Driver for Windows 10 x64
echo ========================================
echo.

cd /d D:\VisualStudio\SSDT-Hooking

echo Building driver...
msbuild SsdtMon.sln /p:Configuration=Debug /p:Platform=x64

if %errorlevel% equ 0 (
    echo.
    echo [SUCCESS] Driver built successfully!
    echo Output: D:\VisualStudio\SSDT-Hooking\bin\x64\Debug\SsdtMon.sys
) else (
    echo.
    echo [FAILED] Build failed with error code %errorlevel%
)

pause