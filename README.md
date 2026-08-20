# G4 Mac mini OS 9 startup-freeze fix (Radeon 9200 / scaled resolutions)

Fixes the intermittent **frozen-cursor startup hang** on a G4 Mac mini running Mac OS 9 at a
**non-native (scaled) resolution**. The fix is a tiny **Startup-Items app** that changes nothing
permanent and, across hundreds of boots, has never once failed to recover a frozen cursor. There
is also an optional **integrated Mac OS ROM** that builds the same re-arm into the display driver
so it fires earlier in startup, but it is a partial early-boot head-start by design, not a
standalone fix. The app is what makes scaled resolutions reliable; keep it in Startup Items either
way.

> **Status: solved. The Startup-Items app is the definitive fix.** It is community-validated and
> rock-solid on multiple G4 minis (see the [MacOS9Lives thread](https://macos9lives.com/smforum/index.php?topic=7829)):
> one tester ran 30+ restart cycles with zero freezes, and toggling the fix off brought the
> freezing right back. In continued daily use it has recovered the cursor on **every** frozen boot
> across hundreds of restarts, without a single miss. Root cause is in [TECHNICAL.md](TECHNICAL.md).
>
> **The integrated ROM is an optional early-boot head-start, not a replacement for the app (see
> [rom/](rom/)).** It moves the same re-arm inside the display driver so it fires at the resolution
> switch. But the interrupt can drop anywhere from that switch through to the desktop finishing its
> load (we have seen it freeze as late as the login/Keychain dialog), and a fix that fires once, at
> the switch, cannot catch a drop that happens later. In practice the ROM noticeably lowers how
> often the freeze happens, by clearing the drops at or near the switch, but the later ones still
> get through. The app catches every case because it runs at the tail of that window, in Startup
> Items. If you flash the ROM, still keep the app in Startup Items.

---

## TL;DR

- **Symptom:** at a scaled resolution, the mini occasionally freezes during startup with a
  frozen cursor and a dead desktop, needing a power-cycle. Native resolution never does this.
- **Cause:** the mini's ROM display driver (`ATY,RockHopper2`) turns the display's VBL
  interrupt off during the boot-time switch into a scaled mode and intermittently never turns
  it back on. No VBL means a frozen cursor.
- **Fix:** re-issue the standard "enable VBL interrupt" call. The **app** does this at every boot
  from Startup Items, which run at the very end of the window in which the freeze can occur, so it
  catches the drop no matter when it happened, and in our use it has never missed. The **ROM** does
  the same re-arm earlier, inside the driver at the instant of the switch, as an optional head-start.
- **The window:** the interrupt can drop anywhere from the resolution switch to the desktop
  finishing its load, then never again until the next reboot. Only a fix that fires at the end of
  that window catches all of it, which is why the Startup-Items app is complete and a switch-time
  or fixed-delay ROM fix is only partial. Native resolution never enters the window at all, so it
  never sees the bug.

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

**Use the app.** Put it in Startup Items and you are done, it is the complete fix. The ROM is an
optional extra for people who want the re-arm to also fire earlier in boot; it never replaces the
app.

| Option | What it is | Best for |
|--------|------------|----------|
| **App v1** | The original Startup-Items app. Heavily field-tested (the 30+ restart validation above). | Most people. The proven, complete fix, nothing permanent. |
| **App v2** | Same fix, plus a guard that makes it a clean no-op under the Classic environment (Mac OS X), so it cannot wedge an OS 9 partition booted via Classic on Tiger. Native OS 9 behavior is identical to v1. | Anyone who ever boots this OS 9 install under Classic. Otherwise interchangeable with v1. |
| **Integrated ROM** | The same re-arm built into the `ATY,RockHopper2` driver, firing at the switch itself. A partial early-boot head-start by design, not a standalone fix: it catches early drops but misses later ones, so it noticeably lowers the freeze rate without eliminating it, and is always run **with the app**, never instead of it. | Advanced users on the standard MacOS9Lives mini ROM who want the re-arm to fire early as well. See [rom/](rom/). |

The app and the ROM issue the same re-arm; the difference is *when*. The app fires at the end of
the freeze window (Startup Items), so it catches every drop and is the reliable, complete,
reversible fix. The ROM fires at the start of the window (the switch), so it only ever catches the
early drops. Run the ROM with the app, not instead of it.

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

The prebuilt ROM is attached to the **[`rom-v1.0` release](../../releases)**, and the patch and
full install and recovery notes are in **[rom/](rom/)**.

> **This ROM is an optional head-start, and the app is not optional with it.** By design the ROM
> re-arms only at the resolution switch, so it cannot catch a VBL drop that happens later in boot;
> those still show up as a frozen cursor. **Keep the `VBLFix` app in Startup Items whenever you run
> this ROM.** The app fires at the end of the boot window and recovers any drop the ROM did not
> reach. The ROM on its own is not a complete fix; the app is.

**Base-ROM caveat:** the prebuilt ROM is the standard MacOS9Lives mini ROM with this single
driver patch applied. It is safe to drop in **only if that is the ROM you already run**. If your
base ROM is different, do not flash mine over it. Use the included patch, which applies the same
change to your own ROM. Full instructions, including how to back up your current ROM and
recover, are in [rom/](rom/).

## Limitations

- **The ROM is a partial head-start by design, not a standalone fix.** It re-arms only at the
  resolution switch, so it cannot catch a VBL drop that happens later in the boot window; those
  still freeze the cursor until the app recovers them. It does noticeably lower the overall freeze
  rate by clearing the drops at or near the switch, and that it helps without eliminating the
  freeze is itself a sign the drops are spread across the window rather than fixed at the switch.
  Always run the `VBLFix` app in Startup Items alongside it. This is a property of *when* the ROM
  fires, not a bug to be tuned out: no single switch-time or fixed-delay re-arm can cover a window
  whose length varies with the boot volume and with late steps like Keychain and server mounts.
- **The rare garbled/hard-hang variant was a failing GPU, not this bug.** Early on, one older mini
  occasionally came up with a garbled screen or a hard hang right at the switch, which no app could
  reach. That machine later turned out to have serious GPU problems, and since moving to a mini in
  good health the variant has never reappeared across hundreds of boots, where every freeze has
  been the recoverable lost-VBL kind that the app revives. We now attribute the garbled/hang
  behavior to dying display hardware, a separate problem from the VBL race this project fixes.
- **Native resolution never engages the bug.** The app makes scaled resolutions reliable in
  practice; native simply never enters the failure window in the first place, because it does not
  drive the scaler.

## Help test, what to report

The bug is intermittent, so many machines is the only way to really confirm this. If you have an
affected machine:

1. **Confirm you have the bug first.** On **stock** (no fix) at your scaled resolution, note
   roughly how often you get a frozen-cursor startup (for example "~1 in 20"). If you never
   freeze, you cannot validate the fix. Please do not report "no freezes after installing"
   without a before baseline; it is uninterpretable.
2. **Install the fix.** For the app, use **`VBLAutofix`** (the tester build) so it reports what
   it did. Beeps: **2** = healthy boot (no-op), **4** = it caught and revived a stuck boot,
   **5** = could not revive, **6** = made it worse (should never happen, report immediately),
   **1** = error. Details also go to `VBL Autofix Log` in the System Folder. If you are testing
   the ROM, keep `VBLAutofix` in Startup Items too: with the ROM working, healthy boots read as
   no-ops, and every **beep 4 (revived)** is a boot the ROM missed and the app caught. Counting
   those tells us how well the ROM is doing.
3. **Report back** (open an [issue](../../issues)) with: machine, GPU, display native res, the
   scaled res you run, OS 9 build; your before freeze rate; after: number of boots and the beeps
   you saw; and whether any **hard hangs or garbled screens** still happened.

## Building from source

See [BUILD.md](BUILD.md). App sources are in [`src/`](src/), `vbl-fix/` (silent everyday build)
and `vbl-autofix/` (tester build), built with [Retro68](https://github.com/autc04/Retro68). The
ROM patch is in [rom/](rom/).

## Credits

Huge thanks to **Elliot Nunn** (MacOS9Lives; NanoKernel reverse-engineering and the Mac OS ROM
toolchain) for pointing this at the GPU's refresh-rate interrupt early on, which is what cracked
it, and to the **MacOS9Lives** community for the OS 9 builds and patched ROMs that make running
OS 9 on these machines possible. Thanks also to **n8blz** and **xc68000** for putting the app
through its paces.

## License

[MIT](LICENSE).
