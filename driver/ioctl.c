/*============================================================================
    ioctl.c - обработка IOCTL команд из user mode
    Соответствует ТЗ: раздел 5
============================================================================*/

#include <ntddk.h>
#include "common.h"

// Внешние переменные
extern ULONG KiServiceLimit;
extern BOOLEAN AutoRestore;

// Внешние функции
extern NTSTATUS InstallHook(ULONG ServiceIndex, ULONG64 HookFunction);
extern NTSTATUS RemoveHook(ULONG ServiceIndex);
extern NTSTATUS StartMonitor(ULONG IntervalMs);
extern NTSTATUS StopMonitor(VOID);
extern NTSTATUS ReadLog(PVOID OutBuffer, ULONG OutSize, PULONG BytesRead);
extern NTSTATUS ClearLog(VOID);
extern NTSTATUS RestoreOriginalSSDT(VOID);
extern NTSTATUS GetHookInfo(PHOOK_INFO InfoBuffer, ULONG BufferSize, PULONG InfoCount);

/*----------------------------------------------------------------------------
    GetHookInfo - получение списка активных хуков
----------------------------------------------------------------------------*/
NTSTATUS GetHookInfo(PHOOK_INFO InfoBuffer, ULONG BufferSize, PULONG InfoCount)
{
    ULONG i;
    ULONG count = 0;
    ULONG maxCount = BufferSize / sizeof(HOOK_INFO);
    
    if (!InfoBuffer || !InfoCount) {
        return STATUS_INVALID_PARAMETER;
    }
    
    for (i = 0; i < KiServiceLimit && count < maxCount; i++) {
        if (HookTable[i] != 0) {
            InfoBuffer[count].ServiceIndex = i;
            InfoBuffer[count].OriginalAddress = GoldenAddressTable[i];
            InfoBuffer[count].CurrentAddress = KiServiceTable[i];
            InfoBuffer[count].IsHooked = TRUE;
            count++;
        }
    }
    
    *InfoCount = count;
    return STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------
    DeviceControl - диспетчер IOCTL (ТЗ раздел 5)
----------------------------------------------------------------------------*/
NTSTATUS DeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    PIO_STACK_LOCATION irpStack;
    NTSTATUS status = STATUS_SUCCESS;
    ULONG ioControlCode;
    ULONG inputLength;
    ULONG outputLength;
    PVOID inputBuffer;
    PVOID outputBuffer;
    ULONG bytesReturned = 0;
    
    UNREFERENCED_PARAMETER(DeviceObject);
    
    irpStack = IoGetCurrentIrpStackLocation(Irp);
    ioControlCode = irpStack->Parameters.DeviceIoControl.IoControlCode;
    inputLength = irpStack->Parameters.DeviceIoControl.InputBufferLength;
    outputLength = irpStack->Parameters.DeviceIoControl.OutputBufferLength;
    inputBuffer = Irp->AssociatedIrp.SystemBuffer;
    outputBuffer = Irp->AssociatedIrp.SystemBuffer;
    
    switch (ioControlCode) {
        case IOCTL_SSDT_GET_HOOKS:
            {
                PHOOK_INFO hookInfo = (PHOOK_INFO)outputBuffer;
                ULONG hookCount = 0;
                status = GetHookInfo(hookInfo, outputLength, &hookCount);
                bytesReturned = hookCount * sizeof(HOOK_INFO);
            }
            break;
            
        case IOCTL_SSDT_SET_HOOK:
            {
                PHOOK_REQUEST request = (PHOOK_REQUEST)inputBuffer;
                if (inputLength < sizeof(HOOK_REQUEST) || !request || !request->HookFunction) {
                    status = STATUS_INVALID_PARAMETER;
                    break;
                }
                status = InstallHook(request->ServiceIndex, request->HookFunction);
            }
            break;
            
        case IOCTL_SSDT_REMOVE_HOOK:
            {
                PULONG index = (PULONG)inputBuffer;
                if (inputLength < sizeof(ULONG) || !index) {
                    status = STATUS_INVALID_PARAMETER;
                    break;
                }
                status = RemoveHook(*index);
            }
            break;
            
        case IOCTL_SSDT_MONITOR_START:
            {
                ULONG interval = (inputLength >= sizeof(ULONG)) ? *(PULONG)inputBuffer : 200;
                status = StartMonitor(interval);
            }
            break;
            
        case IOCTL_SSDT_MONITOR_STOP:
            status = StopMonitor();
            break;
            
        case IOCTL_SSDT_GET_LOG:
            status = ReadLog(outputBuffer, outputLength, &bytesReturned);
            break;
            
        case IOCTL_SSDT_RESTORE:
            status = RestoreOriginalSSDT();
            break;
            
        case IOCTL_SSDT_CLEAR_LOG:
            status = ClearLog();
            break;
            
        default:
            status = STATUS_INVALID_DEVICE_REQUEST;
            break;
    }
    
    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = bytesReturned;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    
    return status;
}