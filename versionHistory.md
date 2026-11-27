# Version History

This document records project changes by version. The current version number is
maintained separately in `VERSION`; this file is only a history log.

## 0.1.3 — 2026-11-27

- Included the prebuilt Windows x64 SNESRecomp analyzer in the setup ZIP so
  Easy Setup and the standalone launcher can generate the game without Rust
  or Cargo on the player's PC.

## 0.1.2 — 2026-11-20

- Enabled the castle 1-4 rendering fixes in the Windows x64 release. ER-07
  addresses the extra lines above and below the boss. ER-12 addresses stray
  pixels at the ends of the rotating bar near the lava. Both fixes were
  available in test builds and shipped disabled by default in the 0.1.1
  setup package.
- Added native RecompLauncher Easy Setup. Select your USA `.sfc` ROM in
  RecompLauncher to verify it and compile the game locally. Play launches
  the prepared executable. The ZIP ships without a ROM.

## 0.1.1 — 2026-11-19

- Adapted the Windows x64 setup package for RecompLauncher. Play opens the
  game's setup wizard, where players select their USA ROM and rebuild locally
  using the portable toolchain.
- Included the rebuild dependencies and removed development-only files from
  the distribution. The asset now uses the shorter
  `islandSnesRecomp-0.1.1-windows-x64.zip` filename.

## 0.1.0 — 2026-10-15

- The game is running and playable.
- Gameplay still has visual issues; these remain under investigation.
