// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Content/VeyraContentId.h"
#include "Engine/TimerHandle.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Subsystems/WorldSubsystem.h"
#include "Teams/VeyraTeam.h"

#include "VeyraRewardSubsystem.generated.h"

class APlayerState;
class UVeyraGoldComponent;
class UVeyraProgressionComponent;

/**
 * Pays Gold and XP for the match's deaths (Economy & Progression Bible §2–§8; ADR-011 §11). It holds
 * no formulas of its own: it decides who qualifies (the killer, the assisters, the recent contributors,
 * the living allies near the death) and pays through VeyraRewards, each participant's
 * UVeyraGoldComponent and UVeyraProgressionComponent. Vanguard kills reach it as Combat's deaths;
 * World reports the deaths only it can describe, a Fluxborn's kind and its team's Flux, a structure's
 * kind. Nothing is paid once the match ends (§8.2). Server only; a client's instance does nothing.
 */
UCLASS()
class VEYRAECONOMY_API UVeyraRewardSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/**
	 * Server: a lane Fluxborn of Kind died, its team's active Team Flux at ActiveFlux (§3, §4). Its last
	 * hitter, if a Vanguard of the other side, takes its Gold; the other nearby living allies take a
	 * share when an ally fought it recently; the nearby living allies share its XP.
	 */
	void RewardFluxbornDeath(const FVeyraDeathEvent& Death, const FVeyraContentId& Kind, double ActiveFlux);

	/**
	 * Server: Team's lane Spire or base-defense tower fell (§8.1). Its recent contributors split its
	 * pool, wherever they are; the first to fall in the match pays the whole destroying team a bonus.
	 * Inhibitors and the Prime Well pay nothing, so World never reports them.
	 */
	void RewardStructureDestroyed(const FVeyraDeathEvent& Death, EVeyraTeam Team);

	/**
	 * Server: starts every participant's passive Gold as the match goes live (author ruling,
	 * 2026-09-28): from Economy.json passiveGold.startSeconds on, a payment each interval, dead or
	 * alive, until Stop. It runs on world time, so a pause holds it.
	 */
	void StartPassiveGold();

	/**
	 * Server: a jungle creature of Species died (§7). The Vanguard who landed its killing blow takes
	 * its Gold, wherever they stand; the killing side's living Vanguards near it share its XP, and with
	 * no Vanguard's blow, each side's near it share it. There is no participation Gold.
	 */
	void RewardWildlifeDeath(const FVeyraDeathEvent& Death, const FVeyraContentId& Species);

	/**
	 * Server: a Flux Well was secured (§8.2). Its Gold pool is split evenly among Capturers, the
	 * securing side's Vanguards working on it at that moment; there is no XP.
	 */
	void RewardFluxWellSecured(TConstArrayView<UAbilitySystemComponent*> Capturers);

	/**
	 * Server: an enemy ward was destroyed (§8.3). Its bounty goes to the Vanguard credited with its
	 * killing blow alone; no one else shares it, and it gives no XP.
	 */
	void RewardWardDestroyed(const FVeyraDeathEvent& Death);

	/** Server: pays nothing more, as when the match ends (§8.2). */
	void Stop();

	/** Server: grants a participant its starting Gold (§1), once, as its match prepares. */
	static void GrantStartingGold(APlayerState& Participant, TOptional<double> SessionStartingGold = {});

private:
	/** One participant as a reward sees it. */
	struct FRecipient
	{
		UAbilitySystemComponent* AbilitySystem = nullptr;
		UVeyraGoldComponent* Gold = nullptr;
		UVeyraProgressionComponent* Progression = nullptr;
		EVeyraTeam Team = EVeyraTeam::None;
		/** Where its Vanguard stands; unset while it has no body. */
		TOptional<FVector> Location;
		bool bAlive = false;
	};

	void OnDeath(const FVeyraDeathEvent& Death);
	void RewardVanguardKill(const FVeyraDeathEvent& Death);
	void PayPassiveGold();

	/** Every participant: the PlayerStates with Gold and progression. */
	TArray<FRecipient> Recipients() const;
	const FRecipient* FindRecipient(TConstArrayView<FRecipient> All, const UAbilitySystemComponent* AbilitySystem) const;

	/** Whether Recipient's Vanguard stands within the reward radius of Location (§2: walls and fog never block it). */
	static bool IsNear(const FRecipient& Recipient, const FVector& Location);

	/** Whether Recipient can still gain XP: below the cap (§3.3, §6). */
	static bool CanGainExperience(const FRecipient& Recipient);

	void GrantExperience(const FRecipient& Recipient, double Amount) const;

	/** Shares Experience among Side's living Vanguards near Where who can still gain XP (§3.3, §7). */
	void ShareExperience(TConstArrayView<FRecipient> All, EVeyraTeam Side, const FVector& Where, double Experience) const;
	bool IsServer() const;

	FDelegateHandle DeathHandle;
	FTimerHandle PassiveGoldTimer;
	bool bFirstBloodTaken = false;
	bool bFirstStructureTaken = false;
	bool bStopped = false;
};
