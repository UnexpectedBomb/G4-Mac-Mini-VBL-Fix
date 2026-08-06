# Integrated ROM fix (ATY,RockHopper2)

This builds the VBL re-arm **into the mini's display driver** inside the Mac OS ROM, so it
happens at the instant of the boot resolution switch, before the desktop draws. See
[../TECHNICAL.md](../TECHNICAL.md) for the mechanism. The change is tiny: it adds 32 bytes and
modifies twelve instructions, all inside the `ATY,RockHopper2` driver, and a full dump
comparison confirms nothing else in the ROM changes.

> ## Still under test, and the app is not optional
>
> This ROM is **not a complete fix on its own yet.** In testing it has **still missed some
> boots** (frozen cursor). Because it acts at the switch itself, during the fragile early-boot
> window, it can catch a boot the app cannot, but it can also miss.
>
> **Whenever you run this ROM, keep the `VBLFix` app in Startup Items as a backstop.** The app
> fires later in startup, after things settle, and reliably recovers any boot the ROM drops.
> With both in place, a ROM miss becomes a brief auto-recover instead of a hang. Running the ROM
> without the app means a missed boot leaves you with a frozen cursor and a power-cycle.
>
> Whether the ROM reaches the *severe* variant (garbled screen or hard hang at the switch) that
> the app can never reach is still unconfirmed. Please report boot counts.

## Read this first

- **This is a boot-critical file.** A wrong ROM means that System Folder will not boot. It is
  recoverable (below), but do it on an **expendable/test volume**, not your primary, and keep a
  recovery boot disc or a second bootable volume handy.
- **Back up your current `Mac OS ROM` before you start.** That backup is your one-step recovery.
- **Have the `VBLFix` app ready to put in Startup Items** on the same System Folder. Treat it as
  part of installing the ROM, not an afterthought.

## Two ways to get it

### A. Prebuilt ROM (most people)

The prebuilt `.hqx` is attached to the **`rom-v1.0` release**. It is the standard MacOS9Lives
mini ROM with this single patch applied.

> **Caveat that matters:** it is safe to drop in **only if the standard MacOS9Lives mini ROM is
> what you already run.** It replaces the whole ROM with mine. If your base ROM is different (a
> different version, a different machine's ROM, or one with other custom patches), **do not use
> the prebuilt ROM.** Use option B instead.

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

1. **Back up** the current `Mac OS ROM` from the target System Folder to another volume. Name it
   something like `Mac OS ROM ORIG`.
2. Copy the `.hqx` to the mini and expand it with **StuffIt Expander**. It produces a file named
   **`Mac OS ROM`** (type `tbxi`, about 2.9 MB, with a resource fork). If it decoded to
   something tiny or plain, the transfer went wrong; re-copy the `.hqx` (it is plain text). Make
   sure the result is named exactly `Mac OS ROM`.
3. You cannot replace the ROM of the System Folder you are booted from, so **boot from a CD or
   another volume.**
4. In the **target** System Folder, replace `Mac OS ROM` with the patched one. Keep the name
   exactly `Mac OS ROM`.
5. **Put the `VBLFix` app in that System Folder's Startup Items** as the backstop (see the note
   at the top).
6. Set the display to the **scaled** resolution that used to freeze, and reboot from the patched
   System Folder.

## Recovery

Boot from the CD or other volume and restore `Mac OS ROM ORIG` over the patched `Mac OS ROM`.
That returns you to stock.

## What to report

Whether it boots cleanly at your scaled resolution, how many boots you ran, the freeze rate
before versus after, and crucially whether any **hard-hang or garbled** boots still happen. If
you keep the `VBLAutofix` tester in Startup Items, every **beep 4 (revived)** is a boot the ROM
missed and the app caught; counting those tells us how much the ROM is really helping. Open an
[issue](../../issues) with your machine, GPU, native and scaled resolutions, and OS 9 build.
