// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "Containers/Map.h"
#include "UObject/WeakObjectPtr.h"

#include "VeyraAttributionComponent.generated.h"

class UAbilitySystemComponent;

/**
 * Who has contributed to a Vanguard's death (Combat Bible §18): each enemy Vanguard that damaged,
 * crowd-controlled or debuffed it, and when it last did. Its death event names them as assisters.
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

	/** Forgets every contribution, as at death. */
	void Clear();

private:
	TMap<TWeakObjectPtr<UAbilitySystemComponent>, double> LastContributions;
};
