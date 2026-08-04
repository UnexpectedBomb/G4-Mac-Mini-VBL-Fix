# Technical details: root cause of the OS 9 G4 mini startup freeze

## Symptom

On a G4 Mac mini (A1103) under Mac OS 9.2.2, booting at a **scaled / non-native** resolution
intermittently (about 5% of boots) leaves the cursor frozen and the desktop dead, needing a
power-cycle. Booting at the display's **native** resolution is rock-solid (0 failures across
many boots).

## What is failing

Classic Mac OS drives the cursor and much timed UI work off the display's **vertical-blank
(VBL) interrupt** (about 60 Hz). If that interrupt stops, the cursor freezes and anything
waiting on a VBL task stalls, but the machine is not truly crashed (timers keep running
underneath).

The startup resolution switch (the screen comes up at one size, then switches to the saved
scaled mode) goes through the driver's mode-switch path, which **turns the VBL interrupt off**
as part of reprogramming the display and relies on a *separate* step to turn it back on.
During boot or restart that re-enable intermittently does not happen, and VBL is left off.

A confirmed diagnostic fingerprint: on a frozen boot, a self-installed **slot-VBL task counts
0** while a GPU-independent **Time Manager task keeps counting** (the system is alive, VBL is
not), and **`cscGetInterrupt` reports the interrupt "enabled" (`csMode=0`) while VBL is
actually dead**. The driver's software bookkeeping and the real hardware state have diverged:
the fingerprint of "disabled and never properly re-enabled."

## Which layer

This is a **software init-ordering** issue, not heat and not failing hardware:

- It reproduces on a **warm restart** as well as a cold boot (cold rate about 1/19, warm about
  1/6 in small samples), so it is not cold-hardware or a PLL cold-lock.
- Driving the *same* native-to-scaled switch **after** the machine is up never triggers it
  (0 starves in 120 in-session switches), so it is specific to the **boot/restart** init path,
  not the switch operation itself.

## In the driver (`ATY,RockHopper2`, RV280)

The mini's GPU is the Radeon 9200 (RV280). Its active display driver is **`ATY,RockHopper2`**,
an Mac OS X 10.3.6 `ndrv` baked into the mini's Mac OS ROM (version `1.0.1f63`). This was
pinned down on hardware with a driver-ID probe that reads the live driver name, version, and
NameRegistry node from the booted mini. (An earlier pass had analyzed the `ATY,Bee`
acceleration extension, assuming it was the display driver; the probe showed `ATY,Bee` is
dormant on this device and `RockHopper2` is the one actually driving the display.)

Disassembling `RockHopper2` (carved from the ROM, with a validated PEF relocation and TOC map):

- Its interrupt **enabler** and **disabler** are a balanced **open / close pair**, dispatched
  by `DoDriverIO` as the Open and Close command handlers. Neither is called from the mode-switch
  path.
- **`cscSwitchMode`** (Control csCode 10, the mode-set handler at `func@0x3750`) reprograms the
  CRTC, PLL, and scaler. That reprogram is what **stops the hardware vblank interrupt**, and the
  switch **never re-arms it**.
- The **only re-arm in the whole driver** is **`cscSetInterrupt`** (Control csCode 7,
  `func@0x3b50`): with `csMode=0` it sets the driver's enabled flag and writes the hardware
  interrupt-enable.

So after a scaled-mode switch the hardware VBL source is off until a `cscSetInterrupt(enable)`
arrives. In steady state that always shows up and sticks; during boot or restart init it
intermittently does not, and VBL stays dead, which is the frozen cursor.

(This is a different mechanism than the earlier `ATY,Bee` reading, where a disabler was called
explicitly inside `cscSwitchMode`. In `RockHopper2` the switch stops VBL as a side effect of
the CRTC/PLL reprogram rather than an explicit disable call. The observable failure and the fix
are the same.)

## The fix (app)

Re-issue the missing enable ourselves. `Control(refNum, cscSetInterrupt, csMode=0)` re-runs the
enable path and re-arms the hardware VBL. On hardware this took a starved boot from **vbl=0 to
vbl=119**, and it is a harmless no-op on a healthy boot (re-enabling an already-enabled
interrupt). `VBL Fix` simply issues this at every startup from Startup Items. See
[README.md](README.md).

## The fix (integrated ROM)

The same re-arm, moved inside the driver so it fires at the switch itself rather than seconds
later from Startup Items. The patch detours the **tail of `cscSwitchMode`** (the converge point
at `0x3788`, reached on both the success and failure paths) into an appended code cave that
calls the `cscSetInterrupt(csMode=0)` handler at `0x3b50`, runs the one displaced instruction,
and returns. So the interrupt is re-armed at the moment of the switch, before the desktop draws.

Because it acts that early, it has a real chance at the severe variant below that no
Startup-Items app can reach, though that has not yet been confirmed on hardware.

The patch is applied with Elliot Nunn's `cfmtool` (dump the driver `ndrv`, append the cave,
rebuild), so relocations and offsets are recomputed correctly. It adds 32 bytes and changes
only twelve instructions, all in `RockHopper2` code. A full dump comparison of the rebuilt ROM
confirms **only the `RockHopper2` driver changed**; every other boot patch (SysEnabler,
NanoKernel, boot script, and the rest) is byte-for-byte untouched. The patch script and install
notes are in [rom/](rom/).

## The severity spectrum (what the app cannot reach)

The scaled switch is a multi-step hardware bring-up (PLL lock, CRTC timing, scaler, framebuffer,
output, VBL enable). *Where* the race loses sets the severity:

- **VBL enable loses** (common): image and system fine, only the periodic interrupt is missing,
  so the cursor freezes with the system running underneath. **This is what the fix recovers.**
- **Partial-apply-then-bail:** grey screen, brief flash of life, then freeze.
- **Scaler or framebuffer botched, or a hang waiting on a lock:** garbled scanout of the desktop
  plus a hard hang **at the switch**, before Startup Items run.

The last two happen too early for any Startup-Items app to intervene; the machine is wedged
before it loads. Those need a fix inside the driver's switch path itself, which is exactly where
the integrated ROM acts. Whether re-arming at the switch tail is early enough to also clear the
partial-apply and hard-hang cases is the open question the ROM is meant to answer, and the main
thing wider testing can settle. Until then, **native remains the only fully-robust mode.**

## Tooling

The measurement instrument installs a self-rearming slot-VBL task (`SlotVInstall` on the boot
display) alongside a Time Manager task and compares counts over a fixed window. The slot-VBL
counter only advances when the driver fires `VSLDoInterruptService`; the Time Manager counter is
GPU-independent ground truth. The tester build ([`src/vbl-autofix/`](src/vbl-autofix/)) uses this
to report before and after the enable; the silent everyday build ([`src/vbl-fix/`](src/vbl-fix/))
skips measurement and just issues the enable. The active display driver was identified with a
separate driver-ID probe that reads the live driver's name, version, and NameRegistry node.
