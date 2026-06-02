@echo off
setlocal enabledelayedexpansion

::==============================================================================
:: build_driver.bat - сборка драйвера SsdtMon.sys для Windows 10 x64
:: Требования: Visual Studio 2022 + WDK 10
::==============================================================================

title Building SSDT Monitor Driver

set RED=[91m
set GREEN=[92m
set YELLOW=[93m
set BLUE=[94m
set RESET=[0m

echo %BLUE%========================================================================%RESET%
echo %GREEN%SSDT Integrity Monitor - Driver Build Script%RESET%
echo %BLUE%========================================================================%RESET%
echo.

::==============================================================================
:: Проверка окружения
::==============================================================================

echo %YELLOW%[1/6] Checking build environment...%RESET%

:: Проверка наличия Visual Studio
set VS_PATH=
for /f "usebackq tokens=*" %%i in (`where /R "C:\Program Files\Microsoft Visual Studio\2022" msbuild.exe 2^>nul`) do (
    if exist "%%i" set VS_PATH=%%i
    goto :vs_found
)

for /f "usebackq tokens=*" %%i in (`where /R "C:\Program Files (x86)\Microsoft Visual Studio\2019" msbuild.exe 2^>nul`) do (
    if exist "%%i" set VS_PATH=%%i
    goto :vs_found
)

:vs_found
if "%VS_PATH%"=="" (
    echo %RED%[ERROR] Visual Studio 2019/2022 not found!%RESET%
    echo Please install Visual Studio with C++ development tools.
    pause
    exit /b 1
)
echo %GREEN%[OK] Visual Studio found: %VS_PATH%%RESET%

:: Проверка наличия WDK
if not exist "C:\Program Files (x86)\Windows Kits\10\bin" (
    echo %RED%[ERROR] WDK 10 not found!%RESET%
    echo Please install Windows Driver Kit (WDK) version 10.
    pause
    exit /b 1
)
echo %GREEN%[OK] WDK 10 found%RESET%

::==============================================================================
:: Настройка переменных
::==============================================================================

echo.
echo %YELLOW%[2/6] Setting up build variables...%RESET%

set PROJECT_NAME=SsdtMon
set CONFIGURATION=Debug
set PLATFORM=x64
set OUTPUT_DIR=driver\x64\%CONFIGURATION%
set DRIVER_OUTPUT=%OUTPUT_DIR%\%PROJECT_NAME%.sys

:: Создание директории проекта
if not exist driver mkdir driver

echo   Project: %PROJECT_NAME%
echo   Configuration: %CONFIGURATION%
echo   Platform: %PLATFORM%
echo   Output: %OUTPUT_DIR%

::==============================================================================
:: Генерация .vcxproj файла (если не существует)
::==============================================================================

echo.
echo %YELLOW%[3/6] Generating project files...%RESET%

if not exist "driver\%PROJECT_NAME%.vcxproj" (
    echo %YELLOW%Creating Visual Studio project file...%RESET%
    call :generate_vcxproj
) else (
    echo %GREEN%[OK] Project file exists%RESET%
)

::==============================================================================
:: Компиляция драйвера
::==============================================================================

echo.
echo %YELLOW%[4/6] Compiling driver...%RESET%

:: Используем MSBuild для сборки
"%VS_PATH%" driver\%PROJECT_NAME%.vcxproj /p:Configuration=%CONFIGURATION% /p:Platform=%PLATFORM% /p:TargetVersion=Windows10 /p:PreferredToolArchitecture=x64

if %errorlevel% neq 0 (
    echo %RED%[ERROR] Build failed with error code %errorlevel%%RESET%
    pause
    exit /b %errorlevel%
)

if not exist "%DRIVER_OUTPUT%" (
    echo %RED%[ERROR] Driver not created at %DRIVER_OUTPUT%%RESET%
    pause
    exit /b 1
)

echo %GREEN%[OK] Driver compiled successfully%RESET%

::==============================================================================
:: Копирование драйвера в целевую директорию
::==============================================================================

echo.
echo %YELLOW%[5/6] Copying driver to target directory...%RESET%

set TARGET_DIR=C:\Drivers
if not exist "%TARGET_DIR%" mkdir "%TARGET_DIR%"

copy /Y "%DRIVER_OUTPUT%" "%TARGET_DIR%\%PROJECT_NAME%.sys" > nul
if %errorlevel% neq 0 (
    echo %RED%[ERROR] Failed to copy driver to %TARGET_DIR%%RESET%
    pause
    exit /b 1
)

echo %GREEN%[OK] Driver copied to %TARGET_DIR%\%PROJECT_NAME%.sys%RESET%

::==============================================================================
:: Создание INF файла (если нужно)
::==============================================================================

echo.
echo %YELLOW%[6/6] Creating INF file...%RESET%

if not exist "driver\%PROJECT_NAME%.inf" (
    call :generate_inf
) else (
    copy /Y "driver\%PROJECT_NAME%.inf" "%TARGET_DIR%\" > nul
    echo %GREEN%[OK] INF file copied%RESET%
)

::==============================================================================
:: Завершение
::==============================================================================

echo.
echo %GREEN%========================================================================%RESET%
echo %GREEN%[SUCCESS] Driver build completed!%RESET%
echo %GREEN%========================================================================%RESET%
echo.
echo   Driver location: %TARGET_DIR%\%PROJECT_NAME%.sys
echo.
echo   Next steps:
echo   1. Run install.bat as Administrator to install the driver
echo   2. Run test.bat to verify functionality
echo.
pause
exit /b 0

::==============================================================================
:: Функция: генерация .vcxproj файла
::==============================================================================
:generate_vcxproj
(
echo ^<?xml version="1.0" encoding="utf-8"?^>
echo ^<Project DefaultTargets="Build" ToolsVersion="12.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003"^>
echo   ^<ItemGroup Label="ProjectConfigurations"^>
echo     ^<ProjectConfiguration Include="Debug|x64"^>
echo       ^<Configuration^>Debug^</Configuration^>
echo       ^<Platform^>x64^</Platform^>
echo     ^</ProjectConfiguration^>
echo     ^<ProjectConfiguration Include="Release|x64"^>
echo       ^<Configuration^>Release^</Configuration^>
echo       ^<Platform^>x64^</Platform^>
echo     ^</ProjectConfiguration^>
echo   ^</ItemGroup^>
echo   ^<PropertyGroup Label="Globals"^>
echo     ^<ProjectGuid^>{A1B2C3D4-E5F6-7890-AB12-CD34EF567890}^</ProjectGuid^>
echo     ^<TemplateGuid^>{dd38f7fc-d7bd-488b-9242-7d8714c2d6ed}^</TemplateGuid^>
echo     ^<TargetFrameworkVersion^>v4.5^</TargetFrameworkVersion^>
echo     ^<MinimumVisualStudioVersion^>12.0^</MinimumVisualStudioVersion^>
echo     ^<Configuration^>Debug^</Configuration^>
echo     ^<Platform Condition="'$(Platform)'==''"^>x64^</Platform^>
echo     ^<RootNamespace^>SsdtMon^</RootNamespace^>
echo     ^<DriverType^>KMDF^</DriverType^>
echo     ^<DriverTargetPlatform^>Universal^</DriverTargetPlatform^>
echo   ^</PropertyGroup^>
echo   ^<Import Project="$(VCTargetsPath)\Microsoft.Cpp.Default.props" /^>
echo   ^<PropertyGroup Condition="'$(Configuration)|$(Platform)'=='Debug|x64'" Label="Configuration"^>
echo     ^<ConfigurationType^>Driver^</ConfigurationType^>
echo     ^<UseDebugLibraries^>true^</UseDebugLibraries^>
echo     ^<PlatformToolset^>WindowsKernelModeDriver10.0^</PlatformToolset^>
echo     ^<ConfigurationType^>Driver^</ConfigurationType^>
echo   ^</PropertyGroup^>
echo   ^<PropertyGroup Condition="'$(Configuration)|$(Platform)'=='Release|x64'" Label="Configuration"^>
echo     ^<ConfigurationType^>Driver^</ConfigurationType^>
echo     ^<UseDebugLibraries^>false^</UseDebugLibraries^>
echo     ^<PlatformToolset^>WindowsKernelModeDriver10.0^</PlatformToolset^>
echo   ^</PropertyGroup^>
echo   ^<Import Project="$(VCTargetsPath)\Microsoft.Cpp.props" /^>
echo   ^<ImportGroup Label="ExtensionSettings"^>
echo   ^</ImportGroup^>
echo   ^<ImportGroup Label="PropertySheets"^>
echo     ^<Import Project="$(UserRootDir)\Microsoft.Cpp.$(Platform).user.props" Condition="exists('$(UserRootDir)\Microsoft.Cpp.$(Platform).user.props')" Label="LocalAppDataPlatform" /^>
echo   ^</ImportGroup^>
echo   ^<PropertyGroup Label="UserMacros" /^>
echo   ^<PropertyGroup /^>
echo   ^<ItemDefinitionGroup Condition="'$(Configuration)|$(Platform)'=='Debug|x64'"^>
echo     ^<ClCompile^>
echo       ^<PreprocessorDefinitions^>_DEBUG;%(PreprocessorDefinitions)^</PreprocessorDefinitions^>
echo       ^<WarningLevel^>Level3^</WarningLevel^>
echo     ^</ClCompile^>
echo   ^</ItemDefinitionGroup^>
echo   ^<ItemGroup^>
echo     ^<ClCompile Include="main.c" /^>
echo     ^<ClCompile Include="ssdt_access.c" /^>
echo     ^<ClCompile Include="hook_mgr.c" /^>
echo     ^<ClCompile Include="ioctl.c" /^>
echo     ^<ClCompile Include="monitor.c" /^>
echo     ^<ClCompile Include="logger.c" /^>
echo   ^</ItemGroup^>
echo   ^<ItemGroup^>
echo     ^<ClInclude Include="common.h" /^>
echo   ^</ItemGroup^>
echo   ^<Import Project="$(VCTargetsPath)\Microsoft.Cpp.targets" /^>
echo   ^<ImportGroup Label="ExtensionTargets"^>
echo   ^</ImportGroup^>
echo ^</Project^>
) > "driver\%PROJECT_NAME%.vcxproj"
echo %GREEN%[OK] Created %PROJECT_NAME%.vcxproj%RESET%
exit /b

::==============================================================================
:: Функция: генерация INF файла
::==============================================================================
:generate_inf
(
echo ;==============================================================================
echo ; SsdtMon.inf - Driver installation file
echo ;==============================================================================
echo.
echo [Version]
echo Signature="$WINDOWS NT$"
echo Class=System
echo ClassGuid={4d36e97d-e325-11ce-bfc1-08002be10318}
echo Provider=%ManufacturerName%
echo DriverVer=06/01/2024,1.0.0.0
echo CatalogFile=SsdtMon.cat
echo.
echo [Manufacturer]
echo %ManufacturerName%=Standard,NTamd64
echo.
echo [Standard.NTamd64]
echo %DeviceDesc%=SsdtMon_Device, ROOT\SsdtMon
echo.
echo [DestinationDirs]
echo SsdtMon.DriverFiles=12
echo.
echo [DefaultInstall]
echo CopyFiles=SsdtMon.DriverFiles
echo.
echo [DefaultInstall.Services]
echo AddService=SsdtMon,0x00000002,SsdtMon_Service
echo.
echo [SsdtMon_Service]
echo DisplayName=%ServiceDesc%
echo ServiceType=1
echo StartType=3
echo ErrorControl=1
echo ServiceBinary=%12%\SsdtMon.sys
echo.
echo [SsdtMon.DriverFiles]
echo SsdtMon.sys
echo.
echo [SourceDisksFiles]
echo SsdtMon.sys=1
echo.
echo [SourceDisksNames]
echo 1="Driver Disk",,,
echo.
echo [Strings]
echo ManufacturerName="SSDT Monitor Project"
echo DeviceDesc="SSDT Integrity Monitor Driver"
echo ServiceDesc="SSDT Monitor Service"
) > "driver\%PROJECT_NAME%.inf"
echo %GREEN%[OK] Created %PROJECT_NAME%.inf%RESET%
exit /b