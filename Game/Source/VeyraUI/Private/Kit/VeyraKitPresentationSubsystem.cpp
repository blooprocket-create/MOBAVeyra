// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Kit/VeyraKitPresentationSubsystem.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/LineBatchComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Greybox/VeyraGreyboxOutline.h"
#include "Greybox/VeyraGreyboxSettings.h"
#include "Greybox/VeyraGreyboxSubsystem.h"
#include "Kit/VeyraKitPresentationSettings.h"
#include "Movement/VeyraDrawnBody.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Statuses/VeyraStatusComponent.h"
#include "Targeting/VeyraTargeting.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "VeyraUILog.h"

namespace
{
	/** The statuses a unit holds: a Vanguard's on its participant, any other unit's on itself. Every machine receives them. */
	const UVeyraStatusComponent* StatusesOf(const AActor& Unit)
	{
		const UAbilitySystemComponent* Abilities = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Unit);
		const AActor* Holder = Abilities ? Abilities->GetOwner() : nullptr;
		return Holder ? Holder->FindComponentByClass<UVeyraStatusComponent>() : nullptr;
	}

	FName NameOf(const FVeyraContentId& Id)
	{
		return FName(*Id.ToString());
	}
}

bool UVeyraKitPresentationSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// Presentation is for worlds someone watches: game and play-in-editor worlds, never a dedicated server's.
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && World->GetNetMode() != NM_DedicatedServer && Super::ShouldCreateSubsystem(Outer);
}

void UVeyraKitPresentationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// Side colours and the server's clock come from the grey-box presentation.
	Collection.InitializeDependency<UVeyraGreyboxSubsystem>();
	const UVeyraKitPresentationSettings& Settings = *GetDefault<UVeyraKitPresentationSettings>();
	TArray<FString> Problems = Settings.Validate();
	if (Problems.IsEmpty())
	{
		StrandEffect = Settings.StrandEffect.LoadSynchronous();
		BurstEffect = Settings.BurstEffect.LoadSynchronous();
		if (!StrandEffect || !BurstEffect)
		{
			Problems.Add(TEXT("StrandEffect: the strand's and the bursts' effects do not both load; run BuildEffects.ps1."));
		}
		for (const FVeyraStatusMark& Mark : Settings.StatusMarks)
		{
			UNiagaraSystem* Effect = Mark.Effect.LoadSynchronous();
			if (!Effect)
			{
				Problems.Add(FString::Printf(TEXT("StatusMarks: %s's effect %s does not load."), *Mark.Status.ToString(), *Mark.Effect.ToString()));
			}
			MarkEffects.Add(Mark.Status, Effect);
		}
	}
	for (const FString& Problem : Problems)
	{
		UE_LOG(LogVeyraUI, Error, TEXT("The kit presentation is off: %s"), *Problem);
	}
	bReady = Problems.IsEmpty();
}

void UVeyraKitPresentationSubsystem::Deinitialize()
{
	for (const TObjectPtr<UNiagaraComponent>& Bead : Beads)
	{
		if (Bead)
		{
			Bead->DestroyComponent();
		}
	}
	Beads.Reset();
	for (const TPair<TPair<TWeakObjectPtr<const AActor>, FName>, TWeakObjectPtr<UNiagaraComponent>>& Mark : Marks)
	{
		if (UNiagaraComponent* Shown = Mark.Value.Get())
		{
			Shown->DestroyComponent();
		}
	}
	Marks.Reset();
	if (Lines)
	{
		Lines->DestroyComponent();
		Lines = nullptr;
	}
	bReady = false;
	Super::Deinitialize();
}

void UVeyraKitPresentationSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	Refresh();
}

TStatId UVeyraKitPresentationSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UVeyraKitPresentationSubsystem, STATGROUP_Tickables);
}

