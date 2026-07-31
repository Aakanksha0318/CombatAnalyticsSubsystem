// CombatAnalyticsSubsystem.cpp

#include "CombatAnalyticsSubsystem.h"

#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFilemanager.h"

void UCombatAnalyticsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Make sure the output directory exists before the first flush ever runs.
	const FString AbsolutePath = GetAbsoluteOutputPath();
	const FString Directory = FPaths::GetPath(AbsolutePath);
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*Directory))
	{
		PlatformFile.CreateDirectoryTree(*Directory);
	}

	UE_LOG(LogTemp, Log, TEXT("[CombatAnalytics] Subsystem initialized. Output: %s"), *AbsolutePath);
}

void UCombatAnalyticsSubsystem::Deinitialize()
{
	// Flush synchronously on shutdown so we never lose the tail of a match's telemetry.
	TArray<TSharedRef<FJsonObject>> RemainingRecords;
	{
		FScopeLock Lock(&DataMutex);
		RemainingRecords = MoveTemp(PendingRecords);
		PendingRecords.Empty();
	}

	if (RemainingRecords.Num() > 0)
	{
		WriteRecordsToDisk(MoveTemp(RemainingRecords), GetAbsoluteOutputPath());
	}

	Super::Deinitialize();
}

// ---------------------------------------------------------------------------
// Telemetry hooks
// ---------------------------------------------------------------------------

void UCombatAnalyticsSubsystem::LogKillEvent(const FString& AttackerId, const FString& VictimId, const FString& WeaponId, FVector HitLocation, int32 SoulGaugeLevel)
{
	if (VictimId.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatAnalytics] LogKillEvent called with empty VictimId, ignoring."));
		return;
	}

	FCombatKillRecord Record;
	Record.AttackerId = AttackerId;
	Record.VictimId = VictimId;
	Record.WeaponId = WeaponId;
	Record.HitLocation = HitLocation;
	Record.SoulGaugeLevel = SoulGaugeLevel;
	Record.Timestamp = FDateTime::UtcNow();

	{
		FScopeLock Lock(&DataMutex);

		FPlayerPerformanceStats& VictimStats = GetOrCreateStats_Locked(VictimId);
		VictimStats.Deaths += 1;

		// Environmental/self kills (empty AttackerId, or attacker == victim) don't
		// count toward another player's Kills or the Nemesis matrix.
		if (!AttackerId.IsEmpty() && AttackerId != VictimId)
		{
			FPlayerPerformanceStats& AttackerStats = GetOrCreateStats_Locked(AttackerId);
			AttackerStats.Kills += 1;
			RegisterNemesisKill_Locked(AttackerId, VictimId);
		}

		BufferRecord_Locked(Record.ToJson());
	}

	MaybeAutoFlush();
}

void UCombatAnalyticsSubsystem::LogVoidFall(const FString& VictimId, FVector HitLocation)
{
	if (VictimId.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatAnalytics] LogVoidFall called with empty VictimId, ignoring."));
		return;
	}

	FCombatKillRecord Record;
	Record.AttackerId = TEXT(""); // No attacker for void falls.
	Record.VictimId = VictimId;
	Record.WeaponId = TEXT("VoidFall");
	Record.HitLocation = HitLocation;
	Record.SoulGaugeLevel = 0;
	Record.Timestamp = FDateTime::UtcNow();

	{
		FScopeLock Lock(&DataMutex);

		FPlayerPerformanceStats& VictimStats = GetOrCreateStats_Locked(VictimId);
		VictimStats.VoidFalls += 1;

		BufferRecord_Locked(Record.ToJson());
	}

	MaybeAutoFlush();
}

void UCombatAnalyticsSubsystem::LogArenaEvent(const FString& HazardType, int32 ActivePlayerCount)
{
	FArenaEventRecord Record;
	Record.HazardType = HazardType;
	Record.ActivePlayerCount = ActivePlayerCount;
	Record.Timestamp = FDateTime::UtcNow();

	{
		FScopeLock Lock(&DataMutex);
		BufferRecord_Locked(Record.ToJson());
	}

	MaybeAutoFlush();
}

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------

FPlayerPerformanceStats UCombatAnalyticsSubsystem::GetPlayerStats(const FString& PlayerId) const
{
	FScopeLock Lock(&DataMutex);
	if (const FPlayerPerformanceStats* Found = PlayerStatsMap.Find(PlayerId))
	{
		return *Found;
	}

	FPlayerPerformanceStats Empty;
	Empty.PlayerId = PlayerId;
	return Empty;
}

