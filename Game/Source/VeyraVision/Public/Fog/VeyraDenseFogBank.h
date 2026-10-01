// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "Rules/VeyraVisionRules.h"

#include "VeyraDenseFogBank.generated.h"

/**
 * Dense Fog an ability laid (Vision Bible §2, "Ability-created Dense Fog"; ADR-036 §1), as Lay the Mist's: the
 * circles one cast made, for as long as it lasts. It is the same construct as the map's fog, which the vision
 * subsystem joins it to; the fog itself is map knowledge, so every player receives it, and the grey box draws
 * it as it draws the map's. Vision spawns it and destroys it as it ends.
 */
UCLASS(NotBlueprintable, NotPlaceable)
class VEYRAVISION_API AVeyraDenseFogBank : public AActor
{
	GENERATED_BODY()

public:
	AVeyraDenseFogBank();

	/** Server, before it joins the world: its circles, all of one radius. */
	void SetCircles(TConstArrayView<FVeyraFogCircle> Circles);

	/** Its circles, on every machine once they have arrived. */
	TArray<FVeyraFogCircle> GetCircles() const;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UPROPERTY(Replicated)
	TArray<FVector2f> Centres;

	UPROPERTY(Replicated)
	float Radius = 0.0f;
};
