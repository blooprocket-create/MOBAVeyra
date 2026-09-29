// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Content/VeyraContentId.h"
#include "Statuses/VeyraStatusTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "Teams/VeyraTeam.h"
#include "TimerManager.h"

#include "VeyraTetherSubsystem.generated.h"

class UAbilitySystemComponent;

/** One tether's declaration (Combat Bible §43), with its values worked out from data by the caller. */
struct FVeyraTetherSpec
{
	/** A newer tether of this ID from the same source replaces the older. */
	FVeyraContentId Id;

	/** Edge to edge, in units; beyond it the tether stretches and ends. Above 0. */
	double MaxRange = 0.0;

	/** Above 0. */
	double DurationSeconds = 0.0;

	/**
	 * Stretched, it first pulls the target this far toward the source, once, before Displacement
	 * Resistance; 0 for none. SnapSpeed is the pull's units per second, above 0 with a pull.
	 */
	double SnapDistance = 0.0;
	double SnapSpeed = 0.0;

	/** Held on the target while the tether lasts, and removed when it ends. */
	TArray<FVeyraStatusSpec> TargetStatuses;
};

/** Why a tether ended (Combat Bible §43). */
enum class EVeyraTetherEndReason : uint8
{
	/** Its time ran out. */
	Expired,
	/** The target went beyond its range. */
	Stretched,
	/** Its source or its target died, or lost its body. */
	Died,
	/** Its source let go of it. */
	Released,
	/** A newer tether of its ID from its source took its place. */
	Replaced,
};

struct FVeyraTetherEnd
{
	TWeakObjectPtr<UAbilitySystemComponent> Source;
	TWeakObjectPtr<UAbilitySystemComponent> Target;
	FVeyraContentId Id;
	EVeyraTetherEndReason Reason = EVeyraTetherEndReason::Expired;
};

/**
 * The server's tethers (Combat Bible §43; ADR-018): active links between a source and a target,
 * judged on a world timer (Combat.json tethers.checkSeconds). Losing sight of the target, or its
 * Camouflage, never breaks one; death, its time or stretching beyond its range does. While a tether
 * holds, the source's side sees its target through fog, stealth and Camouflage, though not in Dense
 * Fog (Vision reads IsTetheredBy). A tether does not hold the target's movement back; only its snap,
 * when declared, moves it.
 */
UCLASS()
class VEYRACOMBAT_API UVeyraTetherSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * Tethers Target to Source by Spec, replacing Source's tether of the same ID. False if refused:
	 * an invalid spec, a unit without a living body, or a unit tethered to itself.
	 */
	bool Tether(UAbilitySystemComponent& Source, UAbilitySystemComponent& Target, const FVeyraTetherSpec& Spec);

	/** Ends Source's tether of ID Id, if it holds one. */
	void Release(const UAbilitySystemComponent& Source, const FVeyraContentId& Id);

	/** Whether Source holds a tether of ID Id now. */
	bool IsTethered(const UAbilitySystemComponent& Source, const FVeyraContentId& Id) const;

	/** Whether a unit on Side holds a tether to Body now, so that the side sees it (Combat Bible §43). */
	bool IsTetheredBy(const AActor& Body, EVeyraTeam Side) const;

	/** Raised as each tether ends, once its statuses are gone. */
	TMulticastDelegate<void(const FVeyraTetherEnd&)> OnTetherEnded;

	virtual void Deinitialize() override;

private:
	struct FLink
	{
		TWeakObjectPtr<UAbilitySystemComponent> Source;
		TWeakObjectPtr<UAbilitySystemComponent> Target;
		FVeyraTetherSpec Spec;
		double EndsAt = 0.0;
	};

	/** Judges every tether: its time, its units' lives and its range. */
	void Check();

	/** Why the link ends now, if it does. */
	TOptional<EVeyraTetherEndReason> Judge(const FLink& Link, double Now) const;

	void End(int32 Index, EVeyraTetherEndReason Reason);

	TArray<FLink> Links;
	FTimerHandle CheckTimer;
};
