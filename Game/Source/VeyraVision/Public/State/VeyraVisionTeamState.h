// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "GameFramework/Info.h"
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
 * What one side's vision tells that side alone (ADR-016 §5): its presence pings and its outlines. Vision
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
};
