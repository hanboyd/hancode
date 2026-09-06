/*++
RC003 HID capture-only filter - DriverEntry and EvtDeviceAdd.

Fail-open by construction: nothing in this file can alter an I/O request.
--*/

#include "driver.h"

_Use_decl_annotations_
NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
    )
{
    WDF_DRIVER_CONFIG config;
    WDF_OBJECT_ATTRIBUTES attributes;
    WDFDRIVER driver;
    NTSTATUS status;

    WDF_DRIVER_CONFIG_INIT(&config, Rc003FilterEvtDeviceAdd);
    config.EvtDriverUnload = Rc003FilterEvtDriverUnload;

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, RC003_DRIVER_CONTEXT);
    status = WdfDriverCreate(DriverObject,
                             RegistryPath,
                             &attributes,
                             &config,
                             &driver);
    if (NT_SUCCESS(status)) {
        PRC003_DRIVER_CONTEXT driverContext = Rc003DriverGetContext(driver);
        if (driverContext != NULL) {
            /* QPC frequency for capture-ring timestamps (shared user page). */
            driverContext->QpcFrequency.QuadPart = SharedUserData->QpcFrequency;

            /*
             * Diagnostic control device.  Per KMDF's control-device model
             * this must be created from DriverEntry (never from a
             * per-device callback such as EvtDeviceAdd).  Fail-open: a
             * failure is logged and recorded in the driver context but
             * must not prevent the filter from attaching.
             */
            driverContext->ControlDeviceStatus =
                Rc003FilterCreateControlDevice(driver);
        }
    }
    return status;
}

_Use_decl_annotations_
NTSTATUS
Rc003FilterEvtDeviceAdd(
    _In_ WDFDRIVER Driver,
    _Inout_ PWDFDEVICE_INIT DeviceInit
    )
{
    NTSTATUS status;
    WDF_OBJECT_ATTRIBUTES attributes;
    WDFDEVICE device;
    PRC003_FILTER_CONTEXT filterContext;
    PRC003_DRIVER_CONTEXT driverContext;

    /*
     * The diagnostic control device is created in DriverEntry (see there);
     * nothing diagnostic may run from this per-device callback, and its
     * outcome never affects the filter attach below.
     */

    WdfFdoInitSetFilter(DeviceInit);
    WdfDeviceInitSetIoInCallerContextCallback(DeviceInit,
                                              Rc003FilterEvtIoInCallerContext);

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, RC003_FILTER_CONTEXT);
    status = WdfDeviceCreate(&DeviceInit, &attributes, &device);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    filterContext = Rc003FilterGetContext(device);
    filterContext->Device = device;
    KeInitializeSpinLock(&filterContext->CaptureLock);

    driverContext = Rc003DriverGetContext(Driver);
    if (driverContext != NULL) {
        driverContext->FilterDevice = device;
    }

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
VOID
Rc003FilterEvtDriverUnload(
    _In_ WDFDRIVER Driver
    )
{
    PRC003_DRIVER_CONTEXT driverContext = Rc003DriverGetContext(Driver);

    /*
     * A driver that creates both a control device and PnP device objects
     * must delete the control device (PASSIVE_LEVEL) after the framework
     * has removed the PnP devices; EvtDriverUnload satisfies that.
     */
    if (driverContext != NULL && driverContext->ControlDevice != NULL) {
        WdfObjectDelete(driverContext->ControlDevice);
        driverContext->ControlDevice = NULL;
    }
}
