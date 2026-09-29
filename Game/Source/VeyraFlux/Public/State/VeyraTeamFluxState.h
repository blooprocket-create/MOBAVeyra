// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "GameFramework/Info.h"
#include "Teams/VeyraTeam.h"

#include "VeyraTeamFluxState.generated.h"

/** A temporary grant as every machine sees it. */
USTRUCT()
struct FVeyraTemporaryFluxView
{
	GENERATED_BODY()

	UPROPERTY()
	double Amount = 0.0;

	/** When it falls away, in the server's world time. */
	UPROPERTY()
	double ExpiresAt = 0.0;
};

/** One team's Team Flux as every machine sees it. */
USTRUCT()
struct FVeyraTeamFluxView
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTeam Team = EVeyraTeam::None;

	UPROPERTY()
	double Permanent = 0.0;

	UPROPERTY()
	TArray<FVeyraTemporaryFluxView> Temporary;

	/** Permanent Flux plus the temporary grants still counting at the server's world time Now. */
	VEYRAFLUX_API double ActiveAt(double Now) const;
};

/**
 * Both teams' Team Flux for presentation (ADR-011 §10): what the server's ledger holds, replicated
 * to everyone, since Team Flux is public. The server's UVeyraTeamFluxSubsystem spawns it and keeps it
 * current; nothing reads it to decide gameplay.
 */
UCLASS(NotBlueprintable, NotPlaceable)
class VEYRAFLUX_API AVeyraTeamFluxState : public AInfo
{
	GENERATED_BODY()

public:
	AVeyraTeamFluxState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server: replaces what every machine sees. */
	void SetTeams(TArray<FVeyraTeamFluxView> InTeams);

	/** A team's view; null before the server has published one. */
	const FVeyraTeamFluxView* Find(EVeyraTeam Team) const;

	const TArray<FVeyraTeamFluxView>& GetTeams() const { return Teams; }

	/** The state in World, if the server has spawned it and it has reached this machine. */
	static AVeyraTeamFluxState* Find(const UWorld* World);

private:
	UPROPERTY(Replicated)
	TArray<FVeyraTeamFluxView> Teams;
};
