@echo off
setlocal enabledelayedexpansion

::==============================================================================
:: test.bat - автоматическое тестирование драйвера и утилиты
:: Соответствует ТЗ: разделы 8 (Критерии приёмки), 9 (Пример сценария)
::==============================================================================

title Testing SSDT Monitor Driver

set RED=[91m
set GREEN=[92m
set YELLOW=[93m
set BLUE=[94m
set CYAN=[96m
set RESET=[0m

:: Счётчики тестов
set TESTS_PASSED=0
set TESTS_FAILED=0
set TESTS_TOTAL=0

::==============================================================================
:: Функция: выполнить тест
::==============================================================================
call :print_header "SSDT Integrity Monitor - Test Suite"
echo.

::==============================================================================
:: Тест 1: Проверка наличия файлов
::==============================================================================
call :test_start "Check required files"

set ALL_FILES_EXIST=1
if not exist "bin\ssdtmon.exe" set ALL_FILES_EXIST=0 & echo   Missing: bin\ssdtmon.exe
if not exist "C:\Drivers\SsdtMon.sys" set ALL_FILES_EXIST=0 & echo   Missing: C:\Drivers\SsdtMon.sys

if "%ALL_FILES_EXIST%"=="1" (
    call :test_pass "All required files exist"
) else (
    call :test_fail "Required files missing. Run build scripts first."
)
echo.

::==============================================================================
:: Тест 2: Проверка запуска драйвера
::==============================================================================
call :test_start "Driver is running"

sc query SsdtMon | find "RUNNING" > nul
if %errorlevel% equ 0 (
    call :test_pass "Driver is running"
) else (
    call :test_fail "Driver is not running. Run install.bat as Administrator"
)
echo.

::==============================================================================
:: Тест 3: Проверка утилиты (--help)
::==============================================================================
call :test_start "User utility --help"

bin\ssdtmon.exe --help > nul 2>&1
if %errorlevel% equ 0 (
    call :test_pass "Utility responds to --help"
) else (
    call :test_fail "Utility failed to run"
)
echo.

::==============================================================================
:: Тест 4: Проверка чтения лога (--get-log)
::==============================================================================
call :test_start "Read log buffer"

bin\ssdtmon.exe --get-log > nul 2>&1
if %errorlevel% equ 0 (
    call :test_pass "Log reading works"
) else (
    call :test_fail "Failed to read log"
)
echo.

::==============================================================================
:: Тест 5: Установка тестового хука
::==============================================================================
call :test_start "Install test hook"

:: Используем адрес оригинальной функции как заглушку (не настоящий хук)
bin\ssdtmon.exe --set-hook 0x3F 0xFFFFFFFFFFFFFFFF > test_hook.log 2>&1
find "successfully" test_hook.log > nul
if %errorlevel% equ 0 (
    call :test_pass "Hook installation successful"
) else (
    call :test_fail "Hook installation failed"
)
del test_hook.log 2> nul
echo.

::==============================================================================
:: Тест 6: Проверка списка хуков
::==============================================================================
call :test_start "List active hooks"

bin\ssdtmon.exe --get-hooks > test_hooks.log 2>&1
find "HOOKED" test_hooks.log > nul
if %errorlevel% equ 0 (
    call :test_pass "Hooks listed correctly"
) else (
    call :test_fail "No hooks found in list"
)
del test_hooks.log 2> nul
echo.

::==============================================================================
:: Тест 7: Снятие хука
::==============================================================================
call :test_start "Remove test hook"

bin\ssdtmon.exe --remove-hook 0x3F > test_remove.log 2>&1
find "successfully" test_remove.log > nul
if %errorlevel% equ 0 (
    call :test_pass "Hook removal successful"
) else (
    call :test_fail "Hook removal failed"
)
del test_remove.log 2> nul
echo.

::==============================================================================
:: Тест 8: Запуск мониторинга
::==============================================================================
call :test_start "Start integrity monitor"

bin\ssdtmon.exe --monitor-start 200 > test_monitor.log 2>&1
find "started successfully" test_monitor.log > nul
if %errorlevel% equ 0 (
    call :test_pass "Monitor started"
) else (
    call :test_fail "Failed to start monitor"
)
del test_monitor.log 2> nul
echo.

::==============================================================================
:: Тест 9: Остановка мониторинга
::==============================================================================
call :test_start "Stop integrity monitor"

bin\ssdtmon.exe --monitor-stop > test_stop.log 2>&1
find "stopped successfully" test_stop.log > nul
if %errorlevel% equ 0 (
    call :test_pass "Monitor stopped"
) else (
    call :test_fail "Failed to stop monitor"
)
del test_stop.log 2> nul
echo.

::==============================================================================
:: Тест 10: Восстановление SSDT
::==============================================================================
call :test_start "Restore SSDT"

bin\ssdtmon.exe --restore > test_restore.log 2>&1
find "restored successfully" test_restore.log > nul
if %errorlevel% equ 0 (
    call :test_pass "SSDT restored"
) else (
    call :test_fail "Failed to restore SSDT"
)
del test_restore.log 2> nul
echo.

::==============================================================================
:: Тест 11: Очистка лога
::==============================================================================
call :test_start "Clear log buffer"

bin\ssdtmon.exe --clear-log > test_clear.log 2>&1
find "successfully" test_clear.log > nul
if %errorlevel% equ 0 (
    call :test_pass "Log cleared"
) else (
    call :test_fail "Failed to clear log"
)
del test_clear.log 2> nul
echo.

::==============================================================================
:: Тест 12: Проверка граничных значений (недопустимый индекс)
::==============================================================================
call :test_start "Invalid service index handling"

bin\ssdtmon.exe --set-hook 0xFFFF 0x12345678 > test_invalid.log 2>&1
find "error" test_invalid.log > nul
if %errorlevel% equ 0 (
    call :test_pass "Invalid index rejected"
) else (
    call :test_fail "Invalid index not handled properly"
)
del test_invalid.log 2> nul
echo.

::==============================================================================
:: Итоговый отчёт
::==============================================================================
call :print_summary

::==============================================================================
:: Выход с соответствующим кодом
::==============================================================================
if %TESTS_FAILED% gtr 0 (
    exit /b 1
) else (
    exit /b 0
)

::==============================================================================
:: Функции
::==============================================================================

:test_start
set /a TESTS_TOTAL+=1
set CURRENT_TEST_NAME=%~1
echo %CYAN%[TEST %TESTS_TOTAL%] %CURRENT_TEST_NAME%%RESET%
exit /b

:test_pass
echo   %GREEN%[PASS] %~1%RESET%
set /a TESTS_PASSED+=1
exit /b

:test_fail
echo   %RED%[FAIL] %~1%RESET%
set /a TESTS_FAILED+=1
exit /b

:print_header
echo %BLUE%============================================================%RESET%
echo %BLUE%   %~1%RESET%
echo %BLUE%============================================================%RESET%
exit /b

:print_summary
echo.
echo %BLUE%============================================================%RESET%
echo %BLUE%   TEST SUMMARY%RESET%
echo %BLUE%============================================================%RESET%
echo.
echo   Total tests:  %CYAN%%TESTS_TOTAL%%RESET%
echo   Passed:       %GREEN%%TESTS_PASSED%%RESET%
echo   Failed:       %RED%%TESTS_FAILED%%RESET%
echo.
if %TESTS_FAILED% equ 0 (
    echo %GREEN%============================================================%RESET%
    echo %GREEN%   ALL TESTS PASSED!%RESET%
    echo %GREEN%============================================================%RESET%
) else (
    echo %RED%============================================================%RESET%
    echo %RED%   %TESTS_FAILED% TEST(S) FAILED%RESET%
    echo %RED%============================================================%RESET%
)
echo.
pause
exit /b