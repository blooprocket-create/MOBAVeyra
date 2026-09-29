// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Engine/TimerHandle.h"
#include "Ledger/VeyraFluxLedger.h"
#include "Subsystems/WorldSubsystem.h"
#include "Teams/VeyraTeam.h"

#include "VeyraTeamFluxSubsystem.generated.h"

class AVeyraTeamFluxState;

/** A source of Team Flux (Battleground Bible §5, §6, §10, §18). */
UENUM()
enum class EVeyraFluxSource : uint8
{
	LaneSpire,
	BaseTower,
	Inhibitor,
	/** A secured Flux Well (§6). */
	FluxWell,
};

/**
 * The authoritative Team Flux of each team (PROJECT_STRUCTURE.md, VeyraFlux; ADR-011 §10). Match
 * grants it when World reports a structure destroyed; temporary grants expire on world-time timers,
 * so a pause holds them. It announces every change, expiries included, so Fluxborn strength follows
 * without polling, and publishes both teams' Flux to AVeyraTeamFluxState for presentation. Server
 * only; a client's instance does nothing.
 */
UCLASS()
class VEYRAFLUX_API UVeyraTeamFluxSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	/** Server: grants Team what Source gives, from the Flux tuning. */
	void Grant(EVeyraTeam Team, EVeyraFluxSource Source);

	/** Permanent Flux plus the temporary grants still counting. */
	double GetActive(EVeyraTeam Team) const;

	/** Permanent Flux alone, which Flux Spell unlocks will read. */
	double GetPermanent(EVeyraTeam Team) const;

	/** The strength Team's active Flux gives its lane Fluxborn (Battleground Bible §4). */
	FVeyraFluxbornStrength GetFluxbornStrength(EVeyraTeam Team) const;

	/** Server: Team's active Flux changed, by a grant or an expiry. */
	TMulticastDelegate<void(EVeyraTeam)> OnTeamFluxChanged;

	/** Server: drops every grant whose time has come. Its timer calls it; tests may too. */
	void ExpireGrants();

	/** The published state, once spawned. */
	AVeyraTeamFluxState* GetState() const { return State.Get(); }

private:
	FVeyraFluxLedger* LedgerOf(EVeyraTeam Team);
	const FVeyraFluxLedger* LedgerOf(EVeyraTeam Team) const;
	double Now() const;
	void Publish();
	void ScheduleExpiry();

	FVeyraFluxLedger LedgerA;
	FVeyraFluxLedger LedgerB;
	TWeakObjectPtr<AVeyraTeamFluxState> State;
	FTimerHandle ExpiryTimer;
};
