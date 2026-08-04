# Integrated ROM fix (ATY,RockHopper2)

This builds the VBL re-arm **into the mini's display driver** inside the Mac OS ROM, so it
happens at the instant of the boot resolution switch, before the desktop draws. No app, no
Startup Items, no brief freeze. Because it acts that early, it also has a real chance at the
rare severe variant (garbled screen or hard hang at the switch) that a Startup-Items app can
never reach. See [../TECHNICAL.md](../TECHNICAL.md) for the mechanism.

The change is tiny: it adds 32 bytes and modifies twelve instructions, all inside the
`ATY,RockHopper2` driver. A full dump comparison confirms nothing else in the ROM changes.

## Read this first

- **This is a boot-critical file.** A wrong ROM means that System Folder will not boot. It is
  recoverable (below), but do it on an **expendable/test volume**, not your primary, and keep
  a recovery boot disc or a second bootable volume handy.
- **Back up your current `Mac OS ROM` before you start.** That backup is your one-step recovery.

## Two ways to get it

### A. Prebuilt ROM (most people)

The prebuilt `.hqx` is attached to the **`rom-v1.0` release**. It is the standard MacOS9Lives
mini ROM with this single patch applied.

> **Caveat that matters:** it is safe to drop in **only if the standard MacOS9Lives mini ROM is
> what you already run.** It replaces the whole ROM with mine. If your base ROM is different
> (a different version, a different machine's ROM, or one with other custom patches), **do not
> use the prebuilt ROM.** Use option B instead.

### B. Apply the patch to your own ROM (advanced)

[`rh_vbl_patch.py`](rh_vbl_patch.py) applies the same change to a `RockHopper2` driver you
extract from your own ROM. It uses `cfmtool` from Elliot Nunn's Mac OS ROM toolchain
(`tbxi-patches`). Point it at that toolchain and run:

```
TBXI_PATCHES=/path/to/tbxi-patches ./rh_vbl_patch.py RockHopper2.pef RockHopper2-VBL.pef
```

The script asserts on the exact code offsets of `RockHopper2 1.0.1f63` (the version in the
standard mini ROM). On a different driver version it will stop rather than mispatch, and the
offsets would need re-deriving from that driver. Extracting the driver from a ROM and rebuilding
the ROM around the patched driver is done with the same `tbxi` toolchain.

## Install (prebuilt)

1. **Back up** the current `Mac OS ROM` from the target System Folder to another volume. Name
   it something like `Mac OS ROM ORIG`.
2. Copy the `.hqx` to the mini and expand it with **StuffIt Expander**. It produces a file
   named **`Mac OS ROM`** (type `tbxi`, about 2.9 MB, with a resource fork). If it decoded to
   something tiny or plain, the transfer went wrong; re-copy the `.hqx` (it is plain text).
   Make sure the result is named exactly `Mac OS ROM`.
3. You cannot replace the ROM of the System Folder you are booted from, so **boot from a CD or
   another volume.**
4. In the **target** System Folder, replace `Mac OS ROM` with the patched one. Keep the name
   exactly `Mac OS ROM`.
5. Set the display to the **scaled** resolution that used to freeze, and reboot from the patched
   System Folder.

## Recovery

Boot from the CD or other volume and restore `Mac OS ROM ORIG` over the patched `Mac OS ROM`.
That returns you to stock.

## What to report

Whether it boots cleanly at your scaled resolution, the freeze rate before versus after, and
crucially whether any **hard-hang or garbled** boots still happen. That last point is the one
thing that tells us if the driver-level fix also covers the severe variant, which the app never
could. Open an [issue](../../issues) with your machine, GPU, native and scaled resolutions, and
OS 9 build.
