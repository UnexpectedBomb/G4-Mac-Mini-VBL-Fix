/*
 * VBL Autofix v1 — the actual fix for the G4 mini startup freeze (mild/common case).
 *
 * THE WIN (2026-06-30): on a starved boot, issuing the real enable —
 * Control(refNum, cscSetInterrupt, csMode=0) — brought VBL back (vbl 0 -> 119).
 * See ../vbl-reenable. The mild freeze is just "VBL left hardware-disabled by the
 * scaled-mode switch," and the standard enable call fixes it. No driver patch.
 *
 * WHAT THIS DOES (vs the vbl-reenable experiment)
 * -----------------------------------------------
 * vbl-reenable only fired the enable when it detected a starve (it was proving the
 * mechanism). This is the production-shaped fix: it issues cscSetInterrupt(csMode=0)
 * **unconditionally** every boot. On a healthy boot that's a harmless no-op
 * (re-enabling an already-enabled interrupt); on a mild-starved boot it revives VBL.
 * No detection needed — just always re-arm.
 *
 * This is the VALIDATION build: it pre-measures, enables, then post-measures, so a
 * batch of warm restarts proves the two things we still need:
 *   - healthy boots STAY healthy after the unconditional enable (it's safe), and
 *   - mild-starved boots get REVIVED.
 * The eventual shipped version can drop the measurements and just issue the enable
 * immediately (faster revive, silent).
 *
 * Beep codes:
 *   2 = was healthy, still healthy after enable (enable is a safe no-op)
 *   4 = was STARVED, REVIVED by the enable (the fix working)
 *   5 = was starved, enable did NOT revive (a deeper failure on this boot)
 *   6 = DANGER: was healthy, STARVED after enable -> csMode=0 is NOT safe here,
 *       do NOT ship the unconditional fix (should never happen; safety assertion)
 *   1 = error (couldn't measure / find the display)
 *
 * SCOPE: this fixes only the MILD case (VBL left disabled, boot still completes to
 * Startup Items). The SEVERE variants (framebuffer corruption / hard hang AT the
 * switch, before Startup Items even run) are unreachable by any app and are not
 * addressed here — native resolution remains the only fully-robust mode for those.
 * Keep a fan on the mini.
 */

#include <MacTypes.h>
#include <Quickdraw.h>
#include <Fonts.h>
#include <Windows.h>
#include <Events.h>
#include <TextUtils.h>
#include <Memory.h>
#include <OSUtils.h>
#include <Retrace.h>
#include <Timer.h>
#include <Devices.h>        /* PBControlSync/PBStatusSync, CntrlParam, GetDCtlEntry */
#include <Video.h>          /* cscSetInterrupt, cscGetInterrupt, VDFlagRecord       */
#include <Folders.h>
#include <Files.h>
#include <Sound.h>

#include <string.h>

#define kEnableCsMode     0        /* confirmed on hardware: csMode 0 = enable VBL */

#define kMeasureTicks     120
#define kTMIntervalMS     16
#define kHealthyVBLHz     40
#define kHealthyVBLMin    ((long)kHealthyVBLHz * kMeasureTicks / 60)
#define kTMSaneMin        ((long)(1000 / kTMIntervalMS) * kMeasureTicks / 60 / 2)
#define kLingerTicks      (20 * 60)

enum { kOutHealthyOK = 0, kOutRevived, kOutStillStarved, kOutBrokeHealthy, kOutError };


/* ---- interrupt-time counters (task record MUST be first) ----------------- */

typedef struct { VBLTask task; volatile long count; } VBLCounter;
typedef struct { TMTask  task; volatile long count; } TMCounter;

static pascal void VBLProc(VBLTaskPtr p)
{ VBLCounter *c = (VBLCounter *)p; c->count++; c->task.vblCount = 1; }
static pascal void TMProc(TMTaskPtr p)
{ TMCounter *c = (TMCounter *)p; c->count++; PrimeTime((QElemPtr)&c->task, kTMIntervalMS); }


/* ---- helpers ------------------------------------------------------------- */

