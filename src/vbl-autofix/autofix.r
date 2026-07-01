/* autofix.r — resources for the VBL Autofix app. Window is created in code; we
 * only need the memory partition and a version stamp. */

#include "Processes.r"
#include "Types.r"

resource 'SIZE' (-1) {
    reserved, acceptSuspendResumeEvents, reserved, canBackground,
    multiFinderAware, backgroundAndForeground, dontGetFrontClicks,
    ignoreChildDiedEvents, is32BitCompatible, isHighLevelEventAware,
    onlyLocalHLEvents, notStationeryAware, dontUseTextEditServices,
    notDisplayManagerAware, reserved, reserved,
    512 * 1024,
    384 * 1024
};

resource 'vers' (1, "VBL Autofix") {
    0x01, 0x00, release, 0x00, verUS,
    "1.0",
    "1.0, G4 mini VBL re-arm fix (mild case)"
};
