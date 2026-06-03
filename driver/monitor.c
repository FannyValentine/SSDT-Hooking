/*============================================================================
    monitor.c - периодическая проверка целостности SSDT (DPC-таймер)
    Соответствует ТЗ: разделы 2.1, 2.4 use-case 2, 9
============================================================================*/

#include <ntddk.h>
#include "common.h"

// Внешние переменные
extern ULONG64* KiServiceTable;
extern ULONG64* GoldenAddressTable;
extern ULONG KiServiceLimit;
extern BOOLEAN MonitoringActive;
extern BOOLEAN AutoRestore;

// Локальные переменные
static KTIMER MonitorTimer;
static KDPC MonitorDpc;
static ULONG MonitorInterval = 200;  // мс, согласно ТЗ раздел 9

// Внешние функции
extern NTSTATUS LogEvent(ULONG ServiceIndex, UCHAR ChangeType, ULONG64 Expected, ULONG64 Actual);
extern NTSTATUS SetSSDTEntry(ULONG Index, ULONG64 Address);

/*----------------------------------------------------------------------------
    CheckSSDTIntegrity - проверка адресов (ТЗ раздел 2.1)
----------------------------------------------------------------------------*/
NTSTATUS CheckSSDTIntegrity(VOID)
{
    ULONG i;
    BOOLEAN changed = FALSE;
    
    for (i = 0; i < KiServiceLimit; i++) {
        ULONG64 current = KiServiceTable[i];
        ULONG64 expected = GoldenAddressTable[i];
        
        if (current != expected) {
            DbgPrint("SsdtMon: [WARNING] SSDT[0x%X] changed: expected 0x%p, current 0x%p\n",
                     i, (PVOID)expected, (PVOID)current);
            
            // Запись в лог (ТЗ раздел 2.2)
            LogEvent(i, CHANGE_TYPE_ADDRESS, expected, current);
            changed = TRUE;
            
            // Автоматическое восстановление (ТЗ раздел 2.4 use-case 3)
            if (AutoRestore) {
                KiServiceTable[i] = expected;
                LogEvent(i, CHANGE_TYPE_RESTORED, current, expected);
                DbgPrint("SsdtMon: Auto-restored index 0x%X\n", i);
            }
        }
    }
    
    return changed ? STATUS_DETECTED : STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------
    CheckBytecodeIntegrity - проверка байт-кода пролога (ТЗ раздел 2.1)
    Обнаружение горячих патчей (inline hooks)
----------------------------------------------------------------------------*/
#define BYTECODE_CHECK_SIZE 8  // Проверяем первые 8 байт функции

static PUCHAR GoldenBytecodeTable = NULL;

NTSTATUS InitBytecodeGolden(VOID)
{
    ULONG i;
    
    GoldenBytecodeTable = (PUCHAR)ExAllocatePoolWithTag(
        NonPagedPoolNx, KiServiceLimit * BYTECODE_CHECK_SIZE, 'tdSS');
    
    if (!GoldenBytecodeTable) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    
    for (i = 0; i < KiServiceLimit; i++) {
        PUCHAR funcAddr = (PUCHAR)KiServiceTable[i];
        if (funcAddr && MmIsAddressValid(funcAddr)) {
            RtlCopyMemory(
                GoldenBytecodeTable + (i * BYTECODE_CHECK_SIZE),
                funcAddr,
                BYTECODE_CHECK_SIZE
            );
        }
    }
    
    return STATUS_SUCCESS;
}

NTSTATUS CheckBytecodeIntegrity(ULONG Index, ULONG64 CurrentAddress)
{
    PUCHAR currentCode = (PUCHAR)CurrentAddress;
    PUCHAR expectedCode = GoldenBytecodeTable + (Index * BYTECODE_CHECK_SIZE);
    ULONG i;
    
    if (!currentCode || !MmIsAddressValid(currentCode)) {
        return STATUS_INVALID_ADDRESS;
    }
    
    __try {
        for (i = 0; i < BYTECODE_CHECK_SIZE; i++) {
            if (currentCode[i] != expectedCode[i]) {
                DbgPrint("SsdtMon: [WARNING] BYTECODE CHANGE at index 0x%X, offset %d\n",
                         Index, i);
                
                LogEvent(Index, CHANGE_TYPE_BYTECODE, 
                        (ULONG64)expectedCode[i], (ULONG64)currentCode[i]);
                return STATUS_DETECTED;
            }
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        return STATUS_ACCESS_VIOLATION;
    }
    
    return STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------
    MonitorWorker - DPC-функция периодической проверки (ТЗ раздел 2.1)
----------------------------------------------------------------------------*/
VOID MonitorWorker(PKDPC Dpc, PVOID Context, PVOID Arg1, PVOID Arg2)
{
    UNREFERENCED_PARAMETER(Dpc);
    UNREFERENCED_PARAMETER(Context);
    UNREFERENCED_PARAMETER(Arg1);
    UNREFERENCED_PARAMETER(Arg2);
    
    if (MonitoringActive) {
        CheckSSDTIntegrity();
        
        // Перезапуск таймера
        LARGE_INTEGER dueTime;
        dueTime.QuadPart = -10000 * (LONGLONG)MonitorInterval;
        KeSetTimerEx(&MonitorTimer, dueTime, MonitorInterval, &MonitorDpc);
    }
}

/*----------------------------------------------------------------------------
    StartMonitor - запуск мониторинга (ТЗ раздел 5)
----------------------------------------------------------------------------*/
NTSTATUS StartMonitor(ULONG IntervalMs)
{
    if (MonitoringActive) {
        return STATUS_ALREADY_COMMITTED;
    }
    
    MonitoringActive = TRUE;
    MonitorInterval = (IntervalMs > 0) ? IntervalMs : 200;
    
    KeInitializeTimer(&MonitorTimer);
    KeInitializeDpc(&MonitorDpc, MonitorWorker, NULL);
    
    LARGE_INTEGER dueTime;
    dueTime.QuadPart = -10000 * (LONGLONG)MonitorInterval;
    KeSetTimerEx(&MonitorTimer, dueTime, MonitorInterval, &MonitorDpc);
    
    DbgPrint("SsdtMon: Monitor started (interval=%d ms)\n", MonitorInterval);
    return STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------
    StopMonitor - остановка мониторинга (ТЗ раздел 5)
----------------------------------------------------------------------------*/
NTSTATUS StopMonitor(VOID)
{
    if (!MonitoringActive) {
        return STATUS_NOT_FOUND;
    }
    
    MonitoringActive = FALSE;
    KeCancelTimer(&MonitorTimer);
    
    DbgPrint("SsdtMon: Monitor stopped\n");
    return STATUS_SUCCESS;
}