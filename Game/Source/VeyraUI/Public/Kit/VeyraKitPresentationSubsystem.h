// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Content/VeyraContentId.h"
#include "Kit/VeyraKitPresentation.h"
#include "Subsystems/WorldSubsystem.h"

#include "VeyraKitPresentationSubsystem.generated.h"

class APawn;
class ULineBatchComponent;
class UNiagaraComponent;
class UNiagaraSystem;

/**
 * Shows Vanguards' kits beyond their bodies on every machine someone watches (ADR-071): a tether as a strand from the
 * unit that holds its other end, with its siphon flowing along it; a self-buff's aura as a ring on the ground while it
 * lasts and its end payload as a ring spreading to its reach; a status as a mark on the unit that holds it; and an
 * ability's own cast effect. It reads only what every client receives (the status ledgers and Abilities.json) and
 * decides nothing.
 */
UCLASS()
class VEYRAUI_API UVeyraKitPresentationSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }

	/** Brings the kit presentation up to date with the world. Its tick calls this; tests call it directly. */
	void Refresh();

	/** Ability's own cast effect and its scale, if it has one (ADR-071 §4); null for the shared flash. */
	UNiagaraSystem* CastEffectOf(const FVeyraContentId& Ability, float& OutScale) const;

	/** What the last refresh drew. */
	struct FStrandShown
	{
		TWeakObjectPtr<const AActor> Source;
		TWeakObjectPtr<const AActor> Holder;
	};
	const TArray<FStrandShown>& GetStrands() const { return Strands; }

	struct FRingShown
	{
		/** The body it surrounds: an aura's holder, a payload's caster. */
		TWeakObjectPtr<const AActor> Holder;
		double Radius = 0.0;
		/** A burst's share of its reach so far; 1 for an aura. */
		double Share = 1.0;
		bool bBurst = false;
	};
	const TArray<FRingShown>& GetRings() const { return Rings; }

	/** The marks poured on Unit now, one per marked status it holds. */
	TArray<UNiagaraComponent*> FindMarks(const AActor& Unit) const;

	/** The beads flowing along the strands now, StrandBeads per strand. */
	TArray<UNiagaraComponent*> GetActiveBeads() const;

private:
	void RefreshStrandsAndRings(double Now);
	void RefreshMarks();
	void DrawBursts(double Now);
	void DrawRing(const FVector& Centre, double Radius, const FLinearColor& Color, float Thickness);
	void PlaceBeads(int32 Count);

	/** Where a strand meets Unit: over its drawn feet, StrandHeightShare of its drawn height up. */
	FVector StrandPointOf(const AActor& Unit) const;

	/** Unit's drawn feet, lifted as the telegraphs are. */
	FVector FeetOf(const AActor& Unit) const;

	bool bReady = false;

	/** Abilities.json's auras and payloads, read once at the first refresh, when the tuning has loaded. */
	bool bIndexed = false;
	FVeyraKitPresentationIndex Index;

	UPROPERTY(Transient)
	TObjectPtr<ULineBatchComponent> Lines;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraSystem> StrandEffect;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraSystem> BurstEffect;

	/** Each mark's and cast's effect, loaded with the settings, by status or ability ID as written there. */
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UNiagaraSystem>> MarkEffects;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UNiagaraSystem>> CastEffects;

	/** The bead effects, reused frame to frame in strand order. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UNiagaraComponent>> Beads;

	/** The marks poured now, by holder and status. */
	TMap<TPair<TWeakObjectPtr<const AActor>, FName>, TWeakObjectPtr<UNiagaraComponent>> Marks;

	/** The bursts still to come or spreading, each once: by the buff's holder, its status's ledger entry and start. */
	struct FBurst
	{
		/** The caster it comes from: the body that applied the buff's status. */
		TWeakObjectPtr<const AActor> Holder;
		FVeyraShownBurst Shown;
		FLinearColor Color = FLinearColor::White;
		bool bPlayed = false;
	};
	TArray<FBurst> Bursts;
	TSet<FString> BurstsSeen;

	TArray<FStrandShown> Strands;
	TArray<FRingShown> Rings;
};
