# FortExternalServer

A Fortnite game server that runs as a standalone `.exe` instead of a DLL loaded into the game.

You drop one executable next to `FortniteGame` and `Engine` in a build folder, run it, and it launches the
game, turns it into a listen server and drives the whole match from its own process.

> Independent community project. Not affiliated with, endorsed by, or supported by Epic Games.
> This repository contains no Fortnite game files, no launcher and no backend services.

---

## Branches

This branch (`main`) is documentation only. The server itself lives on a branch per Fortnite version, so a
version can be worked on, broken and fixed without disturbing any other.

| Branch | Target | Status |
| --- | --- | --- |
| `main` | Documentation | You are here |
| `season-3` | 3.6, Chapter 1 Season 3, CL 4019403, UE 4.19 | Active |

To work on the server, check out a version branch:

```
git clone https://github.com/OGFN-Open-sourceing/FortExternalServer
cd FortExternalServer
git checkout season-3
```

New versions branch off an existing one. See [Porting to another version](#porting-to-another-version).

---

## Contents

- [How it works](#how-it-works)
- [Requirements](#requirements)
- [Building](#building)
- [Installing](#installing)
- [Configuring](#configuring)
- [Running](#running)
- [Console commands](#console-commands)
- [Porting to another version](#porting-to-another-version)
- [Troubleshooting](#troubleshooting)
- [Credits](#credits)

---

## How it works

Most Fortnite game servers ship a DLL that gets loaded into the game, so the server code and the game share
one process. FortExternalServer keeps them apart. All of the match logic lives in your executable and reaches
the game from outside.

That split is not total, and it cannot be. A game server has to run code at engine call sites: something must
happen when the net driver ticks, when a control message arrives, when the engine asks what net mode it is in.
So the server writes small pieces of raw x86-64 machine code into the game process and points those call sites
at them. No DLL is produced, nothing extra is written to disk, and nothing appears in the game's module list.

There are two kinds of stub.

**Constant return stubs** replace a function outright with `mov rax, <value>` and `ret`. Used for
`GetNetMode` (always answer listen server), `KickPlayer`, `CollectGarbage` and `NetDebug`. These involve no
communication at all, which matters because `GetNetMode` is called thousands of times per frame.

**Channel stubs** publish their arguments into a shared block and park the game thread in a dispatch pump. While
the game thread sits in that pump, the server can run engine functions on it, which is the only thread UE will
tolerate them on. The server can also rewrite the stub's arguments before the original function runs, and can
tell the stub to skip the original entirely and return a chosen value.

Everything else is ordinary reads and writes into the game's memory, served through a page cache so that walking
large structures stays cheap.

Structure offsets are not hardcoded tables. The server walks the running build's own reflection data
(`UStruct` children, `UProperty` offsets) to find properties and functions by name, so a lookup is either correct
or reports itself as missing.

---

## Requirements

**To run**

- Windows 10 or Windows 11, x64
- A Fortnite build matching the branch you built, including its `FortniteGame` and `Engine` folders
- Administrator is not required in the usual case, but helps if the game is already running and you are
  attaching to it

**To build**

- Visual Studio 2026 with the Desktop development with C++ workload, or
- CMake 3.21 or newer with Ninja, or
- A mingw-w64 toolchain if you are building from macOS or Linux

No third party libraries. No package restore. C++20 and the Windows SDK are all it needs.

---

## Building

### Visual Studio 2026

1. Open `FortExternalServer.sln`
2. Pick **Release** and **x64**
3. Build

The project targets the `v145` toolset that ships with Visual Studio 2026. If you are on an older install and
want to build anyway, override the toolset without editing the project:

```
msbuild FortExternalServer.sln /p:Configuration=Release /p:Platform=x64 /p:FortExternalServerToolset=v143
```

### CMake on Windows

```
cmake --preset vs2026
cmake --build --preset vs2026
```

or, with Ninja and the MSVC toolchain on your path:

```
cmake --preset ninja-release
cmake --build --preset ninja-release
```

### CMake from macOS or Linux

The output is always a Windows executable, so building elsewhere means cross compiling. Install mingw-w64
first:

```
brew install mingw-w64          # macOS
sudo apt install mingw-w64      # Debian or Ubuntu
```

then:

```
cmake --preset mingw-release
cmake --build --preset mingw-release
```

### Make

For quick cross compiles without CMake:

```
make
```

Every route writes `Binaries/Win64/FortExternalServer.exe`.

---

## Installing

Copy the executable into the build folder, the one that has `FortniteGame` and `Engine` directly inside it:

```
YourFortniteBuild/
├── Engine/
├── FortniteGame/
└── FortExternalServer.exe
```

That is the whole install. The server finds the build root by walking up from its own location until it sees
both `FortniteGame` and `Engine`, so a subfolder works too as long as those two are somewhere above it.

---

## Configuring

One header, `Source/Server/Public/Configuration.h`. Edit it and rebuild. There is no ini file and no command
line, deliberately, the same way Erbium does it.

```cpp
struct FConfiguration
{
    static inline auto Playlist = "Playlist_DefaultSolo";
    static inline auto MapToLoad = "Athena_Terrain";
    static inline auto StartingLoadout = "WID_Harvest_Pickaxe_Athena_C_T01";
    static inline auto Port = 7777;
    static inline auto MaxTickRate = 30;
    static inline auto MaxPlayers = 100;
    static inline auto TeamSize = 1;
    static inline auto MinimumPlayers = 2;
    static inline auto WarmupTime = 120;
    static inline auto bSessions = false;
    static inline auto bJoinInProgress = false;
    static inline auto bFriendlyFire = false;
    static inline auto bHealthRegen = false;
    static inline auto bSpectateAfterDeath = true;
    static inline auto bAttachToRunningProcess = false;
    static inline auto bSkipVersionCheck = false;
    static inline constexpr auto bEnableConsole = true;
    static inline constexpr auto bVerboseLogs = false;
};
```

| Option | Meaning |
| --- | --- |
| `Playlist` | Short name like `Playlist_DefaultSolo`, or a full object path. Short names are expanded for you. |
| `MapToLoad` | Map to travel to. |
| `StartingLoadout` | Comma separated item definition names given to every player on spawn. |
| `Port` | Port players connect on. |
| `MaxTickRate` | Server tick rate. |
| `MaxPlayers` | Session capacity. |
| `TeamSize` | Players per team. `1` solo, `2` duos, `4` squads. |
| `MinimumPlayers` | Players needed before the warmup countdown starts. `0` starts with nobody. |
| `WarmupTime` | Warmup countdown in seconds. `0` skips straight to the bus. |
| `bSessions` | Off, the server picks teams itself and sets SquadId from the team index. On, a team already assigned through the game session is kept and SquadId is left alone. |
| `bJoinInProgress` | Allow players to join after the match has started. |
| `bFriendlyFire` | Allow teammates to damage each other. |
| `bHealthRegen` | Leave the health and shield regeneration effects in place. |
| `bSpectateAfterDeath` | Let eliminated players spectate. |
| `bAttachToRunningProcess` | Wait for a game you started yourself instead of launching one. |
| `bSkipVersionCheck` | Run against a build whose changelist does not match this branch. |
| `bEnableConsole` | Enable the console keys listed below. |
| `bVerboseLogs` | Verbose logging instead of display level. |

`StartingLoadout` takes several entries:

```cpp
static inline auto StartingLoadout = "WID_Harvest_Pickaxe_Athena_C_T01,WID_Shotgun_Standard_Athena_UC_Ore_T03,Athena_Shields";
```

Names resolve directly, then as `Name.Name`, then load on demand. Anything unresolvable is logged and skipped
rather than failing the spawn.

The resolved configuration is printed at startup under the `Config` category, so you can always confirm what
the build you are running actually used.

Values that describe the build rather than a preference, storm timings, flight time, starting health and
shield, backpack size, live in the build profile instead. See
[Porting to another version](#porting-to-another-version).

---

## Running

Double click the executable, or from a terminal in the build folder:

```
FortExternalServer.exe
```

There are no arguments. Everything comes from `Configuration.h` at build time.

Startup runs through these stages, each named in the log if something goes wrong:

1. Resolve the build root
2. Launch or attach to the game process
3. Read the game module and resolve engine globals
4. Install the listen server hooks and confirm the game thread reaches the pump
5. Check the build version
6. Wait for the front end, then travel to the Athena map
7. Prepare the match and start accepting players

From there players connect to your address on the configured port.

---

## Console commands

Single keypress, in the server console window.

| Key | Action |
| --- | --- |
| `H` | List commands |
| `S` | Stage, player count, alive count, current phase |
| `P` | List connected players with team and alive state |
| `B` | Skip the warmup countdown and start the match now |
| `E` | End the match |
| `D` | Dump every loaded object to `Saved/ObjectDump.txt` |
| `Q` | Shut down |

The object dump is the tool to reach for when a playlist, item or class name is not resolving. Dump, search the
file for what you expected, and use the exact name it reports.

---

## Porting to another version

The codebase is built to be branched. Almost everything is version independent: the memory layer, the hook
machinery, reflection, the match flow, teams, inventory and the storm all work off names and reflection rather
than build specific constants.

To start a new version, branch from the closest existing one:

```
git checkout season-3
git checkout -b season-4
```

Then work through these, in this order:

**1. `Source/FortniteGame/Private/Versioning/Season3BuildProfile.cpp`**

Rename it for your version and update the values inside. This one file holds the version number and changelist,
the UObject and UProperty layout offsets, every asset path, the ability sets, the storm phase table, the known
playlists, the world constants and the match tuning values, flight time, storm delay, end of match delay,
starting and maximum health and shield, and backpack size. For a nearby version this is often the only file
that needs real changes.

**2. `Source/Runtime/CoreUObject/Public/UObject/UnrealLayout.h`**

Engine structure offsets. Stable within an Unreal version. Changing Unreal version is where these move, in
particular `UStruct` and `UProperty` layout on 4.22 and later, and again on 4.25 where properties become
`FField`.

**3. `Source/Runtime/CoreUObject/Public/UObject/CoreUObjectSignatures.h`**

Byte patterns for `GObjects`, `StaticFindObject`, `FName` and `ProcessEvent`. `ProcessEvent` is resolved by
finding candidates and then confirming one against a real object's virtual table, so it usually survives small
version changes.

**4. `Source/Runtime/Engine/Public/Engine/EngineSignatures.h`**

Byte patterns for the networking functions. This is where most of the porting work goes. Anything that fails to
resolve is named individually in the startup log, so run it once and the log tells you exactly which patterns
need attention rather than making you guess.

Leave `bSkipVersionCheck` off for a branch once it works. It is what stops someone running a Season 3 build
against a Season 4 game and getting confusing crashes instead of a clear message.

---

## Troubleshooting

**"FortExternalServer.exe must sit in the build folder that contains FortniteGame and Engine"**

The executable is not inside a Fortnite build. It walks upward looking for both folders and did not find them.

**"The game executable was not found at ..."**

`[Process] GameExecutable` does not match your build. Some builds use a different name or path under
`FortniteGame/Binaries/Win64/`. Point the setting at the real one.

**"Failed to resolve every required engine global"**

Byte patterns did not match this build. The log lists which ones. Either you are on a version this branch does
not target, or the patterns need updating for it. See [Porting to another version](#porting-to-another-version).

**"The game thread never reached the bridge pump"**

The net driver tick hook is not firing, so nothing drives the server. Usually means the `NetDriverTickFlush`
pattern matched the wrong function, or did not match at all. Check the unresolved function list above it in the
log.

**"This build reports changelist X but the server targets Y"**

Exactly what it says. Use the branch built for that changelist, or set `bSkipVersionCheck` in
`Configuration.h` to try anyway.

**Players connect but never spawn**

Look for `PlayerBootstrap` lines in the log. A missing player pawn class or an unresolvable loadout entry is
reported there. Use the `D` object dump to confirm the real names in your build.

**Poor frame rate once the server is running**

Lower `MaxTickRate`. The game thread waits on the server for each match update, so a higher rate costs frame
time.

---

## Credits

Built by studying the open source Fortnite server projects that came before it:

- [PongooDev/Core](https://github.com/PongooDev/Core) — version handling, and the `MapToLoad` and
  `bSessions` options
- [plooshi/Erbium](https://github.com/plooshi/Erbium) — module layout, signature resolution and the single
  header configuration style
- [Ducki67/FN-Gameserver-Center](https://github.com/Ducki67/FN-Gameserver-Center) — the Season 3 sources that
  the byte patterns and match flow were derived from, Raider 3.5 in particular

If you use this project, credit it and the projects above.

## License

MIT. See [LICENSE](LICENSE).
