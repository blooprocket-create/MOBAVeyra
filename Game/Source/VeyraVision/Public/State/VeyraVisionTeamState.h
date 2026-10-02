// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "GameFramework/Info.h"
#include "Rules/VeyraVisionRules.h"
#include "Teams/VeyraTeam.h"

#include "VeyraVisionTeamState.generated.h"

/**
 * A presence ping as its team sees it (Vision Bible §4, §6; ADR-016 §5): the Dense Fog circle an enemy
 * Vanguard is present in, never where it stands in it.
 */
USTRUCT()
struct FVeyraPresencePing
{
	GENERATED_BODY()

	UPROPERTY()
	FVector2D Centre = FVector2D::ZeroVector;

	UPROPERTY()
	double Radius = 0.0;

	/** When it was sent, in the server's world time. */
	UPROPERTY()
	double At = 0.0;
};

/**
 * An outline Sweeper shows its team (Vision Bible §5): where an enemy Vanguard inside Dense Fog stands,
 * which grants no targeting. It stays where True Sight last covered it until it fades.
 */
USTRUCT()
struct FVeyraOutline
{
	GENERATED_BODY()

	UPROPERTY()
	FVector Location = FVector::ZeroVector;

	/** When it fades, in the server's world time. */
	UPROPERTY()
	double Until = 0.0;
};

/**
 * The ground a side sees now, on a coarse grid over the battleground (ADR-054 §2): presentation only, which nothing
 * reads to decide gameplay. Its cells are packed as VeyraVisionRules::SeenCells packs them.
 */
USTRUCT()
struct FVeyraSeenGround
{
	GENERATED_BODY()

	UPROPERTY()
	FVector2D Min = FVector2D::ZeroVector;

	UPROPERTY()
	double CellSize = 0.0;

	UPROPERTY()
	int32 CellsAcross = 0;

	UPROPERTY()
	TArray<uint8> Cells;

	FVeyraSeenGrid Grid() const
	{
		FVeyraSeenGrid Out;
		Out.Min = Min;
		Out.CellSize = CellSize;
		Out.CellsAcross = CellsAcross;
		return Out;
	}

	/** Whether cell (X, Y) is seen; false outside the grid or before any arrives. */
	bool IsSeen(int32 X, int32 Y) const { return VeyraVisionRules::IsCellSeen(Cells, Grid(), X, Y); }
};

/**
 * What one side's vision tells that side alone (ADR-016 §5; ADR-054 §2): its presence pings, its outlines and the
 * ground it sees. Vision
 * spawns one per side on the server and keeps it current; the fog gate lets only that side's clients
 * receive it. Nothing reads it to decide gameplay.
 */
UCLASS(NotBlueprintable, NotPlaceable)
class VEYRAVISION_API AVeyraVisionTeamState : public AInfo
{
	GENERATED_BODY()

public:
	AVeyraVisionTeamState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	EVeyraTeam GetVeyraTeam() const { return Team; }

	/** Server, once, as it is spawned. */
	void SetVeyraTeam(EVeyraTeam InTeam);

	/** Server: adds a ping, and lets go of those older than KeepSeconds at the server's world time Now. */
	void AddPing(const FVeyraPresencePing& Ping, double Now, double KeepSeconds);

	/** Server: replaces the outlines. */
	void SetOutlines(TArray<FVeyraOutline> InOutlines);

	/** Server: the ground its side sees now over Grid, as SeenCells packs it; sent only when it changed. */
	void SetSeenGround(const FVeyraSeenGrid& Grid, TArray<uint8> Cells);

	const FVeyraSeenGround& GetSeenGround() const { return SeenGround; }

	const TArray<FVeyraPresencePing>& GetPings() const { return Pings; }
	const TArray<FVeyraOutline>& GetOutlines() const { return Outlines; }

	/** Team's state in World, if the server has spawned it and it has reached this machine. */
	static AVeyraVisionTeamState* Find(const UWorld* World, EVeyraTeam Team);

private:
	UPROPERTY(Replicated)
	EVeyraTeam Team = EVeyraTeam::None;

	UPROPERTY(Replicated)
	TArray<FVeyraPresencePing> Pings;

	UPROPERTY(Replicated)
	TArray<FVeyraOutline> Outlines;

	UPROPERTY(Replicated)
	FVeyraSeenGround SeenGround;
};
