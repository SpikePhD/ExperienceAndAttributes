# Simple Alternate Levelling - AGENTS.md

`AGENTS.md` is the single source of truth for this project. `CLAUDE.md` is intentionally removed to avoid drift.

## What this mod does

Replaces Skyrim AE's vanilla skill-based leveling with an XP-from-actions system.
The player levels up by doing things in the world (killing enemies, completing quests,
discovering locations, reading books, picking locks, pickpocketing) rather than by
grinding individual skills. Skill XP is intercepted and discarded at the engine level.

## Architecture

```text
SKSEPluginLoad()
├── InitializeLog()          - timestamped spdlog, mirrored to Logs/ + SKSE/Spike/
├── Config::Load()           - reads SimpleAlternateLevelling.json immediately
├── Serialization callbacks  - cosave v5: persists s_currentXP + skillsNormalized
└── MessagingInterface kDataLoaded
    ├── SkillHook::Install()          - trampolines into PlayerCharacter::AddSkillExperience
    │                                   (discards all organic skill XP) and
    │                                   TESObjectBOOK::Activate (book XP via deferred task)
    ├── EventSinks::Register()        - BSTEventSink registrations:
    │   ├── TESTrackedStatsEvent      - locations, dungeons, locks, skill books,
    │   │                               misc quests, pickpocket, level-up threshold clamp
    │   ├── TESDeathEvent             - kill XP, per-type (keyword-based), level-delta bonus
    │   ├── TESQuestStageEvent        - quest completion XP by type (main/side/faction/...)
    │   └── TESLockChangedEvent       - caches lock difficulty for the "Locks Picked" handler
    ├── CharCreateWatcher             - MenuOpenCloseEvent sink; fires NormalizeSkills after
    │                                   "RaceSex Menu" or "RaceMenu" closes on a new game
    └── GameSettingCollection         - overrides fXPLevelUpBase + fXPLevelUpMult to match
                                        Config curve; also writes levelThreshold directly
                                        (stored value is baked at char creation, not updated
                                        retroactively by game setting changes)
```

### Key files

| File | Role |
|---|---|
| `src/main.cpp` | Plugin entry, log init, cosave callbacks, kDataLoaded orchestration, CharCreateWatcher |
| `src/Config.cpp` / `include/Config.h` | JSON loader; all XP values as `inline` globals |
| `src/XPManager.cpp` / `include/XPManager.h` | `AwardXP()` (native XP bucket feed), kill/quest dedup guards, cosave accessors |
| `src/SkillHook.cpp` / `include/SkillHook.h` | `write_branch<5>` hooks: AddSkillExperience (discard), TESObjectBOOK::Activate (book XP) |
| `src/EventSinks.cpp` / `include/EventSinks.h` | All BSTEventSink structs + `Register()` |
| `include/PCH.h` | Precompiled header: RE/Skyrim.h, SKSE, spdlog sinks, std includes |
| `data/SKSE/Plugins/SimpleAlternateLevelling.json` | Runtime config (XP values, leveling curve, debug flags) |

### XP flow

```text
Action in game
  -> Hook / Event sink fires on main thread
  -> XPManager::AwardXP(amount, source)
      -> skills->data->xp += amount          (native engine XP bucket)
      -> engine checks xp >= levelThreshold  (every tick, natively)
      -> AdvanceLevel() fires natively       (attribute screen, perk point, overflow carry)
  -> "Level Increases" TrackedStat fires
      -> clamp levelThreshold to xpCap
```

### Leveling formula

```text
threshold(level) = min(xpCap, xpBase + level * xpIncrease)
```

`xpBase` -> `fXPLevelUpBase`, `xpIncrease` -> `fXPLevelUpMult`.
Written to both the game setting and `skills->data->levelThreshold` on kDataLoaded
and on each cosave load. The stored threshold is not retroactively updated by game
setting changes alone - it must be written directly.

### Cosave

- Record ID: `EAXP`, version 5
- Payload: `float` cumulative XP + `int` pendingSkillPoints + `uint8` skillsNormalized
- On load: restores `skills->data->xp` and recalculates `levelThreshold`
- v1-v4 fields are intentionally not read

### Hook addresses (AE 1.6.1170)

| Function | RELOCATION_ID / VTABLE | AE ID | Hook type |
|---|---|---|---|
| `PlayerCharacter::AddSkillExperience` | `RELOCATION_ID(39413, 40488)` | 40488 | `write_branch<5>` |
| `TESObjectBOOK::Activate` | `VTABLE_TESObjectBOOK[0]` (AE 189577) slot 37 | - | `write_vfunc` |

`AddSkillExperienceHook` uses the default trampoline (64 bytes, about 14 bytes used).
`BookActivateHook` patches the vtable directly, so it does not consume trampoline bytes.

Why vtable hook for books: `po3_PapyrusExtender` uses `write_branch<5>` on
`TESObjectBOOK::Read` (ID 17842). Two `write_branch<5>` hooks on the same address corrupt
each other's trampolines and cause an access violation on book activation. Hooking `Activate`
is collision-free. `IsRead()` is still false inside `Activate` before the original is called.

