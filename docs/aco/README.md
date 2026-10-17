# ACO development notes

Source baseline: FlyingMeta's ACGC-PC-Port v0.9.3-playtest, commit `4099d246c927e75b4fd342ca13f4ac4395c55af5`. Preserve upstream history and licensing when extending this fork.

- [Graphics implementation and DLSS plan](graphics-and-dlss.md)
- [Three-mode texture comparison](texture-comparison.md)
- [Co-op/server roadmap](co-op-roadmap.md)
- [Windows/Linux host setup and protocol](../../host/README.md)
- [Host validation results](host-validation.json)
- [Town minimap and HUD options](minimap.md)
- [Texture tooling](../../tools/aco/README.md)

The local build and real-driver integration checks passed. `pc/tests/graphics_smoke.c` tests 60 graphics combinations; `pc/tests/texture_compare_smoke.c` covers original/pack/runtime variants, cache reuse, sampler parity, transparency, addressing, dynamic palettes, absent-pack behavior and resource ownership. Build the `graphics_smoke` and `texture_compare_smoke` CMake targets and run their executables from an SDL2-enabled environment. Neither test requires a game image.

The full game rendered the title/demo in Everything 4x mode, and the native menu switched Original → Pack → Everything 4x. This is not complete gameplay, save, controller, season or multiplayer validation. Injected F6 input was not confirmed in the latest automated UI run; direct menu selection and cycling were confirmed. Full validation reports are included alongside these notes; machine-local runtime logs and game-derived imagery are excluded.

Development uses `aco-native` as the fork's primary branch. `origin` points at the personal fork; `upstream` points at FlyingMeta's repository. Fetch upstream changes and review/merge them deliberately rather than replacing this branch with upstream. Do not push changes to upstream without a separate request.

Keep game images, extracted/upscaled textures, saves, private settings, model weights, binaries and toolchain installations outside version control. The source `.gitignore` excludes the usual runtime paths and file formats. A clean clone needs its own legally obtained supported GAFE01 Rev0 game image and local build dependencies.
