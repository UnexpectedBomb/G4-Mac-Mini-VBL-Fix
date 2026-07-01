# Technical details — root cause of the OS 9 G4 mini startup freeze

## Symptom

On a G4 Mac mini (A1103) under Mac OS 9.2.2, booting at a **scaled / non-native**
resolution intermittently (~5% of boots) leaves the cursor frozen and the desktop dead,
needing a power-cycle. Booting at the display's **native** resolution is rock-solid
(0 failures across many boots).

## What's failing

Classic Mac OS drives the cursor and much timed UI work off the display's **vertical-blank
(VBL) interrupt** (~60 Hz). If that interrupt stops, the cursor freezes and anything waiting
on a VBL task stalls — but the machine isn't truly crashed (timers keep running underneath).

The startup resolution switch (the screen comes up at one size, then switches to the saved
scaled mode) goes through the driver's mode-switch path, which **turns the VBL interrupt off**
as part of reprogramming the display and relies on a *separate* step to turn it back on.
During boot/restart that re-enable intermittently doesn't happen, and VBL is left off.

A confirmed diagnostic fingerprint: on a frozen boot, a self-installed **slot-VBL task counts
0** while a GPU-independent **Time Manager task keeps counting** (the system is alive; VBL is
not), and **`cscGetInterrupt` reports the interrupt "enabled" (`csMode=0`) while VBL is
actually dead**. The driver's software bookkeeping and the real hardware state have diverged —
the fingerprint of "disabled and never properly re-enabled."

## Which layer

This is a **software init-ordering** issue, not heat and not failing hardware:

- It reproduces on a **warm restart** as well as a cold boot (cold rate ~1/19, warm ~1/6 in
  small samples) — so it's not cold-hardware / PLL-cold-lock.
- Driving the *same* native↔scaled switch **after** the machine is up never triggers it
  (0 starves in 120 in-session switches) — so it's specific to the **boot/restart** init
  path, not the switch operation itself.

## In the driver (`ATY,Bee`, RV280)

The mini's GPU is the Radeon 9200 (RV280), Apple codename `ATY,Bee` — fragment [30] of the
fat soft-loaded `ATI Driver Update` ndrv. Disassembling that fragment (carved out, with a
validated PEF relocation / TOC map):

- The per-ASIC interrupt **handler / enabler / disabler** are function pointers fetched by
  `GetInterruptFunctions` into the driver context: **`+308` = handler, `+312` = enabler,
  `+316` = disabler**.
- `cscSwitchMode` (the mode-set core) **calls the disabler `[+316]`** (one site) and then
  re-installs only the *software* interrupt handler via `InstallInterruptFunctions`
  (enabler/disabler passed NULL = "keep existing"). It **never calls the enabler `[+312]`**.
- The **enabler `[+312]` is called from exactly one site in the whole driver** — the open /
  `cscSetInterrupt` (Control csCode 7) path.

So after a scaled-mode switch the hardware VBL source is off until a `cscSetInterrupt(enable)`
arrives. In steady state that always shows up and sticks; during boot/restart init it
intermittently doesn't, and VBL stays dead → frozen cursor.

## The fix

Re-issue the missing enable ourselves. `Control(refNum, cscSetInterrupt, csMode=0)` re-runs
the open path (`GetInterruptFunctions` + the enabler `[+312]`) and re-arms the hardware VBL.
On hardware this took a starved boot from **vbl=0 to vbl=119** — and it's a harmless no-op on
a healthy boot (re-enabling an already-enabled interrupt). `VBL Fix` simply issues this at
every startup from Startup Items. See [README.md](README.md).

## The severity spectrum (what the fix can't reach)

The scaled switch is a multi-step hardware bring-up (PLL lock → CRTC timing → scaler
[`ExactRatioMode`] → framebuffer → output → VBL enable). *Where* the race loses sets the
severity:

- **VBL enable loses** (common): image + system fine, only the periodic interrupt is missing
  → frozen cursor, system runs underneath. **This is what the fix recovers.**
- **Partial-apply-then-bail**: grey screen, brief flash of life, then freeze.
- **Scaler/framebuffer botched, or a hang waiting on a lock**: garbled scanout of the desktop
  + a hard hang **at the switch**, before Startup Items run.

The last two happen too early for any Startup-Items app to intervene — the machine is wedged
before it loads. Those need a fix inside the driver's switch path itself (re-issue the enable
at the right point once the scaler/CRTC settle, and address whatever hangs the hard-hang
case). That's why **native remains the only fully-robust mode**, and why the driver-side fix
is still worth pursuing.

## Tooling

The measurement instrument installs a self-rearming slot-VBL task (`SlotVInstall` on the boot
display) alongside a Time Manager task and compares counts over a fixed window — the slot-VBL
counter only advances when the driver fires `VSLDoInterruptService`; the Time Manager counter
is GPU-independent ground truth. The tester build ([`src/vbl-autofix/`](src/vbl-autofix/))
uses this to report before/after the enable; the silent everyday build
([`src/vbl-fix/`](src/vbl-fix/)) skips measurement and just issues the enable.
