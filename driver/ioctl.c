/*============================================================================
    ioctl.c - обработка IOCTL команд из user mode
    Соответствует ТЗ: разделы 2, 4.3, 5
============================================================================*/

#include <ntddk.h>
#include "common.h"

// Внешние функции и переменные
extern ULONG64* KiServiceTable;
extern ULONG64* GoldenAddressTable;
extern ULONG64* HookTable;
extern ULONG KiServiceLimit;
extern BOOLEAN MonitoringActive;
extern ULONG MonitorInterval;
extern BOOLEAN AutoRestore;
extern PDEVICE_OBJECT g_DeviceObject;

// Прототипы функций из других модулей
NTSTATUS InstallHook(ULONG ServiceIndex, ULONG64 HookFunction);
NTSTATUS RemoveHook(ULONG ServiceIndex);
NTSTATUS StartMonitor(ULONG IntervalMs);
NTSTATUS StopMonitor(VOID);
NTSTATUS RestoreOriginalSSDT(VOID);
NTSTATUS ReadLog(PVOID OutBuffer, ULONG OutSize, PULONG BytesRead);
NTSTATUS ClearLog(VOID);
NTSTATUS LogEvent(ULONG ServiceIndex, UCHAR ChangeType, ULONG64 Expected, ULONG64 Actual);

