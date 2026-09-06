/*++
RC003 HID carrier remap - platform-neutral, kernel-free transform.

Historical wire-level RC003 keyboard reports (report ID 1) look like:
    [01][00][00][usage_lo][usage_hi][00][00] ... (padded to the collection's
    input report length)
i.e. up to three little-endian 16-bit usages at bytes 1..6; on the real
remote a single pressed key lands at bytes 3-4.  The transform rewrites the
three usages kbdhid cannot translate into standard keyboard usages it can:

    0x0080 -> 0x0068 (F13)   volume up
    0x0081 -> 0x0069 (F14)   volume down
    0x00F1 -> 0x006A (F15)   back

Only the matched 16-bit slot changes; report ID, length, and every other
byte stay identical.  Fail-open: any unexpected shape returns 0 and leaves
the buffer untouched.
--*/

#pragma once

/* Deliberately header-free (no stddef.h/CRT): the EWDK toolchain ships a
   kernel-only header set, and the fixture test loads this code as a bare
   DLL with no CRT at all.  ``unsigned __int64`` matches x64 SIZE_T/ULONG
   call sites exactly. */

#ifndef NULL
#define NULL ((void *)0)
#endif

/* Returns the number of usage slots rewritten (0 = untouched). */
unsigned int Rc003RemapReport(unsigned char *report, unsigned __int64 length);
