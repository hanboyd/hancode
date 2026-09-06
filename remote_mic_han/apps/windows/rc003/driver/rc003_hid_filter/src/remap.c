/*++
RC003 HID carrier remap - in-place, equal-length usage replacement.

See remap.h for the report layout and the rationale.  This file is
deliberately free of kernel/WDK dependencies so the identical code runs in
the user-mode fixture test (tests/remap_test.c).
--*/

#include "remap.h"

#define RC003_REPORT_ID_KEYBOARD 0x01
#define RC003_REPORT_MIN_LENGTH  7   /* report id + 3 x 16-bit usages */

#define USAGE_VOLUME_UP   0x0080
#define USAGE_VOLUME_DOWN 0x0081
#define USAGE_BACK        0x00F1

#define USAGE_F13 0x0068
#define USAGE_F14 0x0069
#define USAGE_F15 0x006A

static int
Rc003RemapUsage(
    unsigned short usage,
    unsigned short *target
    )
{
    switch (usage) {
    case USAGE_VOLUME_UP:
        *target = USAGE_F13;
        return 1;
    case USAGE_VOLUME_DOWN:
        *target = USAGE_F14;
        return 1;
    case USAGE_BACK:
        *target = USAGE_F15;
        return 1;
    default:
        return 0;
    }
}

unsigned int
Rc003RemapReport(
    unsigned char *report,
    unsigned __int64 length
    )
{
    unsigned int replaced = 0;
    unsigned int slot;

    if (report == NULL || length < RC003_REPORT_MIN_LENGTH
        || report[0] != RC003_REPORT_ID_KEYBOARD) {
        return 0;
    }

    for (slot = 0; slot < 3; slot++) {
        unsigned short usage = (unsigned short)(
            report[1 + slot * 2] | ((unsigned short)report[2 + slot * 2] << 8)
        );
        unsigned short target = 0;
        if (Rc003RemapUsage(usage, &target)) {
            report[1 + slot * 2] = (unsigned char)(target & 0xFF);
            report[2 + slot * 2] = (unsigned char)(target >> 8);
            replaced++;
        }
    }

    return replaced;
}
