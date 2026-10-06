// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Delegates/IDelegateInstance.h"
#include "Gold/VeyraGoldComponent.h"
#include "UObject/WeakObjectPtr.h"

class AVeyraPlayerState;
class UAbilitySystemComponent;
class UVeyraCombatEventSubsystem;
class UWorld;
struct FVeyraCombatTextLine;
struct FVeyraDamageDealtEvent;
struct FVeyraHealthRestored;
struct FVeyraShieldGranted;

/**
 * Match's floating combat text (ADR-052 §1): it hears Combat's damage, healing and shields, and each watched
 * participant's Gold (ADR-065 §4), on the server and sends each number only to the player it concerns, through that
 * player's controller, never about a unit or a place the player's side cannot see. A bot's participant has no
 * controller and is sent nothing. The game mode owns one. Server only.
 */
class FVeyraCombatTextLink
{
public:
	~FVeyraCombatTextLink();

	/** Listens to World's combat. */
	void Start(UWorld& World);

	/** Watches Participant's Gold, sending its player each grant that makes a number. */
	void WatchGold(AVeyraPlayerState& Participant);

	void Stop();

private:
	void OnDamageTaken(const FVeyraDamageDealtEvent& Event);
	void OnHealthRestored(const FVeyraHealthRestored& Event);
	void OnShieldGranted(const FVeyraShieldGranted& Event);
	void OnGoldGranted(double Amount, EVeyraGoldReason Reason, const FVeyraGoldSource& From, TWeakObjectPtr<AVeyraPlayerState> Participant);

	/** Sends Line to the player whose participant Player is, when it has a controller and its side sees the unit. */
	void Send(UAbilitySystemComponent* Player, FVeyraCombatTextLine Line) const;

	TWeakObjectPtr<UVeyraCombatEventSubsystem> Events;
	FDelegateHandle DamageHandle;
	FDelegateHandle HealingHandle;
	FDelegateHandle ShieldHandle;

	struct FWatchedGold
	{
		TWeakObjectPtr<UVeyraGoldComponent> Gold;
		FDelegateHandle Handle;
	};
	TArray<FWatchedGold> WatchedGold;
};
