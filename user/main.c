/*============================================================================
    main.c - user mode консольная утилита для управления драйвером
    Соответствует ТЗ: раздел 5, пример сценария раздел 9
============================================================================*/

#include <windows.h>
#include <stdio.h>
#include "common.h"

static HANDLE hDevice = INVALID_HANDLE_VALUE;

/*----------------------------------------------------------------------------
    OpenDevice - открытие драйвера
----------------------------------------------------------------------------*/
BOOL OpenDevice(VOID)
{
    hDevice = CreateFileW(
        DEVICE_NAME_SYMBOLIC,
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    
    return (hDevice != INVALID_HANDLE_VALUE);
}

/*----------------------------------------------------------------------------
    CloseDevice - закрытие драйвера
----------------------------------------------------------------------------*/
VOID CloseDevice(VOID)
{
    if (hDevice != INVALID_HANDLE_VALUE) {
        CloseHandle(hDevice);
        hDevice = INVALID_HANDLE_VALUE;
    }
}

/*----------------------------------------------------------------------------
    InstallHook - установка хука (ТЗ раздел 5, IOCTL_SET_HOOK)
----------------------------------------------------------------------------*/
BOOL InstallHook(ULONG ServiceIndex, ULONG64 HookFunction)
{
    HOOK_REQUEST request;
    DWORD bytesReturned;
    BOOL result;
    
    request.ServiceIndex = ServiceIndex;
    request.HookFunction = HookFunction;
    
    result = DeviceIoControl(
        hDevice,
        IOCTL_SSDT_SET_HOOK,
        &request,
        sizeof(request),
        NULL,
        0,
        &bytesReturned,
        NULL);
    
    if (result) {
        printf("[+] Hook installed on index 0x%X\n", ServiceIndex);
    } else {
        printf("[-] Failed to install hook (error: %d)\n", GetLastError());
    }
    
    return result;
}

/*----------------------------------------------------------------------------
    RemoveHook - снятие хука (ТЗ раздел 5, IOCTL_REMOVE_HOOK)
----------------------------------------------------------------------------*/
BOOL RemoveHook(ULONG ServiceIndex)
{
    DWORD bytesReturned;
    BOOL result;
    
    result = DeviceIoControl(
        hDevice,
        IOCTL_SSDT_REMOVE_HOOK,
        &ServiceIndex,
        sizeof(ServiceIndex),
        NULL,
        0,
        &bytesReturned,
        NULL);
    
    if (result) {
        printf("[+] Hook removed from index 0x%X\n", ServiceIndex);
    } else {
        printf("[-] Failed to remove hook (error: %d)\n", GetLastError());
    }
    
    return result;
}

/*----------------------------------------------------------------------------
    StartMonitor - запуск мониторинга (ТЗ раздел 5, IOCTL_MONITOR_START)
----------------------------------------------------------------------------*/
BOOL StartMonitor(ULONG IntervalMs)
{
    DWORD bytesReturned;
    BOOL result;
    
    result = DeviceIoControl(
        hDevice,
        IOCTL_SSDT_MONITOR_START,
        &IntervalMs,
        sizeof(IntervalMs),
        NULL,
        0,
        &bytesReturned,
        NULL);
    
    if (result) {
        printf("[+] Monitor started (interval=%d ms)\n", IntervalMs);
    } else {
        printf("[-] Failed to start monitor (error: %d)\n", GetLastError());
    }
    
    return result;
}

/*----------------------------------------------------------------------------
    StopMonitor - остановка мониторинга (ТЗ раздел 5, IOCTL_MONITOR_STOP)
----------------------------------------------------------------------------*/
BOOL StopMonitor(VOID)
{
    DWORD bytesReturned;
    BOOL result;
    
    result = DeviceIoControl(
        hDevice,
        IOCTL_SSDT_MONITOR_STOP,
        NULL,
        0,
        NULL,
        0,
        &bytesReturned,
        NULL);
    
    if (result) {
        printf("[+] Monitor stopped\n");
    } else {
        printf("[-] Failed to stop monitor (error: %d)\n", GetLastError());
    }
    
    return result;
}

/*----------------------------------------------------------------------------
    GetLog - чтение лога событий (ТЗ раздел 5, IOCTL_GET_LOG)
----------------------------------------------------------------------------*/
BOOL GetLog(VOID)
{
    LOG_ENTRY logBuffer[256];
    DWORD bytesRead;
    BOOL result;
    ULONG i;
    SYSTEMTIME st;
    FILETIME ft;
    
    result = DeviceIoControl(
        hDevice,
        IOCTL_SSDT_GET_LOG,
        NULL,
        0,
        logBuffer,
        sizeof(logBuffer),
        &bytesRead,
        NULL);
    
    if (!result) {
        printf("[-] Failed to read log (error: %d)\n", GetLastError());
        return FALSE;
    }
    
    printf("\n=== SSDT Monitor Log (%d events) ===\n", bytesRead / sizeof(LOG_ENTRY));
    
    for (i = 0; i < bytesRead / sizeof(LOG_ENTRY); i++) {
        // Конвертация времени
        ft.dwLowDateTime = logBuffer[i].Timestamp.LowPart;
        ft.dwHighDateTime = logBuffer[i].Timestamp.HighPart;
        FileTimeToSystemTime(&ft, &st);
        
        printf("[%02d:%02d:%02d] ", st.wHour, st.wMinute, st.wSecond);
        
        switch (logBuffer[i].ChangeType) {
            case CHANGE_TYPE_ADDRESS:
                printf("[ADDR CHANGE] Index 0x%X: 0x%p -> 0x%p\n",
                       logBuffer[i].ServiceIndex,
                       (PVOID)logBuffer[i].ExpectedValue,
                       (PVOID)logBuffer[i].ActualValue);
                break;
            case CHANGE_TYPE_BYTECODE:
                printf("[BYTECODE CHANGE] Index 0x%X: byte mismatch\n",
                       logBuffer[i].ServiceIndex);
                break;
            case CHANGE_TYPE_RESTORED:
                printf("[RESTORED] Index 0x%X: restored to 0x%p\n",
                       logBuffer[i].ServiceIndex,
                       (PVOID)logBuffer[i].ActualValue);
                break;
        }
    }
    
    return TRUE;
}

/*----------------------------------------------------------------------------
    RestoreSSDT - восстановление таблицы (ТЗ раздел 5, IOCTL_RESTORE)
----------------------------------------------------------------------------*/
BOOL RestoreSSDT(VOID)
{
    DWORD bytesReturned;
    BOOL result;
    
    result = DeviceIoControl(
        hDevice,
        IOCTL_SSDT_RESTORE,
        NULL,
        0,
        NULL,
        0,
        &bytesReturned,
        NULL);
    
    if (result) {
        printf("[+] SSDT restored to original\n");
    } else {
        printf("[-] Failed to restore SSDT (error: %d)\n", GetLastError());
    }
    
    return result;
}

/*----------------------------------------------------------------------------
    PrintUsage - справка по командам
----------------------------------------------------------------------------*/
void PrintUsage(void)
{
    printf("\nSSDT Integrity Monitor - User Mode Controller\n");
    printf("Usage: ssdtmon.exe <command> [args]\n\n");
    printf("Commands:\n");
    printf("  --set-hook <index> <address>   Install hook on syscall index\n");
    printf("  --remove-hook <index>          Remove hook from index\n");
    printf("  --monitor-start [ms]           Start integrity monitor (default 200ms)\n");
    printf("  --monitor-stop                 Stop integrity monitor\n");
    printf("  --get-log                      Display log buffer\n");
    printf("  --restore                      Restore original SSDT\n");
    printf("  --clear-log                    Clear log buffer\n");
    printf("  --help                         Show this help\n\n");
    printf("Example (согласно ТЗ раздел 9):\n");
    printf("  ssdtmon.exe --set-hook 0x3F 0xFFFFF80001234567\n");
    printf("  ssdtmon.exe --monitor-start 200\n");
    printf("  ssdtmon.exe --get-log\n");
}

/*----------------------------------------------------------------------------
    main - точка входа
----------------------------------------------------------------------------*/
int main(int argc, char* argv[])
{
    if (argc < 2) {
        PrintUsage();
        return 1;
    }
    
    if (!OpenDevice()) {
        printf("[-] Failed to open driver. Make sure SsdtMon is loaded.\n");
        printf("    Run as Administrator!\n");
        return 1;
    }
    
    if (strcmp(argv[1], "--set-hook") == 0 && argc >= 4) {
        ULONG index = strtoul(argv[2], NULL, 16);
        ULONG64 address = strtoull(argv[3], NULL, 16);
        InstallHook(index, address);
    }
    else if (strcmp(argv[1], "--remove-hook") == 0 && argc >= 3) {
        ULONG index = strtoul(argv[2], NULL, 16);
        RemoveHook(index);
    }
    else if (strcmp(argv[1], "--monitor-start") == 0) {
        ULONG ms = (argc >= 3) ? atoi(argv[2]) : 200;
        StartMonitor(ms);
    }
    else if (strcmp(argv[1], "--monitor-stop") == 0) {
        StopMonitor();
    }
    else if (strcmp(argv[1], "--get-log") == 0) {
        GetLog();
    }
    else if (strcmp(argv[1], "--restore") == 0) {
        RestoreSSDT();
    }
    else if (strcmp(argv[1], "--clear-log") == 0) {
        ClearLog();
    }
    else {
        PrintUsage();
    }
    
    CloseDevice();
    return 0;
}