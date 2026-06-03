/*============================================================================
    main.c - точка входа и инициализация драйвера
    Соответствует ТЗ: разделы 1, 4, 8
============================================================================*/

#include <ntddk.h>
#include <wdm.h>
#include "common.h"

// Внешние функции из других модулей
NTSTATUS FindKeServiceDescriptorTableWin10(VOID);
NTSTATUS CreateMdlForSSDT(VOID);
NTSTATUS SaveGoldenCopy(VOID);
NTSTATUS RestoreOriginalSSDT(VOID);
NTSTATUS InitLogger(VOID);
VOID FreeLogger(VOID);
NTSTATUS StartMonitor(ULONG IntervalMs);
NTSTATUS StopMonitor(VOID);
NTSTATUS InstallHook(ULONG ServiceIndex, ULONG64 HookFunction);
NTSTATUS RemoveHook(ULONG ServiceIndex);
NTSTATUS ClearLog(VOID);
NTSTATUS DeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp);

// Глобальные переменные (экспортируются в другие модули)
PSYSTEM_SERVICE_DESCRIPTOR_TABLE KeServiceDescriptorTable = NULL;
ULONG64* KiServiceTable = NULL;
ULONG KiServiceLimit = 0;
ULONG64* GoldenAddressTable = NULL;
ULONG64* HookTable = NULL;
BOOLEAN MonitoringActive = FALSE;
BOOLEAN AutoRestore = FALSE;
PDEVICE_OBJECT g_DeviceObject = NULL;

/*----------------------------------------------------------------------------
    DriverEntry - точка входа (ТЗ раздел 4.1)
----------------------------------------------------------------------------*/
NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
    NTSTATUS status;
    PDEVICE_OBJECT deviceObject = NULL;
    UNICODE_STRING deviceName;
    UNICODE_STRING symLinkName;
    
    UNREFERENCED_PARAMETER(RegistryPath);
    
    DbgPrint("SsdtMon: DriverEntry called\n");
    
    // 1. Поиск KeServiceDescriptorTable (ТЗ раздел 4.1)
    status = FindKeServiceDescriptorTableWin10();
    if (!NT_SUCCESS(status)) {
        DbgPrint("SsdtMon: Failed to find KeServiceDescriptorTable (0x%X)\n", status);
        return status;
    }
    
    // 2. Создание MDL для доступа к SSDT (ТЗ раздел 4.2)
    status = CreateMdlForSSDT();
    if (!NT_SUCCESS(status)) {
        DbgPrint("SsdtMon: Failed to create MDL (0x%X)\n", status);
        return status;
    }
    
    // 3. Сохранение эталонной копии (ТЗ раздел 4.2)
    status = SaveGoldenCopy();
    if (!NT_SUCCESS(status)) {
        DbgPrint("SsdtMon: Failed to save golden copy (0x%X)\n", status);
        return status;
    }
    
    // 4. Инициализация лога (кольцевой буфер)
    status = InitLogger();
    if (!NT_SUCCESS(status)) {
        DbgPrint("SsdtMon: Failed to init logger (0x%X)\n", status);
        return status;
    }
    
    // 5. Создание устройства для IOCTL (ТЗ раздел 5)
    RtlInitUnicodeString(&deviceName, L"\\Device\\SsdtMon");
    RtlInitUnicodeString(&symLinkName, L"\\DosDevices\\SsdtMon");
    
    status = IoCreateDevice(DriverObject, 0, &deviceName, 
                            FILE_DEVICE_UNKNOWN, 0, FALSE, &deviceObject);
    if (!NT_SUCCESS(status)) {
        DbgPrint("SsdtMon: IoCreateDevice failed (0x%X)\n", status);
        return status;
    }
    
    status = IoCreateSymbolicLink(&symLinkName, &deviceName);
    if (!NT_SUCCESS(status)) {
        IoDeleteDevice(deviceObject);
        return status;
    }
    
    deviceObject->Flags |= DO_DIRECT_IO;
    g_DeviceObject = deviceObject;
    
    // 6. Настройка dispatch-функций
    DriverObject->MajorFunction[IRP_MJ_CREATE] = 
        DriverObject->MajorFunction[IRP_MJ_CLOSE] = 
            (PDRIVER_DISPATCH)IoCreateClose;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DeviceControl;
    DriverObject->DriverUnload = DriverUnload;
    
    DbgPrint("SsdtMon: Driver loaded successfully\n");
    return STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------
    DriverUnload - выгрузка драйвера (ТЗ раздел 8)
----------------------------------------------------------------------------*/
VOID DriverUnload(PDRIVER_OBJECT DriverObject)
{
    UNICODE_STRING symLinkName;
    
    UNREFERENCED_PARAMETER(DriverObject);
    
    DbgPrint("SsdtMon: DriverUnload called\n");
    
    // Остановка мониторинга
    if (MonitoringActive) {
        StopMonitor();
    }
    
    // Восстановление оригинального SSDT (ТЗ раздел 2.4 use-case 3)
    RestoreOriginalSSDT();
    
    // Удаление симлинка и устройства
    RtlInitUnicodeString(&symLinkName, L"\\DosDevices\\SsdtMon");
    IoDeleteSymbolicLink(&symLinkName);
    if (g_DeviceObject) {
        IoDeleteDevice(g_DeviceObject);
    }
    
    // Освобождение памяти
    if (GoldenAddressTable) ExFreePoolWithTag(GoldenAddressTable, 'tdSS');
    if (HookTable) ExFreePoolWithTag(HookTable, 'tdSS');
    FreeLogger();
    
    DbgPrint("SsdtMon: Driver unloaded\n");
}