void UVeyraKitPresentationSubsystem::Refresh()
{
	const UVeyraGreyboxSubsystem* Greybox = GetWorld()->GetSubsystem<UVeyraGreyboxSubsystem>();
	if (!bReady || !Greybox)
	{
		return;
	}
	if (!bIndexed)
	{
		Index = FVeyraKitPresentationIndex::Build(UVeyraAbilitiesTuningSubsystem::Get());
		bIndexed = true;
	}
	if (!Lines)
	{
		Lines = NewObject<ULineBatchComponent>(this, NAME_None, RF_Transient);
		Lines->bCalculateAccurateBounds = false;
		Lines->RegisterComponentWithWorld(GetWorld());
	}
	Lines->Flush();
	const double Now = Greybox->GetServerNow();
	RefreshStrandsAndRings(Now);
	DrawBursts(Now);
	RefreshMarks();
}

FVector UVeyraKitPresentationSubsystem::FeetOf(const AActor& Unit) const
{
	float Radius = 0.0f;
	float HalfHeight = 0.0f;
	Unit.GetSimpleCollisionCylinder(Radius, HalfHeight);
	return VeyraDrawnBody::LocationOf(Unit) + FVector::UpVector * (GetDefault<UVeyraGreyboxSettings>()->TelegraphLift - HalfHeight);
}

FVector UVeyraKitPresentationSubsystem::StrandPointOf(const AActor& Unit) const
{
	float Radius = 0.0f;
	float HalfHeight = 0.0f;
	Unit.GetSimpleCollisionCylinder(Radius, HalfHeight);
	// From its drawn feet to the top of its drawn body, which may stand taller than its capsule (ADR-065 §11).
	const double Height = HalfHeight + GetDefault<UVeyraGreyboxSettings>()->VisualTopOf(Unit);
	const double Up = Height * GetDefault<UVeyraKitPresentationSettings>()->StrandHeightShare - HalfHeight;
	return VeyraDrawnBody::LocationOf(Unit) + FVector::UpVector * Up;
}

void UVeyraKitPresentationSubsystem::DrawRing(const FVector& Centre, double Radius, const FLinearColor& Color, float Thickness)
{
	FVeyraShape Circle;
	Circle.Kind = EVeyraShapeKind::Circle;
	Circle.Radius = Radius;
	for (const FVeyraOutlineSegment& Segment : VeyraGreyboxOutline::Of(FVeyraPlacedShape{ Circle, Centre, FVector::ForwardVector },
		GetDefault<UVeyraGreyboxSettings>()->CircleSegments))
	{
		Lines->DrawLine(Segment.Start, Segment.End, Color, SDPG_World, Thickness, 0.0f);
	}
}

void UVeyraKitPresentationSubsystem::RefreshStrandsAndRings(double Now)
{
	const UVeyraKitPresentationSettings& Settings = *GetDefault<UVeyraKitPresentationSettings>();
	const UVeyraGreyboxSubsystem& Greybox = *GetWorld()->GetSubsystem<UVeyraGreyboxSubsystem>();
	Strands.Reset();
	Rings.Reset();
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		const APawn& Unit = **It;
		const UVeyraStatusComponent* Statuses = Unit.IsHidden() ? nullptr : StatusesOf(Unit);
		if (!Statuses)
		{
			continue;
		}
		for (const FVeyraStatusEntry& Entry : Statuses->GetLedger().Entries)
		{
			const AActor* Source = Entry.SourceBody.Get();
			// A tether's strand, from the unit that holds its other end, while both show (ADR-071 §2).
			if (Source && Source != &Unit && !Source->IsHidden() && Settings.IsStrand(NameOf(Entry.Id)))
			{
				Strands.Add(FStrandShown{ Source, &Unit });
			}
			// A buff on its holder, whether its caster or an ally it was cast on: its aura follows its holder while it lasts,
			// and its payloads come from it (ADR-071 §3).
			const FLinearColor Color = Greybox.SideColorOf(Unit);
			for (const FVeyraShownAura& Aura : Index.AurasOf(Entry, Now))
			{
				const FVector Feet = FeetOf(Unit);
				DrawRing(Feet, Aura.Radius, Color, Settings.AuraThickness);
				DrawRing(Feet, Aura.Radius * FMath::Frac(Now / Settings.AuraPulseSeconds), Color, Settings.AuraThickness * 0.5f);
				Rings.Add(FRingShown{ &Unit, Aura.Radius, 1.0, false });
			}
			// A payload comes from its caster's body, the status's applier, wherever the buff's holder stands.
			for (const FVeyraShownBurst& Burst : Source ? Index.BurstsOf(Entry) : TArray<FVeyraShownBurst>())
			{
				const FString Key = FString::Printf(TEXT("%s/%d/%.3f/%.3f"), *Unit.GetPathName(), Entry.Sequence, Entry.StartedAt, Burst.At);
				if (Burst.At + Settings.BurstSeconds >= Now && !BurstsSeen.Contains(Key))
				{
					BurstsSeen.Add(Key);
					Bursts.Add(FBurst{ Source, Burst, Greybox.SideColorOf(*Source), false });
				}
			}
		}
	}
	// Each strand, its beads flowing from its holder to its source.
	PlaceBeads(Strands.Num() * Settings.StrandBeads);
	int32 Bead = 0;
	const UVeyraGreyboxSettings& Greyboxed = *GetDefault<UVeyraGreyboxSettings>();
	for (const FStrandShown& Strand : Strands)
	{
		const AActor& Source = *Strand.Source.Get();
		const AActor& Holder = *Strand.Holder.Get();
		const FVector From = StrandPointOf(Holder);
		const FVector To = StrandPointOf(Source);
		const FLinearColor Color = Greybox.SideColorOf(Source);
		Lines->DrawLine(From, To, Color, SDPG_World, Settings.StrandThickness, 0.0f);
		for (int32 Each = 0; Each < Settings.StrandBeads && Beads.IsValidIndex(Bead); ++Each, ++Bead)
		{
			UNiagaraComponent* Shown = Beads[Bead];
			Shown->SetWorldLocation(FMath::Lerp(From, To, VeyraKitPresentation::BeadShareAt(Each, Settings.StrandBeads, Now, Settings.StrandFlowSeconds)));
			Shown->SetVariableLinearColor(Greyboxed.EffectColorParameter, Color);
			if (!Shown->IsActive())
			{
				Shown->Activate(/*bReset*/ true);
			}
		}
	}
}

