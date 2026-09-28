// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "Containers/Map.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "UObject/WeakObjectPtr.h"

#include "VeyraAttributionComponent.generated.h"

class UAbilitySystemComponent;

/**
 * Who has contributed to a unit's death (Combat Bible §18): each enemy Vanguard that damaged,
 * crowd-controlled or debuffed it, and when it last did. A Vanguard's death event names them as
 * assisters; a Fluxborn's or structure's lets Economy pay the Vanguards who fought it (ADR-011 §6).
 * Server only; times are the server's world time, which a pause holds.
 */
UCLASS(ClassGroup = Combat)
class VEYRACOMBAT_API UVeyraAttributionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVeyraAttributionComponent();

	/** Source contributed to the unit's death, if it comes, at world time Now. */
	void NoteContribution(UAbilitySystemComponent& Source, double Now);

	/** Everyone who contributed within the tuned assist window before Now, except Killer. */
	TArray<UAbilitySystemComponent*> GetAssisters(const UAbilitySystemComponent* Killer, double Now) const;

	/** Every contributor still present, with when it last contributed. */
	TArray<FVeyraContribution> GetContributions() const;

	/** Forgets every contribution, as at death. */
	void Clear();

private:
	TMap<TWeakObjectPtr<UAbilitySystemComponent>, double> LastContributions;
};
