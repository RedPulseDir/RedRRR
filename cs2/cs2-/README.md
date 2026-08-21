# SpaggetiHop

A tick-perfect bunnyhop for Counter-Strike 2.

Single-file loader — no manual injector, no external DLL to drop in. Launch `SpaggetiHop.exe`, start CS2, hold SPACE, and every jump lands on the frame the server expects.

> **⚠️ Launch `SpaggetiHop.exe` BEFORE `cs2.exe`.** The loader waits for CS2 and injects as it starts up — running it after CS2 is already open is unreliable.


> **Also if anyone who has more knowledge than me in the subject can figure out why the jump timing is not tick perfect feel free to let me know the reason for it. Thanks.**

## Features

- **Tick-synced jumps.** Toggles the jump state once per client tick, so there's no double-press, no skipped hop, no luck involved.
- **Self-contained.** The payload DLL is embedded as a resource inside the EXE; the loader extracts and injects it automatically.
- **Waits for the game.** Launch it before CS2 — it'll sit and inject the moment `cs2.exe` shows up (up to 5 minutes).
- **Clean unload.** Press END in-game and the hook is removed, trampolines are freed, and the DLL unloads itself. No game restart needed.
- **Light obfuscation.** Strings are XOR-encrypted at compile time, module/API resolution is done through PEB walks with FNV1a-hashed names — so none of the obvious giveaways (`client.dll`, `CreateMove`, etc.) appear as plain strings in the binary.

## Usage

1. Download `SpaggetiHop.exe`.
2. Run it **before launching CS2**.
3. Launch CS2. A console opens with a hot-pink banner — when the animation stops, you're injected.
4. In-game: hold **SPACE** to auto-hop.
5. Press **END** to unload.

## Controls

| Key    | Action                          |
|--------|---------------------------------|
| SPACE  | Tick-synced hop                 |
| LMB    | Silent aim (while held)         |
| INSERT | Open/close the menu             |
| END    | Unload the cheat                |

## Menu

**INSERT** opens an ImGui menu rendered inside the game's own swapchain (`IDXGISwapChain::Present` hooked at vtable index 8, `ResizeBuffers` at 13). Everything is toggleable at runtime — no rebuild to turn a feature off:

- **Bunnyhop** — enable, subtick timing, stamina gate
- **Triggerbot** — auto fire while a target sits inside the FOV circle, with a delay slider
- **Autostrafe** — move-based and view-based variants, strafe amount, yaw deadzone
- **Silent aim** — enable, FOV, visible-only filter, max distance, target bone
- **Visuals** — FOV circle on/off, thickness, target highlight, watermark, game FOV used for the pixel conversion

While the menu is open, mouse and keyboard input is swallowed in the WndProc so the game doesn't turn the camera under it, and SPACE goes to the UI instead of the hop.

Defaults are deliberately conservative: bhop and silent aim on, both autostrafe variants off, since those fight the player's own input.

### Resolution handling

- **Drawing follows the back buffer, not the window.** `io.DisplaySize` is overridden from `DXGI_SWAP_CHAIN_DESC` every frame and mouse coordinates are rescaled by the buffer/client ratio, so a non-native render resolution, DSR or a scaled windowed mode doesn't smear the UI or desync the cursor.
- **FOV circle is computed from vertical FOV**: `px_per_rad = (H/2) / tan(vFov/2)`, with `vFov` derived from your CS2 FOV setting treated as horizontal-at-4:3 (Source is Hor+). The circle therefore lines up with the actual aim cone at 4:3, 16:9, 16:10 and 21:9 alike — at a given FOV it always covers the same fraction of screen height.
- **Circle tessellation scales with radius** so it stays round at 4K instead of turning into a visible polygon.
- **UI scales with resolution**, 1080p being 1.0x, clamped to 0.6x–3.0x. Override it under *Interface* if you want it bigger or smaller.
- **Menu is pulled back on screen** whenever the resolution changes, so switching from 4K to 1080p can't strand it off-screen.