void UVeyraKitPresentationSubsystem::PlaceBeads(int32 Count)
{
	while (Beads.Num() < Count)
	{
		UNiagaraComponent* Bead = UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), StrandEffect, FVector::ZeroVector, FRotator::ZeroRotator,
			FVector::OneVector, /*bAutoDestroy*/ false, /*bAutoActivate*/ false, ENCPoolMethod::None);
		if (!Bead)
		{
			// Nothing renders here (a world without a viewport); no bead shows.
			return;
		}
		Beads.Add(Bead);
	}
	for (int32 Unused = Count; Unused < Beads.Num(); ++Unused)
	{
		if (Beads[Unused] && Beads[Unused]->IsActive())
		{
			Beads[Unused]->DeactivateImmediate();
		}
	}
}

TArray<UNiagaraComponent*> UVeyraKitPresentationSubsystem::GetActiveBeads() const
{
	TArray<UNiagaraComponent*> Active;
	for (const TObjectPtr<UNiagaraComponent>& Bead : Beads)
	{
		if (Bead && Bead->IsActive())
		{
			Active.Add(Bead);
		}
	}
	return Active;
}

void UVeyraKitPresentationSubsystem::DrawBursts(double Now)
{
	const UVeyraKitPresentationSettings& Settings = *GetDefault<UVeyraKitPresentationSettings>();
	const UVeyraGreyboxSettings& Greyboxed = *GetDefault<UVeyraGreyboxSettings>();
	for (int32 Each = Bursts.Num() - 1; Each >= 0; --Each)
	{
		FBurst& Burst = Bursts[Each];
		const AActor* Holder = Burst.Holder.Get();
		// A payload comes only from a living caster; one already spread is done.
		if (!Holder || !VeyraTargeting::IsAlive(Holder) || Now > Burst.Shown.At + Settings.BurstSeconds)
		{
			Bursts.RemoveAtSwap(Each);
			continue;
		}
		const TOptional<double> Share = VeyraKitPresentation::BurstShareAt(Burst.Shown, Now, Settings.BurstSeconds);
		if (!Share || Holder->IsHidden())
		{
			continue;
		}
		const FVector Feet = FeetOf(*Holder);
		if (!Burst.bPlayed)
		{
			Burst.bPlayed = true;
			if (UNiagaraComponent* Played = UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), BurstEffect, Feet, FRotator::ZeroRotator, FVector::OneVector,
				/*bAutoDestroy*/ true, /*bAutoActivate*/ true, ENCPoolMethod::AutoRelease))
			{
				Played->SetVariableLinearColor(Greyboxed.EffectColorParameter, Burst.Color);
			}
		}
		DrawRing(Feet, Burst.Shown.Radius * Share.GetValue(), Burst.Color, Settings.BurstThickness);
		Rings.Add(FRingShown{ Holder, Burst.Shown.Radius, Share.GetValue(), true });
	}
	// Forget the bursts long done, so the list stays short over a match.
	if (Bursts.IsEmpty())
	{
		BurstsSeen.Reset();
	}
}

