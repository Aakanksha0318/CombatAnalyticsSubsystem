# Combat Analytics \& Telemetry Subsystem

**Author:** Aakanksha (Lead) — ID 590013777, Data Science (DATA-SCIENCE-B1)

## Overview

This project implements a Combat Analytics and Telemetry Subsystem in Unreal Engine 5 using C++. It captures gameplay events such as kills, void falls, and arena hazards, computes player performance statistics, tracks nemesis relationships, and exports structured telemetry in JSON format for further analysis using Python and Pandas.

## Video Walkthrough

`https://drive.google.com/file/d/1Y0aLr1dbD6Y9TET4kg\_Bq27MuN-aH7dZ/view?usp=sharing` — walkthrough of telemetry capture logic, async
serialization, and data maps in VS Code, plus a live UE5 editor session
showing the raw JSON log populate in real time.

## What This Is

`UCombatAnalyticsSubsystem` is a `UGameInstanceSubsystem` that:

* Records kills, void falls, and arena hazard triggers.
* Maintains real-time per-player Kill/Death/Void-Fall (K/D/V) stats.
* Maintains a Nemesis dominance matrix (who has killed whom, and how often).
* Buffers records and flushes them asynchronously (thread-pool task, not the
game thread) to `Saved/Analytics/Match\\\_Data\\\_Raw.json` as newline-delimited
JSON (NDJSON) — one JSON object per line, so external tools can stream-parse
without ever loading a single giant array.

## Technologies Used

- Unreal Engine 5.5
- C++
- Visual Studio 2022
- JSON Serialization
- Blueprint Integration

## Files

|File|Purpose|
|-|-|
|`Source/CombatAnalytics/Public/CombatAnalyticsTypes.h`|`FArenaEventRecord`, `FCombatKillRecord`, `FPlayerPerformanceStats` structs + `ToJson()`|
|`Source/CombatAnalytics/Private/CombatAnalyticsTypes.cpp`|`ToJson()` implementations|
|`Source/CombatAnalytics/Public/CombatAnalyticsSubsystem.h`|Subsystem API + thread-safe internal state|
|`Source/CombatAnalytics/Private/CombatAnalyticsSubsystem.cpp`|Hooks, K/D/V + Nemesis logic, async NDJSON writer|
|`Source/CombatAnalytics/CombatAnalytics.Build.cs`|Module dependency snippet (`Json`, `JsonUtilities`)|

## Integration

1. Merge the `Json` / `JsonUtilities` dependencies from `CombatAnalytics.Build.cs`
into your project's actual Build.cs.
2. Drop `CombatAnalyticsTypes.\\\*` and `CombatAnalyticsSubsystem.\\\*` into your
module's `Public`/`Private` folders.
3. The subsystem auto-initializes with the `GameInstance` — no manual
spawning needed. Grab it wherever you need it:

```cpp
UCombatAnalyticsSubsystem\\\* Analytics =
    GetGameInstance()->GetSubsystem<UCombatAnalyticsSubsystem>();
```

4. **On a kill** (call from your damage/death handling code, e.g. in your
`PlayerState` or `GameMode`'s kill-processing function):

```cpp
Analytics->LogKillEvent(
    AttackerPlayerId,   // FString - e.g. AttackerPlayerState->GetUniqueId().ToString()
    VictimPlayerId,
    TEXT("PhoenixBlade"),
    HitLocation,         // FVector
    VictimSoulGaugeLevel // int32
);
```

5. **On a void fall** (call from your out-of-bounds/kill-Z handler):

```cpp
Analytics->LogVoidFall(VictimPlayerId, VictimLocationAtFall);
```

6. **On a hazard trigger** (call from your arena hazard actor/manager):

```cpp
Analytics->LogArenaEvent(TEXT("LavaFlood"), CurrentActivePlayerCount);
```

7. **Reading live stats** (for HUD or an in-editor debug widget):

```cpp
FPlayerPerformanceStats Stats = Analytics->GetPlayerStats(PlayerId);
FString Nemesis = Analytics->GetNemesisFor(PlayerId);
```

8. Call `Analytics->FlushNow()` at match end to guarantee the final buffered
records hit disk immediately rather than waiting for the auto-flush
threshold (default 25 buffered records, tweakable in the subsystem's
`EditDefaultsOnly` properties).

## Output JSON Schema

Output is **NDJSON** at `Saved/Analytics/Match\\\_Data\\\_Raw.json` — one compact
JSON object per line, one line per event. Three `event\\\_type` values appear:

**`kill`**

```json
{"event\\\_type":"kill","timestamp":"2026-07-22T10:14:33.201Z","attacker\\\_id":"P\\\_Kael","victim\\\_id":"P\\\_Rhea","weapon\\\_id":"PhoenixBlade","hit\\\_location":{"x":1240.5,"y":-330.0,"z":88.2},"soul\\\_gauge\\\_level":3}
```

**`kill` (void fall variant — same shape, no attacker)**

```json
{"event\\\_type":"kill","timestamp":"2026-07-22T10:15:02.884Z","attacker\\\_id":"","victim\\\_id":"P\\\_Torin","weapon\\\_id":"VoidFall","hit\\\_location":{"x":-40.0,"y":900.1,"z":-1200.0},"soul\\\_gauge\\\_level":0}
```

**`arena\\\_hazard`**

```json
{"event\\\_type":"arena\\\_hazard","timestamp":"2026-07-22T10:16:10.500Z","hazard\\\_type":"MeteorShower","active\\\_player\\\_count":6}
```

### Analytical utility of the schema

* **`weapon\\\_id` + `attacker\\\_id`/`victim\\\_id` frequency** → weapon pick-rate and
kill-share in Pandas via a simple `groupby("weapon\\\_id").size()`, feeding the
"is Phoenix Blade overperforming" question directly.
* **`hit\\\_location`** → heatmap of kill hotspots per map, useful for spotting
chokepoints or overpowered sightlines.
* **`soul\\\_gauge\\\_level` at death** → correlate a game-specific power resource
with survivability; flags whether the gauge mechanic is actually swinging
fights.
* **`hazard\\\_type` + `active\\\_player\\\_count`** → hazard lethality relative to
how crowded the arena was when it fired, letting designers tune trigger
timing/frequency instead of just raw damage.
* **NDJSON format** → each flush appends complete lines, so a crash mid-match
still leaves a valid, parseable partial log (no truncated JSON array to
repair by hand).

Load it in Pandas with:

```python
import pandas as pd
df = pd.read\\\_json("Match\\\_Data\\\_Raw.json", lines=True)
```
> Note: For demonstration purposes, gameplay events are simulated using Blueprint key bindings. In a production game, these functions would be triggered automatically by the combat, damage, and hazard systems.

## Threading Notes

All public `Log\\\*` calls are safe from the game thread. Internal state
(`PlayerStatsMap`, `NemesisMatrix`, `PendingRecords`) is protected by a
`FCriticalSection`. Buffers are snapshotted under lock and handed to a
`ThreadPool` task for the actual `FFileHelper::SaveStringToFile` disk write,
so file IO never blocks gameplay. `Deinitialize()` performs one final
synchronous flush so end-of-match data is never lost.

