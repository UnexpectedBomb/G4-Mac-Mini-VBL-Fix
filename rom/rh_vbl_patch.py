#!/usr/bin/env python3
# rh_vbl_patch.py -- patch ATY,RockHopper2 so cscSwitchMode re-arms the VBL
# interrupt at its tail, doing inside the driver exactly what the proven app fix
# does from outside (cscSetInterrupt with csMode=0).
#
# Mechanism (from the disassembly):
#   cscSwitchMode handler = func@0x3750; its mode/CRTC reprogram (0x3574) stops the
#   HW vblank interrupt and the switch never re-enables it. The ONLY re-enable is the
#   cscSetInterrupt(csMode=0) handler @0x3b50 (sets the enabled flag + writes the HW
#   interrupt-enable via 0x6f68/0x6ffc). We detour the switch's tail converge-point
#   (0x3788, reached on both success and failure) into an appended code cave that
#   calls 0x3b50 with a csMode=0 VDFlagRecord, then runs the displaced instruction
#   and returns. Method mirrors the cfmtool dump -> append cave -> build flow, so
#   relocations/offsets are recomputed correctly.
#
# Requires cfmtool from Elliot Nunn's Mac OS ROM toolchain (tbxi-patches). Point at
# it with the TBXI_PATCHES environment variable, or have cfmtool importable already:
#   TBXI_PATCHES=/path/to/tbxi-patches ./rh_vbl_patch.py RockHopper2.pef RockHopper2-VBL.pef
#
# The offsets below are for RockHopper2 1.0.1f63 (the driver in the standard
# MacOS9Lives mini ROM). The sanity asserts will stop the script rather than
# mispatch a different code layout.
import sys, os, tempfile, struct

_tbxi = os.environ.get('TBXI_PATCHES')
if _tbxi:
    sys.path.insert(0, _tbxi)
try:
    import cfmtool
except ImportError:
    sys.exit("cfmtool not found. Set TBXI_PATCHES=/path/to/tbxi-patches "
             "(from Elliot Nunn's Mac OS ROM toolchain: https://github.com/elliotnunn/tbxi-patches).")

if len(sys.argv) != 3:
    sys.exit(f"usage: {os.path.basename(sys.argv[0])} <in RockHopper2.pef> <out patched.pef>")
SRC, DST = sys.argv[1], sys.argv[2]

DETOUR    = 0x3788   # cscSwitchMode tail converge point (was: addis r4,r30,1)
RESUME    = 0x378c   # continue here after re-arm + displaced instruction
CSCSETINT = 0x3b50   # cscSetInterrupt handler (the VBL re-enable)

def I(x):  return struct.pack('>I', x & 0xFFFFFFFF)
def br(frm, to, link=0):
    rel = to - frm
    assert -(1 << 25) <= rel < (1 << 25), f"branch out of range: {rel}"
    return I(0x48000000 | (rel & 0x03FFFFFC) | (link & 1))

with tempfile.TemporaryDirectory() as tmp:
    cfmtool.dump(SRC, tmp)
    cp = os.path.join(tmp, 'code')
    code = bytearray(open(cp, 'rb').read())

    # sanity: confirm the offsets are what the disassembly said (guards against a
    # different code layout silently producing a wrong patch)
    assert code[0x3750:0x3754] == b'\x7c\x08\x02\xa6', "0x3750 != mflr (cscSwitchMode?)"
    assert code[0x3b50:0x3b54] == b'\x7c\x08\x02\xa6', "0x3b50 != mflr (cscSetInterrupt?)"
    disp = bytes(code[DETOUR:DETOUR + 4])
    assert disp == b'\x3c\x9e\x00\x01', f"0x3788 != 'addis r4,r30,1' (got {disp.hex()})"

    while len(code) % 4:
        code.append(0)
    cave = len(code)

    # detour: 0x3788 -> cave
    code[DETOUR:DETOUR + 4] = br(DETOUR, cave)

    # cave: re-arm VBL by calling cscSetInterrupt(csMode=0), then the displaced instr
    c = bytearray()
    c += I(0x38000000)                       # li    r0, 0
    c += I(0xb0010040)                       # sth   r0, 64(r1)   VDFlagRecord.csMode=0
    c += I(0x38610040)                       # addi  r3, r1, 64    r3 = &VDFlagRecord
    c += I(0x7fc4f378)                       # mr    r4, r30       r4 = driver context
    c += br(cave + len(c), CSCSETINT, link=1)  # bl  cscSetInterrupt (enable)
    c += disp                                # addis r4, r30, 1    (displaced original)
    c += br(cave + len(c), RESUME)           # b     0x378c
    code += c

    open(cp, 'wb').write(code)
    cfmtool.build(tmp, DST)

print(f"patched OK -> {DST}   cave@{hex(cave)}  cave_size={len(c)}B")
