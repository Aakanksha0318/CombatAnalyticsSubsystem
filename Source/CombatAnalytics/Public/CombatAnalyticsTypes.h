// CombatAnalyticsTypes.h
// Core telemetry data structures for the Combat Analytics & Telemetry Subsystem.
// These are plain data containers - all heavy logic lives in UCombatAnalyticsSubsystem.

#pragma once

#include "CoreMinimal.h"
#include "CombatAnalyticsTypes.generated.h"

/**
 * A single arena/map hazard trigger event (e.g. Lava Flood, Meteor Shower).
 * Captured whenever a hazard fires so designers can correlate hazard lethality
 * with player count and timing.
 */
USTRUCT(BlueprintType)
struct FArenaEventRecord
{
	GENERATED_BODY()

	/** UTC timestamp of when the hazard triggered. */
	UPROPERTY(BlueprintReadOnly, Category = "Analytics")
	FDateTime Timestamp = FDateTime::UtcNow();

	/** Identifier for the hazard type, e.g. "LavaFlood", "MeteorShower". */
	UPROPERTY(BlueprintReadOnly, Category = "Analytics")
	FString HazardType;

	/** Number of players alive/active in the match when the hazard fired. */
	UPROPERTY(BlueprintReadOnly, Category = "Analytics")
	int32 ActivePlayerCount = 0;

	/** Serializes this record to a JSON object for disk/pipeline export. */
	TSharedRef<class FJsonObject> ToJson() const;
};

/**
 * A single kill event. This is the atomic unit that feeds both the K/D/V ratio
 * engine and the Nemesis dominance matrix.
 */
USTRUCT(BlueprintType)
struct FCombatKillRecord
{
	GENERATED_BODY()

	/** Unique ID (e.g. PlayerState UniqueId string) of the killer. Empty if the death was environmental/void. */
	UPROPERTY(BlueprintReadOnly, Category = "Analytics")
	FString AttackerId;

	/** Unique ID of the player who died. */
	UPROPERTY(BlueprintReadOnly, Category = "Analytics")
	FString VictimId;

	/** Identifier of the weapon used, e.g. "PhoenixBlade", "TectonicSledge". Empty/"VoidFall" for non-weapon deaths. */
	UPROPERTY(BlueprintReadOnly, Category = "Analytics")
	FString WeaponId;

	/** World-space location of the killing blow, for heatmap/hotspot analysis. */
	UPROPERTY(BlueprintReadOnly, Category = "Analytics")
	FVector HitLocation = FVector::ZeroVector;

	/** The victim's Soul Gauge level at time of death (game-specific resource/power metric). */
	UPROPERTY(BlueprintReadOnly, Category = "Analytics")
	int32 SoulGaugeLevel = 0;

	/** UTC timestamp of the kill. */
	UPROPERTY(BlueprintReadOnly, Category = "Analytics")
	FDateTime Timestamp = FDateTime::UtcNow();

	/** Serializes this record to a JSON object for disk/pipeline export. */
	TSharedRef<class FJsonObject> ToJson() const;
};

/**
 * Rolling per-player performance stats for the current match.
 * Kept in-memory and updated incrementally as kill/death/void-fall events arrive.
 */
USTRUCT(BlueprintType)
struct FPlayerPerformanceStats
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Analytics")
	FString PlayerId;

	UPROPERTY(BlueprintReadOnly, Category = "Analytics")
	int32 Kills = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Analytics")
	int32 Deaths = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Analytics")
	int32 VoidFalls = 0;

	/** K/D/V ratio: Kills / max(1, Deaths + VoidFalls). Avoids divide-by-zero for players with no deaths yet. */
	float GetKDVRatio() const
	{
		const int32 Denominator = FMath::Max(1, Deaths + VoidFalls);
		return static_cast<float>(Kills) / static_cast<float>(Denominator);
	}

	TSharedRef<class FJsonObject> ToJson() const;
};
