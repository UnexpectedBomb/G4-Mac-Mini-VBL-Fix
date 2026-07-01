# G4 Mac mini — OS 9 startup-freeze fix (Radeon 9200 / scaled resolutions)

A tiny Startup-Items app that fixes the intermittent **frozen-cursor startup hang** on a
G4 Mac mini running Mac OS 9 at a **non-native (scaled) resolution**. No driver patch, no
system-file changes.

> **Status: working fix, seeking testers.** It's confirmed on my mini (revives a frozen
> boot; harmless on healthy boots). Because the freeze is intermittent (~5% of boots),
> broad testing across machines is how we build confidence — see
> [Help test](#help-test-what-to-report). Details of the root cause are in
> [TECHNICAL.md](TECHNICAL.md).

---

## TL;DR

- **Symptom:** at a scaled resolution, the mini occasionally freezes during startup —
  frozen cursor, dead desktop, needs a power-cycle. Native resolution never does this.
- **Cause:** the Radeon 9200 (`ATY,Bee`) driver disables the display's VBL interrupt during
  the boot-time switch into a scaled mode and intermittently never re-enables it. No VBL =
  frozen cursor.
- **Fix:** [`VBLFix_v1`](dist/) re-issues the standard "enable VBL interrupt" call on every
  boot. Drop it in **Startup Items**. It modifies nothing permanent.
- **Caveat:** this fixes the *common* case (frozen cursor). A *rare* severe variant (garbled
  screen / hard hang right at the switch) happens too early for any app to catch — for
  guaranteed stability, **native resolution is still the only sure thing.**

## Do you have this bug?

You're a candidate if **all** of these are true:

- a **G4 Mac mini** (or another Mac on OS 9 with an **ATI Radeon 9200 / RV280**, Apple
  codename `ATY,Bee`),
- running at a **scaled / non-native resolution** (e.g. 1024×640 on a natively-1680×1050
  panel), and
- you get **occasional frozen-cursor hangs during startup** — maybe 1 in 20 — needing a
  power-cycle.

If you only run at your display's **native** resolution, you almost certainly never see this
(native doesn't engage the scaler and is rock-solid).

## Download

Grab one from [**Releases**](../../releases) (recommended — the disk images preserve the
Mac resource forks) or from [`dist/`](dist/):

| File | Use |
|------|-----|
| `VBLFix_v1.img` | **The fix.** Silent everyday version — install this. |
| `VBLAutofix_v1.img` | **Tester build.** Same fix, but reports via beep + log so you can tell us what it did. |

Each `.img` is an Apple Partition Map disk image that mounts on OS 9 with the app (and its
resource fork) intact. (`.bin` MacBinary versions are included too.)

## Install

1. Mount `VBLFix_v1.img` on the mini and copy **VBL Fix** into
   **System Folder → Startup Items**.
2. Restart.

That's it — it runs at every boot, re-arms the VBL interrupt, and quits. On a healthy boot
it's a harmless no-op; on a stuck boot it brings the cursor back. It writes nothing unless
the enable call ever fails (then a line to `VBL Fix Log` in the System Folder).

**Remove / recover:** hold **Shift** during startup (disables Startup Items), then drag
**VBL Fix** out. Nothing else to undo — it's just an app.

## Limitations

- **Common case only.** Rarely the scaled switch fails harder — garbled screen / hard hang
  **at the switch**, before Startup Items ever run. Nothing an app can do reaches that. If you
  get a garbled/hung boot (not just a frozen cursor), that's the severe variant.
- **Native is still the only guaranteed-stable mode.** This makes scaled resolutions *much*
  more reliable, but native avoids the failure entirely.
- A fully seamless, all-cases fix would live inside the driver itself (the root cause is in
  the driver — see [TECHNICAL.md](TECHNICAL.md); shared with the driver maintainers).

## Help test — what to report

The bug is intermittent, so many machines is the only way to really confirm this. If you
have an affected machine:

1. **Confirm you have the bug first.** On **stock** (no fix) at your scaled resolution, note
   roughly how often you get a frozen-cursor startup (e.g. "~1 in 20"). If you never freeze,
   you can't validate the fix — please don't report "no freezes after installing" without a
   before baseline; it's uninterpretable.
2. **Install the fix** — use **`VBLAutofix_v1`** (the tester build) so it reports what it did.
   Beeps: **2** = healthy boot (no-op), **4** = it caught and revived a stuck boot, **5** =
   couldn't revive, **6** = made it worse (should never happen — report immediately), **1** =
   error. Details also go to `VBL Autofix Log` in the System Folder.
3. **Report back** (open an [issue](../../issues)) with: machine + GPU + display native res +
   the scaled res you run + OS 9 build; your before freeze rate; after: number of boots and
   the beeps you saw; and whether any **hard hangs / garbled screens** still happened.

My results so far (warm-restart batch with the fix installed): **10 boots — 9 clean healthy
(harmless no-op), 1 frozen boot caught and revived (0 → 119 VBL events), 0 still stuck, 0
hard hangs, 0 cases of the fix making things worse.** Plus the original proof-of-concept
revive that established the mechanism.

## Building from source

See [BUILD.md](BUILD.md). Sources are in [`src/`](src/) — `vbl-fix/` (silent everyday build)
and `vbl-autofix/` (tester build). Built with [Retro68](https://github.com/autc04/Retro68).

## Credits

Huge thanks to **Elliot Nunn** (MacOS9Lives; NanoKernel reverse-engineering) for pointing
this at the GPU's refresh-rate interrupt early on — that's what cracked it — and to the
**MacOS9Lives** community for the OS 9 builds and patched ATI drivers that make running OS 9
on these machines possible.

## License

[MIT](LICENSE).
