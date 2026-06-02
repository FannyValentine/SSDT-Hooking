@echo off
setlocal enabledelayedexpansion

::==============================================================================
:: install.bat - установка и запуск драйвера SsdtMon.sys
:: Требует прав администратора
::==============================================================================

title Installing SSDT Monitor Driver

set RED=[91m
set GREEN=[92m
set YELLOW=[93m
set BLUE=[94m
set RESET=[0m

:: Проверка прав администратора
net session > nul 2>&1
if %errorlevel% neq 0 (
    echo %RED%[ERROR] This script requires Administrator privileges!%RESET%
    echo.
    echo Please right-click and select "Run as Administrator"
    pause
    exit /b 1
)

echo %BLUE%========================================================================%RESET%
echo %GREEN%SSDT Integrity Monitor - Driver Installation%RESET%
echo %BLUE%========================================================================%RESET%
echo.

::==============================================================================
:: Конфигурация
::==============================================================================

set DRIVER_NAME=SsdtMon
set DRIVER_PATH=C:\Drivers\%DRIVER_NAME%.sys
set SERVICE_NAME=SsdtMon

::==============================================================================
:: Шаг 1: Копирование драйвера
::==============================================================================

echo %YELLOW%[1/6] Copying driver files...%RESET%

if not exist "bin\%DRIVER_NAME%.sys" (
    if not exist "%DRIVER_PATH%" (
        echo %RED%[ERROR] Driver file not found!%RESET%
        echo.
        echo Please run build_driver.bat first to compile the driver.
        pause
        exit /b 1
    )
    echo %GREEN%[OK] Driver found at %DRIVER_PATH%%RESET%
) else (
    if not exist "C:\Drivers" mkdir "C:\Drivers"
    copy /Y "bin\%DRIVER_NAME%.sys" "%DRIVER_PATH%" > nul
    echo %GREEN%[OK] Driver copied to %DRIVER_PATH%%RESET%
)

::==============================================================================
:: Шаг 2: Остановка существующей службы (если есть)
::==============================================================================

echo.
echo %YELLOW%[2/6] Stopping existing service...%RESET%

sc query %SERVICE_NAME% > nul 2>&1
if %errorlevel% equ 0 (
    sc stop %SERVICE_NAME% > nul 2>&1
    timeout /t 2 /nobreak > nul
    echo %GREEN%[OK] Service stopped%RESET%
) else (
    echo %GREEN%[OK] Service not running%RESET%
)

::==============================================================================
:: Шаг 3: Удаление существующей службы
::==============================================================================

echo.
echo %YELLOW%[3/6] Removing existing service...%RESET%

sc delete %SERVICE_NAME% > nul 2>&1
timeout /t 1 /nobreak > nul
echo %GREEN%[OK] Service removed%RESET%

::==============================================================================
:: Шаг 4: Создание новой службы
::==============================================================================

echo.
echo %YELLOW%[4/6] Creating service...%RESET%

sc create %SERVICE_NAME% type= kernel start= demand binPath= "%DRIVER_PATH%" DisplayName= "SSDT Integrity Monitor"
if %errorlevel% neq 0 (
    echo %RED%[ERROR] Failed to create service!%RESET%
    pause
    exit /b 1
)
echo %GREEN%[OK] Service created successfully%RESET%

::==============================================================================
:: Шаг 5: Запуск драйвера
::==============================================================================

echo.
echo %YELLOW%[5/6] Starting driver...%RESET%

sc start %SERVICE_NAME%
if %errorlevel% neq 0 (
    echo %RED%[ERROR] Failed to start driver!%RESET%
    echo.
    echo Possible reasons:
    echo   1. Driver not signed (enable test mode)
    echo   2. Missing dependencies
    echo   3. Incompatible Windows version
    echo.
    echo To enable test signing mode:
    echo   bcdedit /set testsigning on
    echo   reboot
    pause
    exit /b 1
)
echo %GREEN%[OK] Driver started successfully%RESET%

::==============================================================================
:: Шаг 6: Проверка статуса
::==============================================================================

echo.
echo %YELLOW%[6/6] Verifying driver status...%RESET%

echo.
sc query %SERVICE_NAME%
echo.

:: Проверка через утилиту
if exist "bin\ssdtmon.exe" (
    echo Testing connection with driver...
    bin\ssdtmon.exe --get-log > nul 2>&1
    if %errorlevel% equ 0 (
        echo %GREEN%[OK] Driver responding to commands%RESET%
    ) else (
        echo %YELLOW%[WARN] Driver not responding to commands%RESET%
    )
)

::==============================================================================
:: Завершение
::==============================================================================

echo.
echo %GREEN%========================================================================%RESET%
echo %GREEN%[SUCCESS] Driver installation completed!%RESET%
echo %GREEN%========================================================================%RESET%
echo.
echo   Service Name: %SERVICE_NAME%
echo   Driver Path: %DRIVER_PATH%
echo.
echo   Management commands:
echo     sc query %SERVICE_NAME%     - Check status
echo     sc stop %SERVICE_NAME%      - Stop driver
echo     sc start %SERVICE_NAME%     - Start driver
echo     sc delete %SERVICE_NAME%    - Remove service
echo.
echo   User utility commands:
echo     ssdtmon.exe --help
echo     ssdtmon.exe --set-hook 0x7F 0xADDRESS
echo     ssdtmon.exe --monitor-start 200
echo     ssdtmon.exe --get-log
echo.
pause