### Important gotchas

- `PlayerCharacter::skills` is not a direct member. Use `player->GetInfoRuntimeData().skills`.
- `skills->data->levelThreshold` is baked at character creation; setting game settings does
  not retroactively update it. Write directly in `OnDataLoaded` and `OnGameLoad`.
- `RE::DebugNotification` must not be called from inside `TESObjectBOOK::Activate`'s call stack;
  defer via `SKSE::GetTaskInterface()->AddTask()`.
- `"Books Read"` TrackedStat is dead in AE 1.6.1170. Use `TESObjectBOOK::Activate` vtable hook.
- `"Skill Books Read"` TrackedStat fires for skill books in AE; `"Books Read"` does not.
- Misc quests never set `IsCompleted()`. Award their XP from `"Misc Objectives Completed"` only.
- `QUEST_DATA::Type::kCompanions` does not exist. Use `kCompanionsQuest`.
- `TESActorValueChangeEvent` and `TESPerkEntryRunEvent` have no struct definitions in this
  CommonLibSSE-NG build; those sinks are commented out.

## Build

```bash
cmake -B build -S . \
  "-DCMAKE_TOOLCHAIN_FILE=C:/Program Files/Microsoft Visual Studio/2022/Community/VC/vcpkg/scripts/buildsystems/vcpkg.cmake" \
  -DVCPKG_TARGET_TRIPLET=x64-windows-static-md \
  -DCMAKE_BUILD_TYPE=Release \
  "-DSKYRIM_PATH=C:/Modlist/NGVO/mods/Simple Alternate Levelling" \
  -DBUILD_TESTS=OFF

cmake --build build --config Release
```

DLL is auto-copied to `C:\Modlist\NGVO\mods\Simple Alternate Levelling\SKSE\Plugins\`
on a successful build.

Critical build/runtime note:

- Use `x64-windows-static-md`. This builds `spdlog` and `fmt` as static libraries so the
  plugin does not import `spdlog.dll` or `fmt.dll` at load time.
- Keep `CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"` set before
  `project()` so the plugin matches the triplet's `/MD` runtime.
- Do not use `x64-windows` for this project.

## Paths

| Thing | Path |
|---|---|
| Source | `C:\Users\lucac\Documents\MyProjects\Simple Alternate Levelling\` |
| Build output | `build\Release\SimpleAlternateLevelling.dll` |
| Deploy target | `C:\Modlist\NGVO\mods\Simple Alternate Levelling\SKSE\Plugins\` |
| SKSE log (game) | `Documents\My Games\Skyrim Special Edition\SKSE\Spike\SimpleAlternateLevelling_<ts>.log` |
| Dev log mirror | `<project root>\Logs\SimpleAlternateLevelling_<ts>.log` |
| Config (game) | `<Skyrim>\Data\SKSE\Plugins\SimpleAlternateLevelling.json` |

## Environment

The development machine has the following tools on PATH:

- CMake - `C:\Program Files\CMake\bin`
- Papyrus compiler - `C:\Modlist\papyrus-compiler` and `C:\Modlist\papyrus-compiler\Original Compiler`
- FFDec - `C:\Program Files (x86)\FFDec`

## NGVO modlist tools

These are launched through MO2 so they see the virtual data folder:

| Tool | Path | Use |
|---|---|---|
| SSEEdit 64 | `tools\SSEEdit\SSEEdit64.exe` | Inspect FormIDs, verify records, build conflict reports, run edit scripts |
| SSEDump 64 | `tools\SSEEdit\SSEDump64.exe` | CLI record dump |
| Synthesis | `tools\Synthesis\Synthesis.exe` | Patcher pipeline |
| Cathedral Assets Optimizer | `tools\Cathedral Assets Optimizer\` | Texture/mesh optimization, BSA packing |
| NifSkope | `tools\Nifskope Dev 8 [Pre-Release]\` | Inspect/edit `.nif` mesh files |
| NIF Optimizer | `tools\NIF Optimizer\` | Batch-optimize NIF meshes for SSE |
| LOOT | `tools\LOOT\` | Plugin load order sorting |
| DynDOLOD | `tools\DynDOLOD\` | Dynamic distant LOD generation |
| xLODGen | `tools\xLODGen\` | Terrain/object LOD generation |
| zEdit | `tools\zEdit\` | Merge plugins, apply zPatch rules |
| BethINI | `tools\BethINI\` | INI editor with SSE presets |
| EasyNPC | `tools\EasyNPC\` | NPC appearance conflict resolution |
| Vram Texture Analyzer | `tools\Vram Texture Analyzer\` | Texture VRAM usage profiling |
| PGPatcher / PCA | `tools\PGPatcher\`, `tools\PCA\` | Particle/physics patchers |

## Json for testing

Make sure that the deployed config JSON at
`C:\Modlist\NGVO\mods\Simple Alternate Levelling\SKSE\Plugins\SimpleAlternateLevelling.json`
has the following:

```json
{
  "debug": {
    "verbose": true
  },
  "leveling": {
    "xp_base": 5.0,
    "xp_increase": 1.0
  }
}
```
