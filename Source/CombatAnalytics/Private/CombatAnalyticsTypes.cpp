// CombatAnalyticsTypes.cpp

#include "CombatAnalyticsTypes.h"
#include "Dom/JsonObject.h"

TSharedRef<FJsonObject> FArenaEventRecord::ToJson() const
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("event_type"), TEXT("arena_hazard"));
	Json->SetStringField(TEXT("timestamp"), Timestamp.ToIso8601());
	Json->SetStringField(TEXT("hazard_type"), HazardType);
	Json->SetNumberField(TEXT("active_player_count"), ActivePlayerCount);
	return Json;
}

TSharedRef<FJsonObject> FCombatKillRecord::ToJson() const
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("event_type"), TEXT("kill"));
	Json->SetStringField(TEXT("timestamp"), Timestamp.ToIso8601());
	Json->SetStringField(TEXT("attacker_id"), AttackerId);
	Json->SetStringField(TEXT("victim_id"), VictimId);
	Json->SetStringField(TEXT("weapon_id"), WeaponId);

	TSharedRef<FJsonObject> LocationJson = MakeShared<FJsonObject>();
	LocationJson->SetNumberField(TEXT("x"), HitLocation.X);
	LocationJson->SetNumberField(TEXT("y"), HitLocation.Y);
	LocationJson->SetNumberField(TEXT("z"), HitLocation.Z);
	Json->SetObjectField(TEXT("hit_location"), LocationJson);

	Json->SetNumberField(TEXT("soul_gauge_level"), SoulGaugeLevel);
	return Json;
}

TSharedRef<FJsonObject> FPlayerPerformanceStats::ToJson() const
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("player_id"), PlayerId);
	Json->SetNumberField(TEXT("kills"), Kills);
	Json->SetNumberField(TEXT("deaths"), Deaths);
	Json->SetNumberField(TEXT("void_falls"), VoidFalls);
	Json->SetNumberField(TEXT("kdv_ratio"), GetKDVRatio());
	return Json;
}
