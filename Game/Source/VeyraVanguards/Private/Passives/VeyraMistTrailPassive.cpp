// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Passives/VeyraMistTrailPassive.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/VeyraGameplayAbility.h"
#include "Attributes/VeyraOffenceSet.h"
#include "Delivery/VeyraAreaDelivery.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Targeting/VeyraTargeting.h"
#include "Targeting/VeyraVisibility.h"
#include "Teams/VeyraTeam.h"
#include "TimerManager.h"
#include "Tuning/VeyraVanguardsTuningSubsystem.h"
#include "Units/VeyraUnit.h"
#include "VeyraCombatVerbs.h"
#include "VeyraVanguardsLog.h"

void UVeyraMistTrailPassive::Start(UAbilitySystemComponent& Owner, const FVeyraContentId& InPassiveId)
{
	Super::Start(Owner, InPassiveId);
	UWorld* World = GetWorld();
	const FVeyraMistTrailTuning* Tuning = UVeyraVanguardsTuningSubsystem::FindMistTrail(PassiveId);
	if (World && Tuning)
	{
		// World time, so a pause holds it.
		World->GetTimerManager().SetTimer(LookTimer, FTimerDelegate::CreateUObject(this, &UVeyraMistTrailPassive::Look), static_cast<float>(Tuning->LookSeconds),
			/*bLoop*/ true);
	}
}

void UVeyraMistTrailPassive::Stop()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LookTimer);
	}
	Approach.Reset();
	Shielded.Reset();
	Super::Stop();
}

void UVeyraMistTrailPassive::Look()
{
	UAbilitySystemComponent* Owner = OwnerAbilitySystem.Get();
	const FVeyraMistTrailTuning* Tuning = UVeyraVanguardsTuningSubsystem::FindMistTrail(PassiveId);
	// Its owner's body: before the pawn spawns, the avatar is the participant, which has none (ADR-034 §3).
	const APawn* Body = Owner ? Cast<APawn>(Owner->GetAvatarActor()) : nullptr;
	UWorld* World = GetWorld();
	if (!Owner || !Tuning || !Body || !World)
	{
		return;
	}
	if (!VeyraTargeting::IsAlive(Body))
	{
		// The fallen leave no trail, and come back with no way behind them.
		Approach.Reset();
		bWasInFog = false;
		LayingUntil = 0.0;
		return;
	}
	const double Now = World->GetTimeSeconds();
	const FVector Here = Body->GetActorLocation();
	const bool bInFog = VeyraVisibility::FogVolumeAt(World, Here) != INDEX_NONE;
	if (bInFog && !bWasInFog)
	{
		// Into the fog: the trail marks the way she came, and goes on with her a while (ADR-036 §5).
		++TrailCount;
		EnteredAt = Here;
		Shielded.Reset();
		LayingUntil = Now + Tuning->LaySeconds;
		LayApproach(*Owner, *Tuning, Here);
		Travelled = 0.0;
		UE_LOG(LogVeyraVanguards, Verbose, TEXT("%s leaves a Mist Trail into the fog at %s."), *GetNameSafe(Owner->GetOwner()), *Here.ToCompactString());
	}
	else if (Now < LayingUntil)
	{
		// Every spacing along the way she went since the last look, the leftover carried on: its spacing holds at any pace.
		const double Stride = FVector::Dist2D(Here, LastSeen);
		const FVector Facing = (Here - LastSeen).GetSafeNormal2D();
		double Along = Tuning->Spacing - Travelled;
		Travelled += Stride;
		for (; Travelled >= Tuning->Spacing; Travelled -= Tuning->Spacing, Along += Tuning->Spacing)
		{
			Lay(*Owner, *Tuning, Stride > 0.0 ? FMath::Lerp(LastSeen, Here, FMath::Clamp(Along / Stride, 0.0, 1.0)) : Here, Facing);
		}
	}
	bWasInFog = bInFog;
	LastSeen = Here;

	// The way she came: her recent positions, no more than the approach's length and a spacing back.
	Approach.Add(Here);
	double Kept = 0.0;
	for (int32 Index = Approach.Num() - 1; Index > 0; --Index)
	{
		Kept += FVector::Dist2D(Approach[Index], Approach[Index - 1]);
		if (Kept > Tuning->ApproachLength + Tuning->Spacing)
		{
			Approach.RemoveAt(0, Index - 1);
			break;
		}
	}
	ShieldFollowers(*Owner, *Tuning);
}

