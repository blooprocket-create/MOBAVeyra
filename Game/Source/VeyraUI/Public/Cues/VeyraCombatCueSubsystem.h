// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Cues/VeyraCombatCues.h"
#include "Subsystems/WorldSubsystem.h"

#include "VeyraCombatCueSubsystem.generated.h"

/**
 * A client's combat cues (ADR-063 §1): each frame it sights every unit this client sees and raises the moments
 * between the last sighting and this one. It reads only replicated state, which the fog gate already filters, so a
 * cue shows nothing a player isn't entitled to see; no gameplay class knows it. A unit seen for the first time, as
 * one stepping out of the fog, raises nothing until its next change. Not created on a dedicated server.
 */
UCLASS()
class VEYRAUI_API UVeyraCombatCueSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** Sights every unit and raises its cues. The tick calls this; tests call it directly. */
	void Refresh();

	/** Raised for each cue, in the order they happen. */
	TMulticastDelegate<void(const FVeyraCombatCue&)> OnCue;

private:
	TMap<TWeakObjectPtr<const AActor>, FVeyraUnitSighting> Sightings;
};
