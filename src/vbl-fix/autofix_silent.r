/* autofix_silent.r — resources for VBL Fix (the silent shipped version). No UI, so
 * we only need a small memory partition and a version stamp. */

#include "Processes.r"
#include "Types.r"

resource 'SIZE' (-1) {
    reserved, acceptSuspendResumeEvents, reserved, canBackground,
    multiFinderAware, backgroundAndForeground, dontGetFrontClicks,
    ignoreChildDiedEvents, is32BitCompatible, isHighLevelEventAware,
    onlyLocalHLEvents, notStationeryAware, dontUseTextEditServices,
    notDisplayManagerAware, reserved, reserved,
    128 * 1024,
    96 * 1024
};

resource 'vers' (1, "VBL Fix") {
    0x01, 0x00, release, 0x00, verUS,
    "1.0",
    "1.0, G4 mini VBL re-arm fix (silent)"
};
