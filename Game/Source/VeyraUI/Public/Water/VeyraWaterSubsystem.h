// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Water/VeyraWaterRipples.h"
#include "VeyraWaterSubsystem.generated.h"

class UMaterialInstanceDynamic;
class UNiagaraComponent;
class UNiagaraSystem;
class UPrimitiveComponent;
class UTextureRenderTarget2D;

/**
 * The river answering the units wading it, on a machine someone watches (ADR-076): ripples simulated over a window of
 * water that follows the camera's view, stirred by every unit in the water, and drawn by the river's material; a wake
 * at each wading unit's feet and a splash where one runs in. Presentation only: it reads where units are and never
 * changes anything they do. Every unit this machine knows of stirs it, seen or not: ripples are a tell for whoever
 * watches the river (author ruling, 2026-10-12).
 */
UCLASS()
class VEYRAUI_API UVeyraWaterSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual TStatId GetStatId() const override;

	/** Stirs the water from the units in it and steps its ripples DeltaSeconds on, their window about Focus. The tick calls this with the camera's view; tests call it directly. */
	void Refresh(float DeltaSeconds, const FVector2D& Focus);

	/** Whether its settings are whole and its material and effects loaded. */
	bool IsReady() const { return bReady; }

	/** Whether it found the river's drawn surface, whose material takes the ripples. */
	bool HasRiver() const { return River.IsValid(); }

	/** The window the ripples were last stepped over. */
	const FVeyraRippleWindow& GetWindow() const { return Window; }

	/** The units' stirs the last refresh gave the ripples, as their material took them. */
	const TArray<FLinearColor>& GetSources() const { return Sources; }

	/** The ripples' latest heights, once the river is found on a machine that renders; null otherwise. */
	UTextureRenderTarget2D* GetRipples() const;

	/** Whether the ripples are stepped here: the river found, its materials made, and its targets, which a machine that renders nothing has none of. */
	bool CanRipple() const;

	/** Whether Unit waded the river at the last refresh. */
	bool IsWading(const AActor& Unit) const;

	/** The wake at Unit's feet while it wades, on a machine that draws effects; null otherwise. */
	UNiagaraComponent* FindWake(const AActor& Unit) const;

	/** How many splashes have been thrown since this world began. */
	int32 GetSplashCount() const { return Splashes; }

private:
	/** Finds the river's drawn surface by its tag and readies the ripples' targets and materials, once. */
	void FindRiver();

	/** Each drawn unit in the water: its stir, its wake, and a splash where it ran in. */
	void Stir();

	/** Steps the ripples Steps times, the first step carrying the window's shift. */
	void Step(int32 Steps);

	/** One unit in the water as this machine draws it: its wake, and whether it waded at the last refresh. */
	struct FWader
	{
		TWeakObjectPtr<UNiagaraComponent> Wake;
		bool bWading = false;
	};

	void Release(FWader& Wader);

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> States[2];

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> Stepper;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> Surface;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraSystem> WakeSystem;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraSystem> SplashSystem;

	TWeakObjectPtr<UPrimitiveComponent> River;
	TMap<TWeakObjectPtr<const AActor>, FWader> Waders;
	TArray<FLinearColor> Sources;
	FVeyraRippleWindow Window;
	int32 Current = 0;
	double Owed = 0.0;
	double Phase = 0.0;
	int32 Splashes = 0;
	bool bReady = false;
	bool bSearched = false;
};
