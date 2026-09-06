/*++
RC003 HID capture-only filter - IRP_MJ_READ observation.

The filter sits below kbdhid, so the requests it sees are the raw reads
kbdhid issues against the hidclass keyboard collection.  Report data is
observed in the IRP completion routine and is never modified.
--*/

#include "driver.h"
#include "remap.h"

#define RC003_REPORT_ID_KEYBOARD 0x01
#define RC003_REPORT_LENGTH      7    /* report id + 3 x 16-bit usages */

#define RC003_USAGE_OK          0x0028
#define RC003_USAGE_VOLUME_UP   0x0080
#define RC003_USAGE_VOLUME_DOWN 0x0081
#define RC003_USAGE_BACK        0x00F1

static
UCHAR
Rc003ClassifyUsage(
    _In_ USHORT Usage
    )
{
    switch (Usage) {
    case RC003_USAGE_OK:
        return RC003_MATCH_OK;
    case RC003_USAGE_VOLUME_UP:
        return RC003_MATCH_VOLUME_UP;
    case RC003_USAGE_VOLUME_DOWN:
        return RC003_MATCH_VOLUME_DOWN;
    case RC003_USAGE_BACK:
        return RC003_MATCH_BACK;
    default:
        return 0;
    }
}

/*
 * Record one observed report into the fixed ring buffer.
 *
 * Only reports containing a target usage (or the OK comparison usage) are
 * stored as entries; ordinary direction-key traffic only bumps counters so
 * the ring cannot be flooded by normal use.
 */
static
VOID
Rc003RecordReport(
    _Inout_ PRC003_FILTER_CONTEXT Context,
    _In_ UCHAR ReportId,
    _In_ USHORT ReportLength,
    _In_reads_bytes_(ReportLength) PUCHAR Data
    )
{
    RC003_CAPTURE_ENTRY entry;
    KIRQL irql;

    if (ReportLength < RC003_REPORT_LENGTH) {
        KeAcquireSpinLock(&Context->CaptureLock, &irql);
        Context->ReportsLengthMismatch++;
        KeReleaseSpinLock(&Context->CaptureLock, irql);
        return;
    }

    entry.MatchFlags = 0;
    entry.UsageSlot0 = 0;
    entry.UsageSlot1 = 0;
    entry.UsageSlot2 = 0;

    if (ReportId == RC003_REPORT_ID_KEYBOARD) {
        entry.UsageSlot0 = (USHORT)(Data[1] | ((USHORT)Data[2] << 8));
        entry.UsageSlot1 = (USHORT)(Data[3] | ((USHORT)Data[4] << 8));
        entry.UsageSlot2 = (USHORT)(Data[5] | ((USHORT)Data[6] << 8));
        entry.MatchFlags = Rc003ClassifyUsage(entry.UsageSlot0)
                         | Rc003ClassifyUsage(entry.UsageSlot1)
                         | Rc003ClassifyUsage(entry.UsageSlot2);
    }

    KeAcquireSpinLock(&Context->CaptureLock, &irql);

    Context->ReportsSeen++;
    if (ReportId == RC003_REPORT_ID_KEYBOARD) {
        Context->ReportId1Seen++;
    }
    if (entry.MatchFlags != 0) {
        LARGE_INTEGER timestamp;

        Context->ReportsMatched++;
        entry.Sequence = Context->Sequence++;
        KeQueryPerformanceCounter(&timestamp);
        entry.TimestampLow = timestamp.LowPart;
        entry.TimestampHigh = timestamp.HighPart;
        entry.ReportId = ReportId;
        entry.ReportLength = ReportLength;
        /* Drop-oldest: overwrite the slot the new sequence maps to. */
        Context->Ring[entry.Sequence % RC003_CAPTURE_MAX_ENTRIES] = entry;
    }

    KeReleaseSpinLock(&Context->CaptureLock, irql);
}

_Function_class_(IO_COMPLETION_ROUTINE)
NTSTATUS
Rc003FilterReadCompletion(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp,
    _In_opt_ PVOID Context
    )
{
    WDFDEVICE device;
    PRC003_FILTER_CONTEXT filterContext;
    PUCHAR buffer;
    ULONG length;

    UNREFERENCED_PARAMETER(Context);

    /*
     * Observation only.  On any unexpected shape simply return; the IRP
     * then completes normally with its original status, length and payload.
     */
    if (!NT_SUCCESS(Irp->IoStatus.Status)) {
        return STATUS_SUCCESS;
    }

    device = WdfWdmDeviceGetWdfDeviceHandle(DeviceObject);
    if (device == NULL) {
        return STATUS_SUCCESS;
    }
    filterContext = Rc003FilterGetContext(device);
    if (filterContext == NULL) {
        return STATUS_SUCCESS;
    }

    buffer = NULL;
    if (Irp->AssociatedIrp.SystemBuffer != NULL) {
        buffer = (PUCHAR)Irp->AssociatedIrp.SystemBuffer;
    } else if (Irp->MdlAddress != NULL) {
        buffer = (PUCHAR)MmGetSystemAddressForMdlSafe(
            Irp->MdlAddress,
            (MM_PAGE_PRIORITY)(NormalPagePriority | MdlMappingNoExecute));
    }
    if (buffer == NULL) {
        return STATUS_SUCCESS;
    }

    length = (ULONG)Irp->IoStatus.Information;
    if (length == 0) {
        return STATUS_SUCCESS;
    }

    Rc003RecordReport(filterContext, buffer[0], (USHORT)length, buffer);

    /*
     * Carrier remap, after capture (so the ring records the original wire
     * bytes).  Equal-length, in-place, report-ID-1 only; the transform
     * touches nothing when the report does not match, so kbdhid receives
     * the byte-identical original on every non-matching report.
     */
    (VOID)Rc003RemapReport(buffer, length);
    return STATUS_SUCCESS;
}

_Function_class_(EVT_WDF_IO_IN_CALLER_CONTEXT)
NTSTATUS
Rc003FilterEvtIoInCallerContext(
    _In_ WDFDEVICE Device,
    _In_ WDFREQUEST Request
    )
{
    PIRP irp;
    PIO_STACK_LOCATION irpStack;
    PDEVICE_OBJECT lowerDeviceObject;

    irp = WdfRequestWdmGetIrp(Request);
    irpStack = IoGetCurrentIrpStackLocation(irp);

    if (irpStack->MajorFunction != IRP_MJ_READ) {
        /*
         * Not a read: hand the request back to the framework.  This filter
         * creates no queues, so the framework forwards it to the next lower
         * driver (the hidclass collection PDO) untouched.
         */
        return WdfDeviceEnqueueRequest(Device, Request);
    }

    /*
     * Read request: forward manually with an observation completion routine.
     * The routine only reads the completed buffer; the request completes to
     * its originator (kbdhid) exactly as it would without this filter.
     */
    IoCopyCurrentIrpStackLocationToNext(irp);
    IoSetCompletionRoutine(irp,
                           Rc003FilterReadCompletion,
                           NULL,
                           TRUE,
                           TRUE,
                           TRUE);
    lowerDeviceObject = WdfDeviceWdmGetAttachedDevice(Device);
    (VOID)IoCallDriver(lowerDeviceObject, irp);
    return STATUS_SUCCESS;
}