void UVeyraMistTrailPassive::LayApproach(UAbilitySystemComponent& Owner, const FVeyraMistTrailTuning& Tuning, const FVector& Here)
{
	// Back along the way she came from where she entered: an area there, then one every spacing, as far as the approach reaches.
	TArray<FVector> Path = Approach;
	Path.Add(Here);
	double Along = 0.0;
	double NextAt = 0.0;
	for (int32 Index = Path.Num() - 1; Index > 0 && NextAt <= Tuning.ApproachLength; --Index)
	{
		const FVector& From = Path[Index];
		const FVector& To = Path[Index - 1];
		const double Segment = FVector::Dist2D(From, To);
		while (NextAt <= Along + Segment && NextAt <= Tuning.ApproachLength)
		{
			const double Share = Segment > 0.0 ? (NextAt - Along) / Segment : 0.0;
			Lay(Owner, Tuning, FMath::Lerp(From, To, Share), (From - To).GetSafeNormal2D());
			NextAt += Tuning.Spacing;
		}
		Along += Segment;
	}
	// With no way behind her yet, the trail starts where she stands.
	if (NextAt == 0.0)
	{
		Lay(Owner, Tuning, Here, FVector::ZeroVector);
	}
}

void UVeyraMistTrailPassive::Lay(UAbilitySystemComponent& Owner, const FVeyraMistTrailTuning& Tuning, const FVector& Where, const FVector& Facing)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	FVeyraEffectFrame Placement;
	Placement.Origin = Where;
	Placement.Direction = Facing.IsNearlyZero() ? FVector::ForwardVector : Facing;
	Placement.bOriginIsCaster = false;
	// A passive has no ranks: its area at its first, from its owner's Level now.
	VeyraAreaDelivery::LayAt(*World, Owner, Tuning.Area, Placement, /*Rank*/ 1, UVeyraGameplayAbility::GetCasterLevel(Owner), /*CastId*/ 0);
}

void UVeyraMistTrailPassive::ShieldFollowers(UAbilitySystemComponent& Owner, const FVeyraMistTrailTuning& Tuning)
{
	UWorld* World = GetWorld();
	const int32 Entered = World && EnteredAt.IsSet() ? VeyraVisibility::FogVolumeAt(World, EnteredAt.GetValue()) : INDEX_NONE;
	if (Entered == INDEX_NONE)
	{
		return;
	}
	const EVeyraTeam Side = VeyraTeams::TeamOf(Owner.GetOwner());
	const FVeyraMistShieldTuning& Shield = Tuning.FollowShield;
	FVeyraShieldGrant Grant;
	Grant.Id = PassiveId;
	Grant.Amount = Shield.Amount + Shield.AmountPerLevel * (UVeyraGameplayAbility::GetCasterLevel(Owner) - 1)
		+ Owner.GetNumericAttribute(UVeyraOffenceSet::GetMagicPowerAttribute()) * Shield.MagicPowerRatio;
	Grant.MaxAmount = Grant.Amount;
	Grant.DurationSeconds = Shield.DurationSeconds;
	Grant.Reapply = EVeyraShieldReapply::Replace;
	for (TActorIterator<APawn> It(World); It; ++It)
	{
		APawn* Ally = *It;
		UAbilitySystemComponent* Abilities = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Ally);
		// An allied Vanguard who followed her trail into the same fog: once each trail.
		if (!Abilities || Abilities == &Owner || Shielded.Contains(Abilities) || !VeyraUnits::IsVanguard(Ally) || VeyraTeams::TeamOf(Ally) != Side
			|| !VeyraTargeting::IsAlive(Ally) || !VeyraCombat::HasStatusFrom(Ally, Tuning.FollowStatus, Owner)
			|| VeyraVisibility::FogVolumeAt(World, Ally->GetActorLocation()) != Entered)
		{
			continue;
		}
		Shielded.Add(Abilities);
		if (Grant.Amount > 0.0)
		{
			VeyraCombat::GrantShield(Owner, *Abilities, Grant);
		}
	}
}
