// CombatAnalyticsSubsystem.h
//
// UCombatAnalyticsSubsystem
// --------------------------
// Game-instance-scoped subsystem that captures combat/hazard telemetry, maintains
// real-time per-player K/D/V stats and a Nemesis dominance matrix, and asynchronously
// flushes buffered records to disk as newline-delimited JSON for external
// Python/Pandas balancing pipelines.
//
// Thread-safety: all public Log*/Get* functions are safe to call from the game
// thread. Internal buffers are protected by a critical section so the async
// flush (running on the task graph thread pool) never touches game-thread data
// races. Buffer snapshots are copied under lock and released before file IO,
// so disk writes never block gameplay.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CombatAnalyticsTypes.h"
#include "CombatAnalyticsSubsystem.generated.h"

class FJsonObject;

UCLASS()
class COMBATANALYTICS_API UCombatAnalyticsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// -- USubsystem interface --
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// -- Telemetry hooks (call these from gameplay code) --

	/** Records a kill, updates attacker/victim K/D stats, and updates the Nemesis matrix. */
	UFUNCTION(BlueprintCallable, Category = "Analytics")
	void LogKillEvent(const FString& AttackerId, const FString& VictimId, const FString& WeaponId, FVector HitLocation, int32 SoulGaugeLevel);

	/** Records a void-fall death (no attacker). Counts toward the victim's V in K/D/V. */
	UFUNCTION(BlueprintCallable, Category = "Analytics")
	void LogVoidFall(const FString& VictimId, FVector HitLocation);

	/** Records an arena hazard trigger (Lava Flood, Meteor Shower, etc). */
	UFUNCTION(BlueprintCallable, Category = "Analytics")
	void LogArenaEvent(const FString& HazardType, int32 ActivePlayerCount);

	// -- Real-time queries (for HUD / dashboard / debug use) --

	/** Returns a copy of the current stats for a player. Returns a zeroed struct if unseen. */
	UFUNCTION(BlueprintCallable, Category = "Analytics")
	FPlayerPerformanceStats GetPlayerStats(const FString& PlayerId) const;

	/** Returns the PlayerId who has killed the given victim the most, or empty string if none. */
	UFUNCTION(BlueprintCallable, Category = "Analytics")
	FString GetNemesisFor(const FString& VictimId) const;

	/** Forces an immediate async flush of buffered records to disk, e.g. on match end. */
	UFUNCTION(BlueprintCallable, Category = "Analytics")
	void FlushNow();

protected:
	/** Number of buffered records that triggers an automatic async flush. */
	UPROPERTY(EditDefaultsOnly, Category = "Analytics")
	int32 FlushThreshold = 25;

	/** Relative-to-project-saved-dir output path for the raw telemetry log. */
	UPROPERTY(EditDefaultsOnly, Category = "Analytics")
	FString OutputRelativePath = TEXT("Analytics/Match_Data_Raw.json");

private:
	// -- Internal helpers (game thread only unless noted) --

	FPlayerPerformanceStats& GetOrCreateStats_Locked(const FString& PlayerId);
	void RegisterNemesisKill_Locked(const FString& AttackerId, const FString& VictimId);
	void BufferRecord_Locked(TSharedRef<FJsonObject> RecordJson);
	void MaybeAutoFlush();

	/** Kicks off an async task-graph job that snapshots the buffer and writes it to disk. */
	void DispatchAsyncFlush();

	/** Runs on a background thread pool worker. Appends NDJSON lines to disk. */
	static void WriteRecordsToDisk(TArray<TSharedRef<FJsonObject>> RecordsSnapshot, FString AbsoluteFilePath);

	FString GetAbsoluteOutputPath() const;

	// -- State (protected by DataMutex) --

	/** PlayerId -> rolling performance stats for the current match. */
	TMap<FString, FPlayerPerformanceStats> PlayerStatsMap;

	/** VictimId -> (AttackerId -> kill count). Drives "who is dominating whom". */
	TMap<FString, TMap<FString, int32>> NemesisMatrix;

	/** Records awaiting an async disk flush. */
	TArray<TSharedRef<FJsonObject>> PendingRecords;

	/** Guards PlayerStatsMap, NemesisMatrix, and PendingRecords against the async flush thread. */
	mutable FCriticalSection DataMutex;

	/** True while an async flush is in flight, to avoid overlapping writers. */
	FThreadSafeBool bFlushInProgress = false;
};
