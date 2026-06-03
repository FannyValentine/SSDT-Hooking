@echo off
:: install.bat - установка и запуск драйвера

echo ========================================
echo Installing SsdtMon Driver
echo ========================================
echo.

:: Проверка прав администратора
net session >nul 2>nul
if %errorlevel% neq 0 (
    echo [ERROR] Please run this script as Administrator!
    pause
    exit /b 1
)

:: Копирование драйвера в system32\drivers
echo Copying driver...
copy /Y driver\x64\Debug\SsdtMon.sys C:\Windows\System32\drivers\ >nul

:: Создание службы
echo Creating service...
sc create SsdtMon type= kernel binPath= C:\Windows\System32\drivers\SsdtMon.sys start= demand

:: Запуск драйвера
echo Starting driver...
sc start SsdtMon

:: Проверка статуса
echo.
sc query SsdtMon

echo.
echo [SUCCESS] Driver installed and started.
echo Use 'sc stop SsdtMon' to stop.
echo Use 'sc delete SsdtMon' to remove.

pause