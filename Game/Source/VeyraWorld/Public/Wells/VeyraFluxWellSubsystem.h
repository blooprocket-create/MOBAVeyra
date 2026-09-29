// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Engine/TimerHandle.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Subsystems/WorldSubsystem.h"
#include "Teams/VeyraTeam.h"

#include "VeyraFluxWellSubsystem.generated.h"

class AVeyraFluxWell;

/** A Flux Well secured, as World reports it to Match (ADR-014 §4, §6). */
struct FVeyraFluxWellSecuredEvent
{
	/** Which of World.json's sites. */
	int32 Site = INDEX_NONE;

	/** The side that landed its last hit, by damage or by presence. */
	EVeyraTeam Team = EVeyraTeam::None;

	/** The side's Vanguards that secured it: those present, and whoever landed the last hit (Economy Bible §8.2). */
	TArray<TWeakObjectPtr<UAbilitySystemComponent>> Capturers;

	/** The Vanguard that landed the last hit, by damage or by presence; null when no Vanguard did (Match Statistics Bible §5). */
	TWeakObjectPtr<UAbilitySystemComponent> FinalHitter;
};

/**
 * The North and South Flux Wells on the server (Battleground Bible §6; ADR-014 §4). Once Match starts
 * it as the match goes live, it spawns a Well at each of World.json's sites, closed until their opening
 * time on the match clock. An open Well is drained by damage and, on a world-time timer, by the
 * presence of each side's living Vanguards within its radius, capped and contested; the drain is dealt
 * as damage in a present Vanguard's name, so presence can land the last hit. With nobody working on it
 * a while, it heals. At 0 Health the side credited with its last hit secures it: World pays that side's
 * Vanguards present the Gold pool through Economy and announces the secure, so Match can grant the
 * Team Flux; the Well then waits its cycle and opens again at full Health. A pause holds every timer.
 * It needs the battleground. A client's instance does nothing.
 */
UCLASS()
class VEYRAWORLD_API UVeyraFluxWellSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Server: spawns the Wells, closed, and starts their clocks, the match clock at 0 now. Does nothing without the battleground, once started, or once stopped. */
	void Start();

	/** Server: no more opening, draining or healing, as when the match ends. */
	void Stop();

	/** Server: opens Well Index now at full Health. Its timer calls it; tests may too. */
	void Open(int32 Index);

	/** Server: one presence tick, Seconds long: each open Well drained by the Vanguards at it, or healed. Its timer calls it; tests may too. */
	void UpdatePresence(double Seconds);

	const TArray<TObjectPtr<AVeyraFluxWell>>& GetWells() const { return Wells; }

	/** Server: a Well was secured. */
	TMulticastDelegate<void(const FVeyraFluxWellSecuredEvent&)> OnFluxWellSecured;

private:
	void OnDeath(const FVeyraDeathEvent& Death);
	void OnHostileDamage(const FVeyraHostileDamageEvent& Event);
	void OnPresenceTimer();

	/** The living Vanguards of Side within the Wells' radius of Well, nearest first. */
	TArray<AActor*> PresentVanguards(const AVeyraFluxWell& Well, EVeyraTeam Side) const;
	bool IsServer() const;

	UPROPERTY()
	TArray<TObjectPtr<AVeyraFluxWell>> Wells;

	/** When each Well was last damaged or drained, in world time. */
	TArray<double> LastWorkedAt;
	TArray<FTimerHandle> OpenTimers;
	FTimerHandle PresenceTimer;
	FDelegateHandle DeathHandle;
	FDelegateHandle HostileDamageHandle;
	bool bStarted = false;
	bool bStopped = false;
};
