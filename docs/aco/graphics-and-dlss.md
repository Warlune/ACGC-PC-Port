# Graphics expansion

The existing OpenGL 3.3 renderer remains supported. The graphics work adds a presentation stage and menus; it does not replace the game's GX-to-OpenGL renderer.

## Implemented in source

- Native Windows File / Graphics / View / Help menu bar in windowed mode.
- Output resolution choices up to 3840 x 2160, plus the existing custom INI resolutions and in-game resolution list.
- Existing windowed, exclusive fullscreen and borderless modes. Alt+Enter toggles borderless/windowed; the native menu returns in windowed mode.
- Live MSAA off / 2x / 4x / 8x, with native-menu availability checked against the driver limit.
- Optional spatial FXAA.
- Internal render scale 50–200 percent of output dimensions: below 100 upscales, above 100 supersamples.
- Nearest-neighbor, bilinear and Catmull-Rom bicubic spatial scaling.
- Bounded sharpening, 0–100 percent.
- Anisotropic texture filtering off / 2x / 4x / 8x / 16x, clamped to driver support.
- Existing VSync and frame limiting, plus a switch for FPS in the title bar.
- Native, performance and supersampling presets.
- Same persisted settings in the native menu and the existing in-game menu's Video and new Image tabs. Finish pending in-game edits before using native graphics controls.
- Existing texture-pack, audio, control and gameplay settings retained.

These options operate on the scene including its HUD. A future renderer can separate the HUD for cleaner reconstruction. Upscaling at low internal resolution can soften text; 100 percent or supersampling is preferable when performance permits. Increasing the game FPS cap is not frame generation.

The renderer reports effective internal/output dimensions and MSAA in Graphics → Graphics capabilities. Allocation failures fall back to native output without MSAA, and requested render dimensions are limited to the driver texture/renderbuffer maximum and 8192 pixels per axis.

## Real DLSS while retaining OpenGL

DLSS and DLAA remain requested features, but are not implemented in this build. Spatial scaling is not presented as DLSS, FSR or XeSS.

NVIDIA documents DirectX 11/12 or Vulkan integration and per-frame depth, motion vectors, jitter and camera constants:

- https://developer.nvidia.com/blog/how-to-integrate-nvidia-dlss-4-into-your-game-with-nvidia-streamline/
- https://github.com/NVIDIA/DLSS

The intended architecture is additive:

1. Keep OpenGL as a selectable renderer with these graphics options.
2. Establish the process/SDK architecture requirement. The current game port is 32-bit because it casts pointers to 32-bit values; do not assume a modern SDK can be linked into it unchanged.
3. Add a DirectX or Vulkan renderer behind the GX interface. Select the backend before creating the graphics device; retain OpenGL fallback if the optional backend cannot initialize.
4. Track camera and moving-object transforms across frames. Produce valid velocity and depth buffers, jittered projection, exposure and history-reset signals. Acre changes, scene transitions and menus must invalidate temporal history correctly.
5. Integrate the official NVIDIA SDK, query hardware/runtime support, and implement real DLSS quality modes and DLAA. Keep UI composition after reconstruction where practical.
6. Validate image quality and frame pacing in moving scenes, camera transitions, foliage, inventory overlays and interiors. Test both the DLSS backend and the retained OpenGL backend.

The local GPU was detected as an NVIDIA GeForce RTX 3090. Hardware alone does not make DLSS available in the current OpenGL/32-bit client. Frame generation, ray tracing and HDR output each require separate implementation and capability checks.

## Validation

`pc/tests/graphics_smoke.c` runs real OpenGL driver integration tests without game assets. It checks 60 combinations of render scale, MSAA and spatial filter, alternates FXAA/sharpening values, validates resolved color and image orientation, checks GL state restoration, tests resize, and checks anisotropic sampler state. This does not replace visual testing in the full game.

The full modified game was compiled and tested in an isolated folder. The native menu rendered correctly, a performance preset changed the internal render target and MSAA live, and those settings loaded again on restart. It also booted with the original runtime's SDL2.dll. The capabilities dialog correctly reported the RTX 3090 and 600x450 internal / 800x600 output with 2x MSAA. Extended gameplay, visual verification of the in-game Image tab, and fullscreen transitions remain unverified. See `graphics-validation.json` for the build hash and logs.
