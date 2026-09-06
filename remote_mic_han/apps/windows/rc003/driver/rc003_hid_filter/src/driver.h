/*++
RC003 HID capture-only filter - shared definitions.

This prototype attaches as a lower filter on the RC003 keyboard TLC
devnode (between kbdhid and the hidclass collection PDO).  It observes
IRP_MJ_READ completions for report ID 1 and records metadata for the
target usages only.  It never modifies, suppresses, or injects anything.
--*/

#pragma once

#include <ntddk.h>
#include <wdf.h>

#define RC003_POOL_TAG (ULONG)'F3CR'

#define RC003_CAPTURE_MAX_ENTRIES 256

/* Match flags. */
#define RC003_MATCH_OK         0x01  /* 0x0028 - working comparison   */
#define RC003_MATCH_VOLUME_UP  0x02  /* 0x0080                        */
#define RC003_MATCH_VOLUME_DOWN 0x04 /* 0x0081                        */
#define RC003_MATCH_BACK       0x08  /* 0x00F1                        */

#pragma pack(push, 1)
typedef struct _RC003_CAPTURE_ENTRY {
    ULONG         Sequence;
    ULONG         TimestampLow;   /* QPC capture time                  */
    ULONG         TimestampHigh;
    UCHAR         ReportId;
    UCHAR         MatchFlags;
    USHORT        ReportLength;
    USHORT        UsageSlot0;
    USHORT        UsageSlot1;
    USHORT        UsageSlot2;
} RC003_CAPTURE_ENTRY;
#pragma pack(pop)

typedef struct _RC003_DRIVER_CONTEXT {
    WDFDEVICE  FilterDevice;
    LARGE_INTEGER QpcFrequency;
} RC003_DRIVER_CONTEXT, *PRC003_DRIVER_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(RC003_DRIVER_CONTEXT, Rc003DriverGetContext)

typedef struct _RC003_FILTER_CONTEXT {
    WDFDEVICE Device;

    /* Capture ring: fixed size, drop-oldest, single writer per entry. */
    RC003_CAPTURE_ENTRY Ring[RC003_CAPTURE_MAX_ENTRIES];
    ULONG  Sequence;         /* monotonically increasing               */
    KSPIN_LOCK CaptureLock;

    /* Diagnostic counters (informational only). */
    ULONG  ReportsSeen;
    ULONG  ReportsMatched;
    ULONG  ReportId1Seen;
    ULONG  ReportsLengthMismatch;
} RC003_FILTER_CONTEXT, *PRC003_FILTER_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(RC003_FILTER_CONTEXT, Rc003FilterGetContext)

/* Control device IOCTLs (see tools/rc003_capture_dump.py). */
#define IOCTL_RC003_CAPTURE_DUMP \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_RC003_CAPTURE_CLEAR \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x804, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#pragma pack(push, 1)
typedef struct _RC003_DUMP_HEADER {
    ULONG         EntryCount;     /* valid entries in the snapshot      */
    ULONG         LastSequence;   /* sequence of the newest entry        */
    ULONG         ReportsSeen;
    ULONG         ReportsMatched;
    ULONG         ReportId1Seen;
    ULONG         ReportsLengthMismatch;
    LARGE_INTEGER QpcFrequency;
    LARGE_INTEGER DumpTimestamp;  /* QPC when the snapshot was taken     */
} RC003_DUMP_HEADER, *PRC003_DUMP_HEADER;
#pragma pack(pop)

/* driver.c */
DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD Rc003FilterEvtDeviceAdd;
EVT_WDF_DRIVER_UNLOAD Rc003FilterEvtDriverUnload;

/* read_capture.c */
_Function_class_(EVT_WDF_IO_IN_CALLER_CONTEXT)
NTSTATUS
Rc003FilterEvtIoInCallerContext(
    _In_ WDFDEVICE Device,
    _In_ WDFREQUEST Request
    );

/* control.c */
NTSTATUS Rc003FilterCreateControlDevice(_In_ WDFDRIVER Driver);
