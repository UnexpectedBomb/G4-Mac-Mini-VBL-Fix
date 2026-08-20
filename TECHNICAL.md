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

Why that placement is the *complete* fix is a timing argument, covered in
[the freeze window](#the-freeze-window-why-the-app-is-complete) below: Startup Items run at the end
of the interval in which VBL can die, so a single re-arm there is downstream of every possible
drop. In practice the app has recovered the cursor on every frozen boot across hundreds of restarts
without a miss.

## The fix (integrated ROM)

The same re-arm, moved inside the driver so it fires at the switch itself rather than seconds
later from Startup Items. The patch detours the **tail of `cscSwitchMode`** (the converge point
at `0x3788`, reached on both the success and failure paths) into an appended code cave that
calls the `cscSetInterrupt(csMode=0)` handler at `0x3b50`, runs the one displaced instruction,
and returns. So the interrupt is re-armed at the moment of the switch, before the desktop draws.

Because it acts that early, it re-arms VBL before the desktop draws, which is useful as a
head-start. But firing at the switch also bounds what it can do: it re-arms once, at the *start* of
the window in which VBL can die, so any drop that happens later in boot is past it. That is why the
ROM is a partial head-start and not a standalone fix, and why the Startup-Items app, firing at the
*end* of the window, is the one that is complete. See
[the freeze window](#the-freeze-window-why-the-app-is-complete).

The patch is applied with Elliot Nunn's `cfmtool` (dump the driver `ndrv`, append the cave,
rebuild), so relocations and offsets are recomputed correctly. It adds 32 bytes and changes
only twelve instructions, all in `RockHopper2` code. A full dump comparison of the rebuilt ROM
confirms **only the `RockHopper2` driver changed**; every other boot patch (SysEnabler,
NanoKernel, boot script, and the rest) is byte-for-byte untouched. The patch script and install
notes are in [rom/](rom/).

## The freeze window (why the app is complete)

The key property of this bug is *when* it can strike. The lost interrupt is not fixed to the
instant of the switch: VBL can be dead anywhere in the interval from the boot resolution switch
through to the desktop finishing its load. We have watched the cursor move normally well into
startup and then freeze as late as the login/Keychain dialog. Once the desktop is fully up, it
never happens again until the next reboot. So the failure lives in a bounded window with a hard,
event-defined end: **switch ... desktop done.**

That single fact decides which fixes can be complete:

- **The app fires at the end of the window.** Startup Items run as the last phase of boot, after
  extensions, the login/Keychain step, and any server or alias mounts. A re-arm there is downstream
  of every point where VBL could have dropped, so it catches all of them in one shot. This is why
  the app has never missed across hundreds of boots.
- **The ROM fires at the start of the window.** Re-arming at the switch covers a drop that has
  already happened by then, but nothing that happens afterward. It is a genuine head-start, not a
  complete fix: in practice it noticeably lowers the freeze rate by clearing the drops at or near
  the switch, and the fact that it helps without eliminating the freeze is direct evidence that the
  drop timing is spread across the window rather than pinned to the switch.
- **A fixed-delay ROM timer cannot close the gap either.** The window's length is not constant: it
  varies with the boot volume (an SSD reaches the desktop far sooner than a hard disk) and with
  variable late steps like Keychain and network/alias mounts. No fixed delay reliably lands after a
  window whose end moves. Only firing on the boot *event* (Startup Items) does, which is exactly
  what the app does and what makes it hardware-agnostic.

A note on severity. Early in this work, one older mini occasionally failed harder than a lost
interrupt: a garbled screen or a true hang right at the switch, wedged before Startup Items run and
beyond any app. That machine later proved to have serious GPU problems, and after moving development
to a mini in good health the behavior never returned; across hundreds of boots since, every freeze
has been the recoverable lost-VBL kind and the app has revived all of them. We now attribute the
garbled/hang variant to failing display hardware, a separate problem from the VBL race this project
fixes. With the app in Startup Items, scaled resolution has been as reliable in practice as native.

## Tooling

The measurement instrument installs a self-rearming slot-VBL task (`SlotVInstall` on the boot
display) alongside a Time Manager task and compares counts over a fixed window. The slot-VBL
counter only advances when the driver fires `VSLDoInterruptService`; the Time Manager counter is
GPU-independent ground truth. The tester build ([`src/vbl-autofix/`](src/vbl-autofix/)) uses this
to report before and after the enable; the silent everyday build ([`src/vbl-fix/`](src/vbl-fix/))
skips measurement and just issues the enable. The active display driver was identified with a
separate driver-ID probe that reads the live driver's name, version, and NameRegistry node.
