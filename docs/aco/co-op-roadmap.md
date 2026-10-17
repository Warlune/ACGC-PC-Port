# Co-op implementation notes

## Requested behavior

- Native Windows clients, no Dolphin at runtime.
- Dedicated Linux server hosted on a QNAP NAS; exact CPU and QNAP model are still unknown.
- One persistent town available when either player joins, without the other player hosting.
- Two independently controlled characters and shared permissions to build and change the map.
- User's current priority: establish the native PC port first.

## Current baseline

The local native client is FlyingMeta's v0.9.3-playtest, commit `4099d246c927e75b4fd342ca13f4ac4395c55af5`.
The port replaces the GameCube platform/graphics layer and runs the game's C code as native x86 code. It still reads original assets from the user's disc image.
This is a single-player baseline. Upstream's FAQ explicitly says online play is not planned; multiplayer must be implemented in our fork.

## Source observations, not tested multiplayer behavior

- `pc/src/pc_main.c`: native startup and asset loading.
- `pc/src/pc_disc.c`: ISO/GCM and block-mapped CISO reader. This format is not supported by the connected REA `open_binary` tool; local header validation was used instead.
- `pc/src/pc_m_card.c`: native memory-card/GCI storage.
- `pc/src/pc_save_bswap.c`: save-data byte-order conversion.
- `include/m_field_make.h`: `mFM_fg_c` stores an acre's foreground item array.
- `include/m_field_info.h` and `src/game/m_field_info.c`: tile reads and changes, including `mFI_GetBlockUtNum2FG`, `mFI_BlockUtNumtoFGSet`, `mFI_SetFG_common`, `mFI_SetFG2`, and `mFI_SetFGStructure_common`.
- `pc/DOCUMENTATION.md`: native frame loop and platform architecture.

These are candidate integration points. Not all world mutations necessarily pass through these setters; callers, direct writes, placement side effects, collision, and saving still need tracing before implementation.

## Development sequence

1. Native PC baseline: verify rendering, keyboard/controller input, new-game flow, town loading, and save/reload. Build the matching source with 32-bit GCC before making game-code changes.
2. Establish explicit serializable town/player state and independent player identity. Preserve town metadata, acre layout, foreground items, buildings, inventories, and progression. Do not synchronize raw pointers or whole process memory.
3. Add a server with durable town storage, authenticated players, versioned commands, conflict handling, reconnect snapshots, and server time. Determine the QNAP CPU architecture before selecting the deployment image.
4. Connect one native client to the server. Prove tile edits survive server and client restarts without allowing the client's local save to overwrite the shared town.
5. Add the second player actor, independent movement/camera, acre transitions, and interior handling. Shared input into one character is not co-op.
6. Route placement/removal, item transfers, currency changes, NPC state, and time-based updates through validated server commands. Test simultaneous edits, reconnects, duplicate commands, and interrupted saves.
7. Package the headless server for QNAP, mount persistent data, and test both players joining with the other client closed.

## First multiplayer acceptance test

Two native Windows clients connect to the same server. Each controls a distinct visible character. One places an object and the other sees it. Both clients disconnect; the server restarts; either player reconnects and the object remains. Conflicting edits cannot duplicate items or lose a committed change.

No server component has been implemented or deployed yet.