static void BeepN(short n)
{
    unsigned long t; short i;
    for (i = 0; i < n; i++) { SysBeep(8); if (i + 1 < n) Delay(54, &t); }
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

static Boolean GetMainDisplaySlot(short *slotOut, short *refNumOut)
{
    GDHandle gd = GetMainDevice(); short refNum; AuxDCEHandle dce;
    if (gd == NULL) return false;
    refNum = (**gd).gdRefNum;
    if (refNumOut) *refNumOut = refNum;
    dce = (AuxDCEHandle)GetDCtlEntry(refNum);
    if (dce == NULL || *dce == NULL) return false;
    *slotOut = (short)(**dce).dCtlSlot;
    return true;
}

static OSErr MeasureVBL(short slot, long durationTicks, long *vblOut, long *tmOut)
{
    VBLCounter *vc; TMCounter *tc; long startTick; OSErr vblErr;
    *vblOut = 0; *tmOut = 0;
    vc = (VBLCounter *)NewPtrClear(sizeof(VBLCounter));
    tc = (TMCounter  *)NewPtrClear(sizeof(TMCounter));
    if (vc == NULL || tc == NULL) {
        if (vc) DisposePtr((Ptr)vc);
        if (tc) DisposePtr((Ptr)tc);
        return memFullErr;
    }
    vc->task.qType = vType; vc->task.vblAddr = NewVBLUPP(VBLProc);
    vc->task.vblCount = 1; vc->task.vblPhase = 0;
    vblErr = SlotVInstall((QElemPtr)&vc->task, slot);
    tc->task.tmAddr = NewTimerUPP(TMProc);
    InsXTime((QElemPtr)&tc->task);
    PrimeTime((QElemPtr)&tc->task, kTMIntervalMS);
    startTick = TickCount();
    while (TickCount() - startTick < durationTicks) { /* spin */ }
    if (vblErr == noErr) SlotVRemove((QElemPtr)&vc->task, slot);
    RmvTime((QElemPtr)&tc->task);
    *vblOut = vc->count; *tmOut = tc->count;
    if (vc->task.vblAddr) DisposeVBLUPP(vc->task.vblAddr);
    if (tc->task.tmAddr)  DisposeTimerUPP(tc->task.tmAddr);
    DisposePtr((Ptr)vc); DisposePtr((Ptr)tc);
    return vblErr;
}

static Boolean Alive(long vbl) { return (vbl >= kHealthyVBLMin); }

/* Control(refNum, cscSetInterrupt, &VDFlagRecord) — the enable. csParam holds a
 * POINTER to the param record (confirmed working on this driver). */
static OSErr SetVBLInterrupt(short refNum, SInt8 mode)
{
    CntrlParam pb; VDFlagRecord flag;
    flag.csMode = mode; flag.filler = 0;
    memset(&pb, 0, sizeof(pb));
    pb.ioCRefNum = refNum;
    pb.csCode = cscSetInterrupt;
    *(Ptr *)&pb.csParam[0] = (Ptr)&flag;
    return PBControlSync((ParmBlkPtr)&pb);
}
/* Status(refNum, cscGetInterrupt, ...) — read-only current state (logged for info). */
static OSErr GetVBLInterrupt(short refNum, SInt8 *modeOut)
{
    CntrlParam pb; VDFlagRecord flag;
    flag.csMode = -1; flag.filler = 0;
    memset(&pb, 0, sizeof(pb));
    pb.ioCRefNum = refNum;
    pb.csCode = cscGetInterrupt;
    *(Ptr *)&pb.csParam[0] = (Ptr)&flag;
    {
        OSErr e = PBStatusSync((ParmBlkPtr)&pb);
        *modeOut = flag.csMode;
        return e;
    }
}


/* ---- the fix + validation ------------------------------------------------ */

typedef struct {
    short slot, refNum; Boolean slotOK; OSErr measErr;
    OSErr getErr; SInt8 getMode; OSErr setErr;
    long  vblBefore, tmBefore, vblAfter, tmAfter;
    short outcome;
} FixResult;

static void RunFix(FixResult *r)
{
    memset(r, 0, sizeof(*r));
    r->outcome = kOutError; r->getMode = -1;

    r->slotOK = GetMainDisplaySlot(&r->slot, &r->refNum);
    if (!r->slotOK) return;

    r->getErr  = GetVBLInterrupt(r->refNum, &r->getMode);
    r->measErr = MeasureVBL(r->slot, kMeasureTicks, &r->vblBefore, &r->tmBefore);
    if (r->measErr != noErr || r->tmBefore < kTMSaneMin) { r->outcome = kOutError; return; }

    /* The fix: re-arm the VBL interrupt unconditionally. */
    r->setErr = SetVBLInterrupt(r->refNum, kEnableCsMode);

    MeasureVBL(r->slot, kMeasureTicks, &r->vblAfter, &r->tmAfter);

    /* 2x2 on alive-before x alive-after. */
    if (Alive(r->vblBefore))
        r->outcome = Alive(r->vblAfter) ? kOutHealthyOK : kOutBrokeHealthy;
    else
        r->outcome = Alive(r->vblAfter) ? kOutRevived : kOutStillStarved;
}


/* ---- reporting ----------------------------------------------------------- */

static const char *OutcomeName(short o)
{
    switch (o) {
        case kOutHealthyOK:    return "HEALTHY - stayed healthy after enable (safe no-op)";
        case kOutRevived:      return "REVIVED - enable restarted a starved VBL (the fix!)";
        case kOutStillStarved: return "STILL STARVED - enable did not revive this boot";
        case kOutBrokeHealthy: return "DANGER - enable DISABLED a healthy display!";
        default:               return "ERROR - could not measure";
    }
}
static short OutcomeBeeps(short o)
{
    switch (o) {
        case kOutHealthyOK:    return 2;
        case kOutRevived:      return 4;
        case kOutStillStarved: return 5;
        case kOutBrokeHealthy: return 6;
        default:               return 1;
    }
}

static void WriteLog(const FixResult *r)
{
    short vRefNum; long dirID; FSSpec spec; short refNum; long eof, len; Str255 line;
    if (FindFolder(kOnSystemDisk, kSystemFolderType, kDontCreateFolder, &vRefNum, &dirID) != noErr) return;
    if (FSMakeFSSpec(vRefNum, dirID, "\pVBL Autofix Log", &spec) != noErr) {
        if (FSpCreate(&spec, 'ttxt', 'TEXT', smSystemScript) != noErr) return;
    }
    if (FSpOpenDF(&spec, fsRdWrPerm, &refNum) != noErr) return;
    if (GetEOF(refNum, &eof) == noErr) SetFPos(refNum, fsFromStart, eof);

    line[0] = 0;
    PStrCat(line, "VBL Autofix: "); PStrCat(line, OutcomeName(r->outcome));
    PStrCat(line, " | before vbl="); PStrCatNum(line, r->vblBefore);
    PStrCat(line, " tm=");           PStrCatNum(line, r->tmBefore);
    PStrCat(line, " after vbl=");    PStrCatNum(line, r->vblAfter);
    PStrCat(line, " getInt=");       PStrCatNum(line, r->getMode);
    PStrCat(line, " setErr=");       PStrCatNum(line, r->setErr);
    PStrCat(line, " getErr=");       PStrCatNum(line, r->getErr);
    PStrCat(line, " slot=");         PStrCatNum(line, r->slot);
    PStrCat(line, " refNum=");       PStrCatNum(line, r->refNum);
    PStrCat(line, "\r");
    len = line[0]; FSWrite(refNum, &len, &line[1]); FSClose(refNum); FlushVol(NULL, vRefNum);
}

static void DrawLine(short v, const char *label, long value)
{
    Str255 s; s[0] = 0; PStrCat(s, label); PStrCatNum(s, value);
    MoveTo(20, v); DrawString(s);
}

static void ShowResultWindow(const FixResult *r)
{
    Rect bounds; WindowPtr win; short screenW, screenH; long deadline; EventRecord evt; Str255 s;
    screenW = qd.screenBits.bounds.right  - qd.screenBits.bounds.left;
    screenH = qd.screenBits.bounds.bottom - qd.screenBits.bounds.top;
    bounds.left = (screenW - 440) / 2; bounds.top = (screenH - 240) / 2;
    bounds.right = bounds.left + 440;  bounds.bottom = bounds.top + 240;
    win = NewWindow(NULL, &bounds, "\pVBL Autofix v1", true, documentProc, (WindowPtr)-1L, false, 0);
    if (win == NULL) return;
    SetPort((GrafPtr)win);
    TextFont(kFontIDGeneva); TextSize(9);

    TextFace(bold);
    MoveTo(20, 26); DrawString("\pG4 mini freeze fix: re-arm VBL via cscSetInterrupt(enable)");
    s[0] = 0; PStrCat(s, "Result: "); PStrCat(s, OutcomeName(r->outcome));
    MoveTo(20, 50); DrawString(s);
    TextFace(normal);

    DrawLine( 82, "VBL fires BEFORE enable: ",       r->vblBefore);
    DrawLine(100, "VBL fires AFTER enable: ",        r->vblAfter);
    DrawLine(118, "Time Manager fires (reference): ", r->tmBefore);
    DrawLine(136, "Boot display slot: ",             r->slot);
    DrawLine(154, "Boot display driver refNum: ",    r->refNum);

    s[0] = 0; PStrCat(s, "cscGetInterrupt csMode: "); PStrCatNum(s, r->getMode);
    PStrCat(s, "   setErr: "); PStrCatNum(s, r->setErr);
    MoveTo(20, 172); DrawString(s);

    s[0] = 0; PStrCat(s, "Beep code: "); PStrCatNum(s, OutcomeBeeps(r->outcome));
    PStrCat(s, "  (2=healthy 4=revived 5=still 6=DANGER 1=err)");
    MoveTo(20, 196); DrawString(s);
    MoveTo(20, 220); DrawString("\pClick or press a key to quit. Logged to System Folder.");

    deadline = TickCount() + kLingerTicks;
    while (TickCount() < deadline) {
        if (WaitNextEvent(mDownMask | keyDownMask, &evt, 6, NULL))
            if (evt.what == mouseDown || evt.what == keyDown) break;
    }
    DisposeWindow(win);
}

int main(void)
{
    FixResult r;
    InitGraf(&qd.thePort); InitFonts(); InitWindows(); InitMenus();
    TEInit(); InitDialogs(NULL); InitCursor();

    RunFix(&r);
    BeepN(OutcomeBeeps(r.outcome));
    WriteLog(&r);
    ShowResultWindow(&r);
    return 0;
}