FString UCombatAnalyticsSubsystem::GetNemesisFor(const FString& VictimId) const
{
	FScopeLock Lock(&DataMutex);

	const TMap<FString, int32>* Attackers = NemesisMatrix.Find(VictimId);
	if (!Attackers || Attackers->Num() == 0)
	{
		return FString();
	}

	FString BestAttacker;
	int32 BestCount = 0;
	for (const TPair<FString, int32>& Pair : *Attackers)
	{
		if (Pair.Value > BestCount)
		{
			BestCount = Pair.Value;
			BestAttacker = Pair.Key;
		}
	}
	return BestAttacker;
}

void UCombatAnalyticsSubsystem::FlushNow()
{
	DispatchAsyncFlush();
}

// ---------------------------------------------------------------------------
// Internal helpers - all assume DataMutex is already held unless noted otherwise
// ---------------------------------------------------------------------------

FPlayerPerformanceStats& UCombatAnalyticsSubsystem::GetOrCreateStats_Locked(const FString& PlayerId)
{
	FPlayerPerformanceStats* Existing = PlayerStatsMap.Find(PlayerId);
	if (Existing)
	{
		return *Existing;
	}

	FPlayerPerformanceStats NewStats;
	NewStats.PlayerId = PlayerId;
	return PlayerStatsMap.Add(PlayerId, NewStats);
}

void UCombatAnalyticsSubsystem::RegisterNemesisKill_Locked(const FString& AttackerId, const FString& VictimId)
{
	TMap<FString, int32>& AttackersForVictim = NemesisMatrix.FindOrAdd(VictimId);
	int32& KillCount = AttackersForVictim.FindOrAdd(AttackerId);
	KillCount += 1;
}

void UCombatAnalyticsSubsystem::BufferRecord_Locked(TSharedRef<FJsonObject> RecordJson)
{
	PendingRecords.Add(RecordJson);
}

void UCombatAnalyticsSubsystem::MaybeAutoFlush()
{
	bool bShouldFlush = false;
	{
		FScopeLock Lock(&DataMutex);
		bShouldFlush = PendingRecords.Num() >= FlushThreshold;
	}

	if (bShouldFlush)
	{
		DispatchAsyncFlush();
	}
}

void UCombatAnalyticsSubsystem::DispatchAsyncFlush()
{
	// Skip if a flush is already in flight; the next MaybeAutoFlush/FlushNow call
	// will pick up whatever accumulated in the meantime.
	if (bFlushInProgress.AtomicSet(true))
	{
		return;
	}

	TArray<TSharedRef<FJsonObject>> Snapshot;
	{
		FScopeLock Lock(&DataMutex);
		Snapshot = MoveTemp(PendingRecords);
		PendingRecords.Empty();
	}

	if (Snapshot.Num() == 0)
	{
		bFlushInProgress = false;
		return;
	}

	const FString AbsolutePath = GetAbsoluteOutputPath();

	// Offload the actual file IO to the thread pool so we never hitch the game thread.
	TWeakObjectPtr<UCombatAnalyticsSubsystem> WeakThis(this);
	Async(EAsyncExecution::ThreadPool, [Snapshot = MoveTemp(Snapshot), AbsolutePath, WeakThis]() mutable
	{
		WriteRecordsToDisk(MoveTemp(Snapshot), AbsolutePath);

		if (UCombatAnalyticsSubsystem* StrongThis = WeakThis.Get())
		{
			StrongThis->bFlushInProgress = false;
		}
	});
}

void UCombatAnalyticsSubsystem::WriteRecordsToDisk(TArray<TSharedRef<FJsonObject>> RecordsSnapshot, FString AbsoluteFilePath)
{
	// Newline-delimited JSON (NDJSON): one record per line. This lets the
	// Python/Pandas pipeline stream-parse the file (pd.read_json(path, lines=True))
	// without needing to load and re-parse a single giant JSON array on every flush.
	FString OutputBuffer;
	OutputBuffer.Reserve(RecordsSnapshot.Num() * 256);

	for (const TSharedRef<FJsonObject>& Record : RecordsSnapshot)
	{
		FString Line;
		TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Line);
		FJsonSerializer::Serialize(Record, Writer);

		OutputBuffer += Line;
		OutputBuffer += LINE_TERMINATOR;
	}

	// Append rather than overwrite so successive flushes accumulate the full match log.
	FFileHelper::SaveStringToFile(
		OutputBuffer,
		*AbsoluteFilePath,
		FFileHelper::EEncodingOptions::AutoDetect,
		&IFileManager::Get(),
		FILEWRITE_Append);
}

FString UCombatAnalyticsSubsystem::GetAbsoluteOutputPath() const
{
	return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / OutputRelativePath);
}
