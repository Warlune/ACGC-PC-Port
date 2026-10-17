# Local texture tools

`build-texture-pack.py` extracts source-resolved textures using the game's decoder, runs a local Real-ESRGAN batch and produces a validated 4x DDS pack. It does not upload game data. `inspect-disc.mjs` reads disc metadata and accepts an explicit image path. Game data, generated images and model weights are not distributed here.

The Python tool defaults to the parent of this source checkout as its workspace. Set `ACO_WORKSPACE` to override. It expects the existing workspace layout:

```
workspace/
  native-client/                 # this checkout; another name is supported
  native-runtime/rom/             # supported CISO supplied locally
  tools/                         # generated texture-decode.exe output
  toolchain/msys64/mingw32/
  toolchain/python-packages/      # xxhash if not installed in Python
  toolchain/realesrgan-full/      # executable and models/
  texture-work/                  # generated PNGs and extraction metadata
  texture-packs/                 # generated DDS pack
```

Use Python with Pillow, NumPy and xxhash. The local model is `realesrgan-x4plus-anime`; install it separately from the upstream Real-ESRGAN distribution. The codec requires the native source's configured `pc/build32/` and the MINGW32 toolchain. Create `workspace/tools/` before extraction in a fresh workspace.

From the source root:

```
python tools/aco/build-texture-pack.py extract
python tools/aco/build-texture-pack.py upscale
python tools/aco/build-texture-pack.py validate
python tools/aco/build-texture-pack.py install
node tools/aco/inspect-disc.mjs path/to/disc.ciso
```

The offline pack covers resolved terrain, exterior and furniture textures; it is not a complete inventory. Everything 4x mode supplies runtime filtered fallbacks for other decoded textures. The install phase intentionally refuses to overwrite an existing installed pack. Consult the generated reports and visually review adjoining tile seams before using a regenerated pack.
