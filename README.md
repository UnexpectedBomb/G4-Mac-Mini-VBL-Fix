# G4 Mac mini OS 9 startup-freeze fix (Radeon 9200 / scaled resolutions)

Fixes the intermittent **frozen-cursor startup hang** on a G4 Mac mini running Mac OS 9 at a
**non-native (scaled) resolution**. It comes in two forms: a tiny **Startup-Items app** that
changes nothing permanent, and a new **integrated Mac OS ROM** that builds the same fix into
the display driver so it acts at the moment of the boot switch.

> **Status: the app fix is working and community-validated.** Confirmed rock-solid on
> multiple G4 minis (see the [MacOS9Lives thread](https://macos9lives.com/smforum/index.php?topic=7829)):
> one tester ran 30+ restart cycles with zero freezes, and toggling the fix off brought the
> freezing right back. Root cause is in [TECHNICAL.md](TECHNICAL.md).
>
> **The integrated ROM is new (see [rom/](rom/)).** It moves the same re-arm inside the
> display driver, so there is no app and no brief freeze, and it has a real chance at the rare
> severe variant the app cannot reach. It is stable in my own testing (ten boots, five warm
> and five cold, zero freezes) but has not yet had wide field testing. Testers welcome.

---

## TL;DR

- **Symptom:** at a scaled resolution, the mini occasionally freezes during startup with a
  frozen cursor and a dead desktop, needing a power-cycle. Native resolution never does this.
- **Cause:** the mini's ROM display driver (`ATY,RockHopper2`) turns the display's VBL
  interrupt off during the boot-time switch into a scaled mode and intermittently never turns
  it back on. No VBL means a frozen cursor.
- **Fix:** re-issue the standard "enable VBL interrupt" call. The app does this at every boot
  from **Startup Items**; the ROM does it inside the driver at the instant of the switch.
- **Caveat:** the app fixes the *common* case (frozen cursor). A *rare* severe variant
  (garbled screen or hard hang right at the switch) happens too early for any app to catch.
  The ROM acts early enough that it may cover that case too, but that part is not yet
  confirmed. For guaranteed stability, **native resolution is still the only sure thing.**

## Do you have this bug?

You are a candidate if **all** of these are true:

- a **G4 Mac mini** (or another Mac on OS 9 with an **ATI Radeon 9200 / RV280**), whose ROM
  display driver is **`ATY,RockHopper2`**,
- running at a **scaled / non-native resolution** (for example 1024x640 on a natively
  1680x1050 panel), and
- you get **occasional frozen-cursor hangs during startup**, maybe 1 in 20, needing a
  power-cycle.

If you only run at your display's **native** resolution, you almost certainly never see this
(native does not engage the scaler and is rock-solid).

## Which fix should I use?

Three options, most-proven first:

| Option | What it is | Best for |
|--------|------------|----------|
| **App v1** | The original Startup-Items app. Heavily field-tested (the 30+ restart validation above). | Most people. The most-proven option, nothing permanent. |
| **App v2** | Same fix, plus a guard that makes it a clean no-op under the Classic environment (Mac OS X), so it cannot wedge an OS 9 partition booted via Classic on Tiger. Native OS 9 behavior is identical to v1. Still accumulating field testing. | Anyone who ever boots this OS 9 install under Classic. |
| **Integrated ROM** | The same re-arm built into the `ATY,RockHopper2` driver in the Mac OS ROM, acting at the switch itself. No app, no Startup Items, no brief freeze, and a shot at the severe variant. Newest, least field-tested. | People running the standard MacOS9Lives mini ROM who want a seamless, all-in-one fix. See the caveat in [rom/](rom/). |

The app and the ROM do the same thing at heart. The app is the safe, proven, reversible drop-in.
The ROM is the seamless version for those comfortable reflashing their Mac OS ROM.

## Download (app)

Grab one from [**Releases**](../../releases) (recommended, the disk images preserve the Mac
resource forks) or from [`dist/`](dist/):

| File | Use |
|------|-----|
| `VBLFix_v1.img` / `VBLFix_v2.img` | **The fix.** Silent everyday version. v1 is the most-proven; v2 adds the Classic guard. |
| `VBLAutofix_v1.img` / `VBLAutofix_v2.img` | **Tester build.** Same fix, but reports via beep and log so you can tell us what it did. |

Each `.img` is an Apple Partition Map disk image that mounts on OS 9 with the app (and its
resource fork) intact. The `.bin` MacBinary versions are included too.

### Install (app)

1. Mount the `.img` on the mini and copy **VBL Fix** into **System Folder > Startup Items**.
2. Restart.

That is it. It runs at every boot, re-arms the VBL interrupt, and quits. On a healthy boot it
is a harmless no-op; on a stuck boot it brings the cursor back. It writes nothing unless the
enable call ever fails (then a line to `VBL Fix Log` in the System Folder).

**Remove / recover:** hold **Shift** during startup (disables Startup Items), then drag **VBL
Fix** out. Nothing else to undo, it is just an app.

## Download (integrated ROM)

The prebuilt ROM is attached to the **[`rom-v1.0` release](../../releases)**, and the patch
and full install and recovery notes are in **[rom/](rom/)**.

**Important caveat:** the prebuilt ROM is the standard MacOS9Lives mini ROM with this single
driver patch applied. It is safe to drop in **only if that is the ROM you already run**. If
your base ROM is different, do not flash mine over it. Use the included patch, which applies
the same change to your own ROM. Full instructions, including how to back up your current ROM
and recover, are in [rom/](rom/).

## Limitations

- **The app fixes the common case only.** Rarely the scaled switch fails harder, with a
  garbled screen or hard hang **at the switch**, before Startup Items ever run. Nothing an app
  can do reaches that. The integrated ROM acts at the switch itself, so it may cover this case,
  but I have not been able to reproduce a severe boot to confirm it. This is the single most
  useful thing testers can report.
- **Native is still the only guaranteed-stable mode.** Both fixes make scaled resolutions
  *much* more reliable, but native avoids the failure entirely.

## Help test, what to report

The bug is intermittent, so many machines is the only way to really confirm this. If you have
an affected machine:

1. **Confirm you have the bug first.** On **stock** (no fix) at your scaled resolution, note
   roughly how often you get a frozen-cursor startup (for example "~1 in 20"). If you never
   freeze, you cannot validate the fix. Please do not report "no freezes after installing"
   without a before baseline; it is uninterpretable.
2. **Install the fix.** For the app, use **`VBLAutofix`** (the tester build) so it reports what
   it did. Beeps: **2** = healthy boot (no-op), **4** = it caught and revived a stuck boot,
   **5** = could not revive, **6** = made it worse (should never happen, report immediately),
   **1** = error. Details also go to `VBL Autofix Log` in the System Folder. For the ROM, just
   boot and watch: a clean desktop is good; a frozen cursor or a garbled or hung boot is a
   miss worth reporting.
3. **Report back** (open an [issue](../../issues)) with: machine, GPU, display native res, the
   scaled res you run, OS 9 build; your before freeze rate; after: number of boots and the
   beeps you saw; and whether any **hard hangs or garbled screens** still happened.

## Building from source

See [BUILD.md](BUILD.md). App sources are in [`src/`](src/), `vbl-fix/` (silent everyday
build) and `vbl-autofix/` (tester build), built with [Retro68](https://github.com/autc04/Retro68).
The ROM patch is in [rom/](rom/).

## Credits

Huge thanks to **Elliot Nunn** (MacOS9Lives; NanoKernel reverse-engineering and the Mac OS ROM
toolchain) for pointing this at the GPU's refresh-rate interrupt early on, which is what
cracked it, and to the **MacOS9Lives** community for the OS 9 builds and patched ROMs that make
running OS 9 on these machines possible. Thanks also to **n8blz** and **xc68000** for putting
the app through its paces.

## License

[MIT](LICENSE).