/*----------------------------------------------------------------------------
    HandleGetHooks - возвращает список активных хуков
    Соответствует ТЗ: раздел 5, IOCTL_SSDT_GET_HOOKS
----------------------------------------------------------------------------*/
NTSTATUS HandleGetHooks(PIRP Irp)
{
    ULONG i;
    ULONG hookCount = 0;
    PHOOK_INFO hookInfo = (PHOOK_INFO)Irp->AssociatedIrp.SystemBuffer;
    ULONG maxHooks = Irp->IoStatus.Information / sizeof(HOOK_INFO);
    KIRQL oldIrql;
    
    if (!hookInfo || maxHooks == 0) {
        return STATUS_INVALID_PARAMETER;
    }
    
    // Захватываем спин-блокировку для безопасного доступа к таблицам
    KeRaiseIrql(DISPATCH_LEVEL, &oldIrql);
    
    for (i = 0; i < KiServiceLimit && hookCount < maxHooks; i++) {
        if (HookTable[i] != 0) {
            hookInfo[hookCount].ServiceIndex = i;
            hookInfo[hookCount].OriginalAddress = GoldenAddressTable[i];
            hookInfo[hookCount].CurrentAddress = KiServiceTable[i];
            hookInfo[hookCount].IsHooked = TRUE;
            hookCount++;
        }
    }
    
    KeLowerIrql(oldIrql);
    
    Irp->IoStatus.Information = hookCount * sizeof(HOOK_INFO);
    return STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------
    HandleSetHook - устанавливает хук на системный вызов
    Соответствует ТЗ: разделы 2.3, 5, IOCTL_SSDT_SET_HOOK
----------------------------------------------------------------------------*/
NTSTATUS HandleSetHook(PIRP Irp)
{
    PHOOK_REQUEST request = (PHOOK_REQUEST)Irp->AssociatedIrp.SystemBuffer;
    NTSTATUS status;
    
    if (!request) {
        return STATUS_INVALID_PARAMETER;
    }
    
    // Проверка валидности адреса хук-функции
    if (!request->HookFunction || 
        !MmIsAddressValid((PVOID)request->HookFunction)) {
        return STATUS_INVALID_PARAMETER;
    }
    
    // Устанавливаем хук
    status = InstallHook(request->ServiceIndex, request->HookFunction);
    
    if (NT_SUCCESS(status)) {
        DbgPrint("SsdtMon: IOCTL_SET_HOOK success for index 0x%X\n", 
                 request->ServiceIndex);
        LogEvent(request->ServiceIndex, CHANGE_TYPE_ADDRESS,
                 GoldenAddressTable[request->ServiceIndex],
                 request->HookFunction);
    }
    
    return status;
}

/*----------------------------------------------------------------------------
    HandleRemoveHook - снимает хук с системного вызова
    Соответствует ТЗ: разделы 2.3, 5, IOCTL_SSDT_REMOVE_HOOK
----------------------------------------------------------------------------*/
NTSTATUS HandleRemoveHook(PIRP Irp)
{
    PULONG index = (PULONG)Irp->AssociatedIrp.SystemBuffer;
    NTSTATUS status;
    
    if (!index) {
        return STATUS_INVALID_PARAMETER;
    }
    
    status = RemoveHook(*index);
    
    if (NT_SUCCESS(status)) {
        DbgPrint("SsdtMon: IOCTL_REMOVE_HOOK success for index 0x%X\n", *index);
    }
    
    return status;
}

/*----------------------------------------------------------------------------
    HandleMonitorStart - запускает периодический мониторинг
    Соответствует ТЗ: разделы 2.1, 5, IOCTL_SSDT_MONITOR_START
----------------------------------------------------------------------------*/
NTSTATUS HandleMonitorStart(PIRP Irp)
{
    PULONG interval = (PULONG)Irp->AssociatedIrp.SystemBuffer;
    ULONG intervalMs;
    
    if (interval && Irp->IoStatus.Information >= sizeof(ULONG)) {
        intervalMs = *interval;
        DbgPrint("SsdtMon: Starting monitor with custom interval %d ms\n", intervalMs);
    } else {
        intervalMs = 200;  // Значение по умолчанию из ТЗ раздел 9
        DbgPrint("SsdtMon: Starting monitor with default interval 200 ms\n");
    }
    
    return StartMonitor(intervalMs);
}

/*----------------------------------------------------------------------------
    HandleMonitorStop - останавливает периодический мониторинг
    Соответствует ТЗ: разделы 2.1, 5, IOCTL_SSDT_MONITOR_STOP
----------------------------------------------------------------------------*/
NTSTATUS HandleMonitorStop(PIRP Irp)
{
    UNREFERENCED_PARAMETER(Irp);
    return StopMonitor();
}

/*----------------------------------------------------------------------------
    HandleGetLog - возвращает буфер лога событий
    Соответствует ТЗ: разделы 2.2, 5, IOCTL_SSDT_GET_LOG
----------------------------------------------------------------------------*/
NTSTATUS HandleGetLog(PIRP Irp)
{
    PVOID buffer = Irp->AssociatedIrp.SystemBuffer;
    ULONG bufferSize = Irp->IoStatus.Information;
    ULONG bytesRead;
    NTSTATUS status;
    
    if (!buffer || bufferSize == 0) {
        return STATUS_INVALID_PARAMETER;
    }
    
    status = ReadLog(buffer, bufferSize, &bytesRead);
    
    if (NT_SUCCESS(status)) {
        Irp->IoStatus.Information = bytesRead;
    }
    
    return status;
}

/*----------------------------------------------------------------------------
    HandleRestore - восстанавливает оригинальный SSDT
    Соответствует ТЗ: разделы 2.4 (use-case 3), 5, IOCTL_SSDT_RESTORE
----------------------------------------------------------------------------*/
NTSTATUS HandleRestore(PIRP Irp)
{
    UNREFERENCED_PARAMETER(Irp);
    return RestoreOriginalSSDT();
}

/*----------------------------------------------------------------------------
    HandleClearLog - очищает буфер лога
    Соответствует ТЗ: раздел 5, IOCTL_SSDT_CLEAR_LOG
----------------------------------------------------------------------------*/
NTSTATUS HandleClearLog(PIRP Irp)
{
    UNREFERENCED_PARAMETER(Irp);
    return ClearLog();
}

/*----------------------------------------------------------------------------
    HandleUnknownIoctl - обработка неизвестных IOCTL команд
    Соответствует ТЗ: требования безопасности
----------------------------------------------------------------------------*/
NTSTATUS HandleUnknownIoctl(PIRP Irp)
{
    DbgPrint("SsdtMon: Unknown IOCTL code received\n");
    return STATUS_INVALID_DEVICE_REQUEST;
}

/*----------------------------------------------------------------------------
    DeviceControl - основной диспетчер IOCTL команд
    Соответствует ТЗ: раздел 5
----------------------------------------------------------------------------*/
NTSTATUS DeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    PIO_STACK_LOCATION irpStack;
    NTSTATUS status = STATUS_SUCCESS;
    ULONG ioControlCode;
    ULONG inputLength;
    ULONG outputLength;
    
    UNREFERENCED_PARAMETER(DeviceObject);
    
    // Получаем информацию о запросе
    irpStack = IoGetCurrentIrpStackLocation(Irp);
    ioControlCode = irpStack->Parameters.DeviceIoControl.IoControlCode;
    inputLength = irpStack->Parameters.DeviceIoControl.InputBufferLength;
    outputLength = irpStack->Parameters.DeviceIoControl.OutputBufferLength;
    
    // Сохраняем размер выходного буфера для обработчиков
    Irp->IoStatus.Information = outputLength;
    
    DbgPrint("SsdtMon: IOCTL received: 0x%X (Input: %d, Output: %d)\n",
             ioControlCode, inputLength, outputLength);
    
    // Диспетчеризация по коду IOCTL
    switch (ioControlCode) {
        case IOCTL_SSDT_GET_HOOKS:
            status = HandleGetHooks(Irp);
            break;
            
        case IOCTL_SSDT_SET_HOOK:
            if (inputLength < sizeof(HOOK_REQUEST)) {
                DbgPrint("SsdtMon: Invalid input buffer size for SET_HOOK\n");
                status = STATUS_BUFFER_TOO_SMALL;
                break;
            }
            status = HandleSetHook(Irp);
            break;
            
        case IOCTL_SSDT_REMOVE_HOOK:
            if (inputLength < sizeof(ULONG)) {
                DbgPrint("SsdtMon: Invalid input buffer size for REMOVE_HOOK\n");
                status = STATUS_BUFFER_TOO_SMALL;
                break;
            }
            status = HandleRemoveHook(Irp);
            break;
            
        case IOCTL_SSDT_MONITOR_START:
            status = HandleMonitorStart(Irp);
            break;
            
        case IOCTL_SSDT_MONITOR_STOP:
            status = HandleMonitorStop(Irp);
            break;
            
        case IOCTL_SSDT_GET_LOG:
            status = HandleGetLog(Irp);
            break;
            
        case IOCTL_SSDT_RESTORE:
            status = HandleRestore(Irp);
            break;
            
        case IOCTL_SSDT_CLEAR_LOG:
            status = HandleClearLog(Irp);
            break;
            
        default:
            status = HandleUnknownIoctl(Irp);
            break;
    }
    
    // Завершаем IRP
    Irp->IoStatus.Status = status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    
    return status;
}