// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Rules/VeyraVisionRules.h"
#include "Subsystems/WorldSubsystem.h"
#include "Targeting/VeyraVisibility.h"
#include "Teams/VeyraTeam.h"
#include "TimerManager.h"

#include "VeyraVisionSubsystem.generated.h"

class FVeyraFogGate;

/**
 * A match's vision (ADR-016 §2, §3), on the server. Every pass (Vision.json's updateSeconds) it works
 * out which enemy and neutral units each team sees, from its living Vanguards, Fluxborn and standing
 * structures, and records it. Targeting, orders and bots read that record through the visibility
 * contract, and the fog gate makes each client receive exactly what its player sees, so what a
 * player may target and what reaches them always agree.
 *
 * Match starts it when the match begins and stops it at the end; a world it never started sees
 * everything (unit tests and development worlds without a match).
 */
UCLASS()
class VEYRAVISION_API UVeyraVisionSubsystem : public UWorldSubsystem, public IVeyraVisibility
{
	GENERATED_BODY()

public:
	UVeyraVisionSubsystem();
	virtual ~UVeyraVisionSubsystem() override;

	/** Server: starts working vision out, governing targeting and gating what clients receive. */
	void Start();

	/** Stops; the world sees everything again. */
	void Stop();

	bool IsStarted() const { return bStarted; }

	/** Works vision out now rather than at the next pass. For tests and for Start. */
	void UpdateNow();

	// IVeyraVisibility
	virtual bool CanSee(const UObject& Observer, const AActor& Target) const override;
	virtual bool IsVisibleToTeam(EVeyraTeam Team, const AActor& Target) const override;

	virtual void Deinitialize() override;

private:
	/** Whether Unit is hidden from those who cannot see it: every unit but structures and Flux Wells. */
	static bool IsGated(const AActor& Unit);

	void OnActorSpawned(AActor* Actor);

	/** What each side saw at the last pass: its enemies' and neutral units it may see and target. */
	TMap<EVeyraTeam, TSet<TWeakObjectPtr<const AActor>>> Seen;
	/**
	 * Every gated unit the last pass knew, and its sight sources: a unit that spawned since is judged
	 * against those sources at once, so it is not hidden until the next pass.
	 */
	TSet<TWeakObjectPtr<const AActor>> Known;
	TArray<FVeyraSightSource> Sources;
	/** Shared rather than unique so this header need not know it (Private/Gate). */
	TSharedPtr<FVeyraFogGate> Gate;
	FTimerHandle Timer;
	FDelegateHandle SpawnedHandle;
	bool bStarted = false;
};
