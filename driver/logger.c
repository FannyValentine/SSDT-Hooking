/*============================================================================
    logger.c - кольцевой буфер для событий (64KB, перезапись)
    Соответствует ТЗ: разделы 2.2, 3
============================================================================*/

#include <ntddk.h>
#include "common.h"

#define LOG_BUFFER_SIZE (64 * 1024)   // 64KB согласно ТЗ раздел 3
#define MAX_LOG_ENTRIES (LOG_BUFFER_SIZE / sizeof(LOG_ENTRY))

static PLOG_ENTRY LogBuffer = NULL;
static volatile LONG LogHead = 0;
static volatile LONG LogTail = 0;
static KSPIN_LOCK LogLock;

/*----------------------------------------------------------------------------
    InitLogger - инициализация буфера лога
----------------------------------------------------------------------------*/
NTSTATUS InitLogger(VOID)
{
    LogBuffer = (PLOG_ENTRY)ExAllocatePoolWithTag(
        NonPagedPoolNx, LOG_BUFFER_SIZE, 'tdSS');
    
    if (!LogBuffer) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    
    RtlZeroMemory(LogBuffer, LOG_BUFFER_SIZE);
    KeInitializeSpinLock(&LogLock);
    
    return STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------
    LogEvent - запись события в лог (ТЗ раздел 2.2)
----------------------------------------------------------------------------*/
NTSTATUS LogEvent(ULONG ServiceIndex, UCHAR ChangeType, ULONG64 Expected, ULONG64 Actual)
{
    KIRQL oldIrql;
    LONG nextTail;
    PLOG_ENTRY entry;
    
    if (!LogBuffer) {
        return STATUS_NOT_FOUND;
    }
    
    KeAcquireSpinLock(&LogLock, &oldIrql);
    
    nextTail = (LogTail + 1) % MAX_LOG_ENTRIES;
    
    // Перезапись при заполнении (кольцевой буфер)
    if (nextTail == LogHead) {
        LogHead = (LogHead + 1) % MAX_LOG_ENTRIES;
    }
    
    entry = &LogBuffer[LogTail];
    KeQuerySystemTime(&entry->Timestamp);
    entry->ServiceIndex = ServiceIndex;
    entry->ChangeType = ChangeType;
    entry->ExpectedValue = Expected;
    entry->ActualValue = Actual;
    entry->WasRestored = (ChangeType == CHANGE_TYPE_RESTORED);
    
    LogTail = nextTail;
    
    KeReleaseSpinLock(&LogLock, oldIrql);
    
    return STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------
    ReadLog - чтение лога пользовательской программой (ТЗ раздел 5)
----------------------------------------------------------------------------*/
NTSTATUS ReadLog(PVOID OutBuffer, ULONG OutSize, PULONG BytesRead)
{
    KIRQL oldIrql;
    ULONG count = 0;
    LONG i = LogHead;
    PLOG_ENTRY outEntry = (PLOG_ENTRY)OutBuffer;
    ULONG maxEntries = OutSize / sizeof(LOG_ENTRY);
    
    if (!LogBuffer || !OutBuffer) {
        return STATUS_INVALID_PARAMETER;
    }
    
    KeAcquireSpinLock(&LogLock, &oldIrql);
    
    while (i != LogTail && count < maxEntries) {
        RtlCopyMemory(&outEntry[count], &LogBuffer[i], sizeof(LOG_ENTRY));
        count++;
        i = (i + 1) % MAX_LOG_ENTRIES;
    }
    
    KeReleaseSpinLock(&LogLock, oldIrql);
    
    if (BytesRead) {
        *BytesRead = count * sizeof(LOG_ENTRY);
    }
    
    return STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------
    ClearLog - очистка лога (ТЗ раздел 5)
----------------------------------------------------------------------------*/
NTSTATUS ClearLog(VOID)
{
    KIRQL oldIrql;
    
    if (!LogBuffer) {
        return STATUS_NOT_FOUND;
    }
    
    KeAcquireSpinLock(&LogLock, &oldIrql);
    RtlZeroMemory(LogBuffer, LOG_BUFFER_SIZE);
    LogHead = 0;
    LogTail = 0;
    KeReleaseSpinLock(&LogLock, oldIrql);
    
    return STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------
    FreeLogger - освобождение буфера лога
----------------------------------------------------------------------------*/
VOID FreeLogger(VOID)
{
    if (LogBuffer) {
        ExFreePoolWithTag(LogBuffer, 'tdSS');
        LogBuffer = NULL;
    }
}