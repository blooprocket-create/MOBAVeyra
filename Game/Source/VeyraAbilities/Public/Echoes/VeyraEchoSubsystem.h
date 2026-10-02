// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Abilities/VeyraGameplayAbility.h"
#include "Content/VeyraContentId.h"
#include "Engine/TimerHandle.h"
#include "Slots/VeyraAbilitySlot.h"
#include "Subsystems/WorldSubsystem.h"
#include "VeyraAbilityTypes.h"

#include "VeyraEchoSubsystem.generated.h"

class AVeyraEcho;
class UAbilitySystemComponent;
struct FVeyraHostileDamageEvent;

/** Why an Echo ended (ADR-050 §4). */
UENUM()
enum class EVeyraEchoEnd : uint8
{
	/** It repeated as many abilities as it may. */
	Spent,
	/** Its window ran out first. */
	Expired,
	/** Its holder formed another. */
	Replaced,
	/** Its holder died, left the battleground, or its player disconnected. */
	HolderGone,
	/** Its Integrity ran out. */
	Faded,
	/** It went beyond its tether radius, or the radius shrank past it. */
	Strayed,
};

/**
 * Keeps each Vanguard's Echo (ADR-050 §2–§6), on the server: one per holder. It forms an Echo where an Echo ability
 * sends it; hands a waiting Echo the repeat of its holder's next eligible cast; keeps a projected Echo's Integrity and
 * tether, its holder in Stasis meanwhile, and passes control to it once formed; and ends it. An ended Echo stays
 * withdrawn, out of sight and dead at nobody's hand, until its holder forms another, so what it set going keeps its
 * source.
 */
UCLASS()
class VEYRAABILITIES_API UVeyraEchoSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/**
	 * Server: Holder's Echo for Ability's manifest forms at Where, on the nearest ground, and waits for Holder's next
	 * eligible cast for its window (ADR-050 §4); any Echo Holder had ends first. Null if refused.
	 */
	AVeyraEcho* Manifest(UAbilitySystemComponent& Holder, const FVeyraContentId& Ability, const FVector& Where);

	/**
	 * Server: Holder projects its Echo for Ability's projection at Where (ADR-050 §4): Holder enters the projection's
	 * Stasis where it stands, and the Echo forms at Where, on the nearest ground, immune to enemy hits for its immunity
	 * and taking control once formed. Any Echo Holder had ends first. Null if refused.
	 */
	AVeyraEcho* Project(UAbilitySystemComponent& Holder, const FVeyraContentId& Ability, const FVector& Where);

	/** Server: Holder's Echo while it stands, or null. */
	AVeyraEcho* FindStanding(const UAbilitySystemComponent& Holder) const;

	/** Server: Holder's projected Echo once control has passed to it, while it stands, or null. */
	AVeyraEcho* FindCommanded(const UAbilitySystemComponent& Holder) const;

	/**
	 * Server: the repeat Holder's waiting Echo takes of Holder's committed Cast (ADR-050 §5), when Cast's ability sits in
	 * one of the slots the Echo repeats: the same ability at the same rank, cast by the Echo from where it stands, aimed
	 * as the cast was and within its range, under a cast ID of its own. Unset when it takes none. Its last repeat ends it.
	 * The caller delivers the repeat, and only an ability whose delivery can be repeated asks.
	 */
	TOptional<FVeyraCast> TakeRepeat(UAbilitySystemComponent& Holder, const FVeyraCast& Cast);

	/**
	 * Server: Holder's commanded Echo casts the ability in Holder's Slot at Target (ADR-050 §5): an eligible ability of
	 * Holder's, at its rank, delivered from where the Echo stands at the Echo's share of the damage, while it has a repeat
	 * left. It costs nothing and leaves Holder's cooldowns alone. Projected when the Echo may not cast it: an item's
	 * Active, a Flux Spell, a slot it does not repeat, an ability it cannot repeat, or no repeat left.
	 */
	EVeyraCastRejection CastFrom(UAbilitySystemComponent& Holder, EVeyraAbilitySlot Slot, const FVeyraCastTarget& Target);

	/** Server: Holder's standing Echo ends for Why. Nothing happens without one. */
	void End(const UAbilitySystemComponent& Holder, EVeyraEchoEnd Why);

	/** Server: control passed to Holder's projected Echo, which its holder's orders now move (ADR-050 §6). */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnEchoCommanded, UAbilitySystemComponent& /*Holder*/, AVeyraEcho& /*Echo*/);
	FOnEchoCommanded OnEchoCommanded;

	/** Server: an Echo ended, and why; a commanded Echo's control returns to its holder. */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnEchoEnded, AVeyraEcho& /*Echo*/, EVeyraEchoEnd /*Why*/);
	FOnEchoEnded OnEchoEnded;

private:
	struct FKept
	{
		TWeakObjectPtr<UAbilitySystemComponent> Holder;
		TWeakObjectPtr<AVeyraEcho> Echo;
		int32 RepeatsLeft = 0;
		FTimerHandle Timer;

		/** A projected Echo's: its holder's Stasis, when its immunity ends, its Integrity and when it last changed. */
		bool bProjected = false;
		bool bCommanded = false;
		FVeyraContentId Stasis;
		double ImmuneUntil = 0.0;
		double Integrity = 0.0;
		double UpdatedAt = 0.0;
		FTimerHandle ControlTimer;
	};

	FKept* FindKept(const UAbilitySystemComponent& Holder);
	const FKept* FindKept(const UAbilitySystemComponent& Holder) const;

	/** Forms Holder's Echo for Ability at Where, ending and clearing away any Echo Holder had. Null if refused. */
	AVeyraEcho* Form(UAbilitySystemComponent& Holder, const FVeyraContentId& Ability, const FVector& Where, TOptional<double> Integrity);

	void EndKept(FKept& Entry, EVeyraEchoEnd Why);
	void OnWindowEnded(TWeakObjectPtr<UAbilitySystemComponent> Holder);

	/** A projected Echo's Integrity decays on its timer, and its tether is judged. */
	void OnProjectionUpdate(TWeakObjectPtr<UAbilitySystemComponent> Holder);
	void OnFormed(TWeakObjectPtr<UAbilitySystemComponent> Holder);

	/** Shows Entry's Integrity on its Echo, and ends it at 0 or once beyond its tether. */
	void ApplyIntegrity(FKept& Entry);

	/** An enemy's hit on a projected Echo, once its immunity ends, costs its kind's Integrity (ADR-050 §3). */
	void OnHostileDamage(const FVeyraHostileDamageEvent& Event);

	TArray<FKept> Kept;
	FDelegateHandle HostileDamageHandle;
};
