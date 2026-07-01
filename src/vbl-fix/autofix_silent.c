/*
 * VBL Fix (silent) — the everyday shipped fix for the G4 mini OS 9 startup freeze.
 *
 * Issues cscSetInterrupt(enable) to the boot display at startup, then quits. No
 * window, no beep, no measurement — invisible when it works (like an extension).
 * Writes a log line ONLY if the enable call returns an error (nothing on success).
 *
 * This is the stripped everyday version of VBL Autofix; see autofix.c for the
 * validation build that measures and reports (beep + log) what it did. Confirmed on
 * hardware (2026-06-30): cscSetInterrupt(csMode=0) revives a starved boot
 * (vbl 0 -> 119) and is a harmless no-op on a healthy boot (validated across 9
 * healthy boots + 2 revives). Fixes the mild/common freeze only; the rare severe
 * variant (framebuffer corruption / hard hang AT the switch, before Startup Items
 * run) is unreachable by any app.
 *
 * Install: drop into System Folder -> Startup Items. Remove: boot holding Shift and
 * delete it. Modifies no driver or system file.
 */

#include <MacTypes.h>
#include <Quickdraw.h>
#include <Devices.h>       /* PBControlSync, CntrlParam        */
#include <Video.h>         /* cscSetInterrupt, VDFlagRecord    */
#include <Folders.h>
#include <Files.h>
#include <TextUtils.h>

#include <string.h>

#define kEnableCsMode 0    /* confirmed on hardware: csMode 0 = enable VBL */

/* Control(refNum, cscSetInterrupt, &VDFlagRecord) — the enable. csParam holds a
 * POINTER to the param record (convention confirmed working on this driver). */
static OSErr SetVBLInterrupt(short refNum, SInt8 mode)
{
    CntrlParam   pb;
    VDFlagRecord flag;
    flag.csMode = mode;
    flag.filler = 0;
    memset(&pb, 0, sizeof(pb));
    pb.ioCRefNum = refNum;
    pb.csCode    = cscSetInterrupt;
    *(Ptr *)&pb.csParam[0] = (Ptr)&flag;
    return PBControlSync((ParmBlkPtr)&pb);
}

static void PStrCat(Str255 dst, const char *src)
{
    short len = dst[0];
    while (*src && len < 255) dst[++len] = (unsigned char)*src++;
    dst[0] = (unsigned char)len;
}
static void PStrCatNum(Str255 dst, long n)
{
    Str255 num; short i;
    NumToString(n, num);
    for (i = 1; i <= num[0] && dst[0] < 255; i++) dst[++dst[0]] = num[i];
}

/* Only called on error — leave a breadcrumb for a user who is still freezing. */
static void LogError(short refNum, OSErr err)
{
    short vRefNum; long dirID; FSSpec spec; short fRef; long eof, len; Str255 line;
    if (FindFolder(kOnSystemDisk, kSystemFolderType, kDontCreateFolder,
                   &vRefNum, &dirID) != noErr) return;
    if (FSMakeFSSpec(vRefNum, dirID, "\pVBL Fix Log", &spec) != noErr) {
        if (FSpCreate(&spec, 'ttxt', 'TEXT', smSystemScript) != noErr) return;
    }
    if (FSpOpenDF(&spec, fsRdWrPerm, &fRef) != noErr) return;
    if (GetEOF(fRef, &eof) == noErr) SetFPos(fRef, fsFromStart, eof);
    line[0] = 0;
    PStrCat(line, "VBL Fix: cscSetInterrupt(enable) FAILED err=");
    PStrCatNum(line, err);
    PStrCat(line, " refNum="); PStrCatNum(line, refNum);
    PStrCat(line, "\r");
    len = line[0];
    FSWrite(fRef, &len, &line[1]);
    FSClose(fRef);
    FlushVol(NULL, vRefNum);
}

int main(void)
{
    GDHandle gd; short refNum; OSErr err;

    InitGraf(&qd.thePort);          /* QuickDraw globals (GetMainDevice needs them) */

    gd = GetMainDevice();
    if (gd == NULL) return 0;
    refNum = (**gd).gdRefNum;

    err = SetVBLInterrupt(refNum, kEnableCsMode);   /* re-arm VBL; no-op if already on */
    if (err != noErr) LogError(refNum, err);

    return 0;
}