void UVeyraKitPresentationSubsystem::RefreshMarks()
{
	const UVeyraKitPresentationSettings& Settings = *GetDefault<UVeyraKitPresentationSettings>();
	const UVeyraGreyboxSettings& Greyboxed = *GetDefault<UVeyraGreyboxSettings>();
	const UVeyraGreyboxSubsystem& Greybox = *GetWorld()->GetSubsystem<UVeyraGreyboxSubsystem>();
	TSet<TPair<TWeakObjectPtr<const AActor>, FName>> Held;
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		const APawn& Unit = **It;
		const UVeyraStatusComponent* Statuses = StatusesOf(Unit);
		if (!Statuses || !VeyraTargeting::IsAlive(&Unit))
		{
			continue;
		}
		for (const FVeyraStatusEntry& Entry : Statuses->GetLedger().Entries)
		{
			const FVeyraStatusMark* Mark = Settings.MarkOf(NameOf(Entry.Id));
			UNiagaraSystem* Effect = Mark ? MarkEffects.FindRef(Mark->Status) : nullptr;
			USceneComponent* Anchor = Effect ? VeyraDrawnBody::AnchorOf(Unit) : nullptr;
			if (!Anchor)
			{
				continue;
			}
			const TPair<TWeakObjectPtr<const AActor>, FName> Key(&Unit, Mark->Status);
			Held.Add(Key);
			UNiagaraComponent* Shown = Marks.FindRef(Key).Get();
			if (!Shown)
			{
				float Radius = 0.0f;
				float HalfHeight = 0.0f;
				Unit.GetSimpleCollisionCylinder(Radius, HalfHeight);
				const double Up = (HalfHeight + Greyboxed.VisualTopOf(Unit)) * Mark->HeightShare - HalfHeight;
				Shown = UNiagaraFunctionLibrary::SpawnSystemAttached(Effect, Anchor, NAME_None, FVector::UpVector * Up, FRotator::ZeroRotator,
					EAttachLocation::KeepRelativeOffset, /*bAutoDestroy*/ false);
				if (!Shown)
				{
					continue;
				}
				Shown->SetVariableFloat(Greyboxed.EffectScaleParameter, Mark->Scale);
				Marks.Add(Key, Shown);
			}
			// In the colour of the side that marked it, as the viewer sees that side, even while the unit that marked it is
			// beyond this machine's view.
			const AActor* Source = Entry.SourceBody.Get();
			Shown->SetVariableLinearColor(Greyboxed.EffectColorParameter, Source ? Greybox.SideColorOf(*Source) : Greybox.ColorOfSide(Entry.SourceTeam));
			Shown->SetVisibility(!Unit.IsHidden());
		}
	}
	for (auto It = Marks.CreateIterator(); It; ++It)
	{
		if (!Held.Contains(It.Key()))
		{
			if (UNiagaraComponent* Shown = It.Value().Get())
			{
				Shown->DestroyComponent();
			}
			It.RemoveCurrent();
		}
	}
}

TArray<UNiagaraComponent*> UVeyraKitPresentationSubsystem::FindMarks(const AActor& Unit) const
{
	TArray<UNiagaraComponent*> Found;
	for (const TPair<TPair<TWeakObjectPtr<const AActor>, FName>, TWeakObjectPtr<UNiagaraComponent>>& Mark : Marks)
	{
		if (Mark.Key.Key.Get() == &Unit && Mark.Value.IsValid())
		{
			Found.Add(Mark.Value.Get());
		}
	}
	return Found;
}