## Silent aim

Hold **LMB** and shots leave along the angle to the nearest enemy head inside a 6 deg cone — the camera never moves. **INSERT** toggles the module (it starts enabled).

It hooks `WriteSubtickFromEntry` — the client function that fills a `CCSGOInputHistoryEntry` and then copies its angles into the outgoing usercmd at `[rdx+0x18]` / `[rdx+0x1C]`. The detour rewrites `view_angles` in the entry before calling the original and puts the player's real angles back right after, so the modified angle exists only for the duration of that call. It also fills `shoot_position` and the `target_*` fields, which is what the server cross-checks for hit registration.

If the signature ever stops matching, the module silently falls back to swapping `client.dll + dwViewAngles` around `oCreateMove`. That path still shifts the shot but leaves the hit-reg fields untouched.

Everything lives in `rcs/SilentAim.h`; tunables (`FOV_DEG`, `KEY_AIM`, `MAX_DIST`, `VISIBLE_ONLY`, `OFF_AIM_PUNCH`) sit at the top of the file.

Credit: [TKazer/CS2-External-Silent-AimBot](https://github.com/TKazer/CS2-External-Silent-AimBot) for tracing the pitch/yaw stores, and the input-history approach for the hook target.

Notes:

- Visibility is `m_bSpottedByMask`, i.e. "somebody on my team spotted them", not a real line-of-sight trace. It is off by default because it rejects perfectly shootable targets. A `TraceShape` filter is the correct fix and is not implemented — see below.
- **No wallbang detection.** Deciding "this wall is penetrable with this gun" needs the engine's own trace plus `CCSWeaponBaseVData` penetration values. The signatures exist in `patterns.json` (`TraceShape`, `GameTraceLine`, `pGameTraceManager`, `TraceInitFilter`, `AutowallTraceData`, `HandleBulletPenetration_v2`), but IDA's decompiled prototypes there are wrong (vectors decoded as `int`), and calling an engine trace with a guessed argument layout crashes instantly. Needs correct prototypes before it can be wired.
- Aim-punch compensation is off (`OFF_AIM_PUNCH = 0`) because `m_aimPunchAngle` is missing from the current a2x dump. Drop the real offset in and it starts compensating.

## Signatures

Addresses are resolved by pattern at startup (`rcs/Patterns.h`) instead of trusting static RVAs, since offsets die every game update. `rcs/patterns.json` is the raw scan dump the signatures came from.

| Name | Resolve | Used for |
|------|---------|----------|
| `CreateMove` | raw | bhop hook |
| `WriteSubtickFromEntry` | raw | silent aim hook |
| `pEntityList` | riprel | entity system global |
| `pLocalPlayerController` | riprel | local controller global |

Anything not in that table still comes from `SDK.h` as a static offset.

## Troubleshooting

- **Silent aim does nothing** — most likely `dwViewAngles` or the entity/bone offsets in `SDK.h` went stale after a game update; refresh them from a2x/cs2-dumper (`offsets.hpp` + `client_dll.hpp`).
- **SPACE does nothing in-game** — CS2 likely got an update; the offsets in `SDK.h` are stale and need refreshing from a public dumper (a2x / offsets.hpp).
- **Console closes instantly** — check `bhop_error.log` next to the EXE; that's where startup errors get written.
- **Antivirus flags it** — expected. It's a DLL injector. Whitelist the EXE or pause real-time scanning before running.

## How it works (short version)

The EXE carries the DLL as an embedded resource, extracts it to `%TEMP%`, and injects it into `cs2.exe` via `LoadLibrary` in a remote thread. Once inside, the DLL pattern-scans `client.dll` for `CreateMove`, hooks it with MinHook, and toggles the engine's force-jump flag between PRESS and RELEASE on each new client tick — which is exactly what a pixel-perfect bunnyhop looks like to the server.

## Disclaimer

Use at your own risk. Intended for offline play and community servers. Don't use on VAC-protected matchmaking.
