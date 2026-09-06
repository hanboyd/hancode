/*++
RC003 HID capture-only filter - diagnostic control device.

A named control device exposes the capture ring to user mode through two
IOCTLs:
  - IOCTL_RC003_CAPTURE_DUMP:  header + ring snapshot (read only)
  - IOCTL_RC003_CAPTURE_CLEAR: reset ring and counters

The control device is optional and diagnostic-only; any failure here must
not affect the filter's pass-through behavior.
--*/

#include "driver.h"
#include <wdmsec.h>

DECLARE_CONST_UNICODE_STRING(Rc003ControlDeviceName, L"\\Device\\Rc003HidCapture");
DECLARE_CONST_UNICODE_STRING(Rc003ControlSymbolicLink, L"\\DosDevices\\Rc003HidCapture");

_Function_class_(EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL)
VOID
Rc003ControlEvtIoDeviceControl(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
    )
{
    NTSTATUS status;
    WDFDEVICE controlDevice;
    WDFDRIVER driver;
    PRC003_DRIVER_CONTEXT driverContext;
    PRC003_FILTER_CONTEXT filterContext;
    PVOID outputBuffer;
    size_t bufferLength;
    size_t bytesReturned;

    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);

    controlDevice = WdfIoQueueGetDevice(Queue);
    driver = WdfDeviceGetDriver(controlDevice);
    driverContext = Rc003DriverGetContext(driver);
    filterContext = NULL;

    switch (IoControlCode) {
    case IOCTL_RC003_CAPTURE_DUMP:
        status = WdfRequestRetrieveOutputBuffer(
            Request,
            sizeof(RC003_DUMP_HEADER),
            &outputBuffer,
            &bufferLength);
        if (!NT_SUCCESS(status)) {
            WdfRequestComplete(Request, status);
            return;
        }

        RtlZeroMemory(outputBuffer, bufferLength);
        bytesReturned = 0;

        if (driverContext != NULL && driverContext->FilterDevice != NULL) {
            filterContext = Rc003FilterGetContext(driverContext->FilterDevice);
        }

        if (filterContext != NULL) {
            KIRQL irql;
            PRC003_DUMP_HEADER header = (PRC003_DUMP_HEADER)outputBuffer;
            size_t entries = 0;
            size_t slot;
            ULONG sequence;

            KeAcquireSpinLock(&filterContext->CaptureLock, &irql);

            KeQueryPerformanceCounter(&header->DumpTimestamp);
            header->QpcFrequency.QuadPart = header->DumpTimestamp.HighPart;
            header->ReportsSeen = filterContext->ReportsSeen;
            header->ReportsMatched = filterContext->ReportsMatched;
            header->ReportId1Seen = filterContext->ReportId1Seen;
            header->ReportsLengthMismatch = filterContext->ReportsLengthMismatch;
            header->LastSequence = filterContext->Sequence;

            if (filterContext->Sequence != 0) {
                entries = min(filterContext->Sequence,
                              (ULONG)RC003_CAPTURE_MAX_ENTRIES);
                sequence = filterContext->Sequence - (ULONG)entries;
                for (slot = 0; slot < entries; slot++) {
                    PUCHAR destination = (PUCHAR)outputBuffer
                                       + sizeof(RC003_DUMP_HEADER)
                                       + slot * sizeof(RC003_CAPTURE_ENTRY);
                    if (sizeof(RC003_DUMP_HEADER)
                        + (slot + 1) * sizeof(RC003_CAPTURE_ENTRY)
                        > bufferLength) {
                        break;
                    }
                    RtlCopyMemory(destination,
                                  &filterContext->Ring[sequence % RC003_CAPTURE_MAX_ENTRIES],
                                  sizeof(RC003_CAPTURE_ENTRY));
                    sequence++;
                }
                header->EntryCount = (ULONG)slot;
            }

            KeReleaseSpinLock(&filterContext->CaptureLock, irql);

            bytesReturned = sizeof(RC003_DUMP_HEADER)
                          + (size_t)header->EntryCount * sizeof(RC003_CAPTURE_ENTRY);
            if (bytesReturned > bufferLength) {
                bytesReturned = bufferLength;
            }
        }

        WdfRequestSetInformation(Request, bytesReturned);
        WdfRequestComplete(Request, STATUS_SUCCESS);
        return;

    case IOCTL_RC003_CAPTURE_CLEAR:
        if (driverContext != NULL && driverContext->FilterDevice != NULL) {
            filterContext = Rc003FilterGetContext(driverContext->FilterDevice);
        }
        if (filterContext != NULL) {
            KIRQL irql;
            KeAcquireSpinLock(&filterContext->CaptureLock, &irql);
            RtlZeroMemory(filterContext->Ring, sizeof(filterContext->Ring));
            filterContext->Sequence = 0;
            filterContext->ReportsSeen = 0;
            filterContext->ReportsMatched = 0;
            filterContext->ReportId1Seen = 0;
            filterContext->ReportsLengthMismatch = 0;
            KeReleaseSpinLock(&filterContext->CaptureLock, irql);
        }
        WdfRequestSetInformation(Request, 0);
        WdfRequestComplete(Request, STATUS_SUCCESS);
        return;

    default:
        WdfRequestSetInformation(Request, 0);
        WdfRequestComplete(Request, STATUS_INVALID_DEVICE_REQUEST);
        return;
    }
}

_Use_decl_annotations_
NTSTATUS
Rc003FilterCreateControlDevice(
    _In_ WDFDRIVER Driver
    )
{
    PWDFDEVICE_INIT deviceInit;
    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_IO_QUEUE_CONFIG queueConfig;
    WDFDEVICE controlDevice;
    WDFQUEUE queue;
    PRC003_DRIVER_CONTEXT driverContext;
    NTSTATUS status;

    deviceInit = WdfControlDeviceInitAllocate(
        Driver,
        &SDDL_DEVOBJ_SYS_ALL_ADM_RWX_WORLD_R);
    if (deviceInit == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    WdfDeviceInitSetDeviceType(deviceInit, FILE_DEVICE_UNKNOWN);
    WdfDeviceInitSetCharacteristics(deviceInit,
                                    FILE_DEVICE_SECURE_OPEN,
                                    FALSE);
    WdfDeviceInitSetExclusive(deviceInit, FALSE);
    status = WdfDeviceInitAssignName(deviceInit, &Rc003ControlDeviceName);
    if (!NT_SUCCESS(status)) {
        WdfDeviceInitFree(deviceInit);
        return status;
    }

    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    status = WdfDeviceCreate(&deviceInit, &attributes, &controlDevice);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = WdfDeviceCreateSymbolicLink(controlDevice, &Rc003ControlSymbolicLink);
    if (!NT_SUCCESS(status)) {
        WdfObjectDelete(controlDevice);
        return status;
    }

    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queueConfig,
                                           WdfIoQueueDispatchParallel);
    queueConfig.EvtIoDeviceControl = Rc003ControlEvtIoDeviceControl;
    status = WdfIoQueueCreate(controlDevice,
                              &queueConfig,
                              WDF_NO_OBJECT_ATTRIBUTES,
                              &queue);
    if (!NT_SUCCESS(status)) {
        WdfObjectDelete(controlDevice);
        return status;
    }

    driverContext = Rc003DriverGetContext(Driver);
    if (driverContext != NULL && driverContext->QpcFrequency.QuadPart == 0) {
        LARGE_INTEGER qpc;
        KeQueryPerformanceCounter(&qpc);
        driverContext->QpcFrequency.QuadPart = qpc.HighPart;
    }

    return STATUS_SUCCESS;
}
