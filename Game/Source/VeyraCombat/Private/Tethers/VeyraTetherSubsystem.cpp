// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Tethers/VeyraTetherSubsystem.h"

#include "AbilitySystemComponent.h"
#include "Attributes/VeyraVitalsSet.h"
#include "Engine/World.h"
#include "Movement/VeyraForcedMovementTypes.h"
#include "Targeting/VeyraTargeting.h"
#include "Tuning/VeyraCombatTuningSubsystem.h"
#include "VeyraCombatLog.h"
#include "VeyraCombatVerbs.h"

namespace
{
	bool IsValidSpec(const FVeyraTetherSpec& Spec)
	{
		const bool bSnaps = Spec.SnapDistance > 0.0;
		const bool bSiphons = Spec.SiphonDamage.IsValid();
		return Spec.Id.IsValid() && Spec.MaxRange > 0.0 && FMath::IsFinite(Spec.MaxRange) && Spec.DurationSeconds > 0.0
			&& FMath::IsFinite(Spec.DurationSeconds) && Spec.SnapDistance >= 0.0 && FMath::IsFinite(Spec.SnapDistance)
			&& (bSnaps ? Spec.SnapSpeed > 0.0 && FMath::IsFinite(Spec.SnapSpeed) : Spec.SnapSpeed == 0.0)
			&& (!bSiphons || (Spec.SiphonIntervalSeconds > 0.0 && FMath::IsFinite(Spec.SiphonIntervalSeconds) && Spec.SiphonHealShare >= 0.0
				&& FMath::IsFinite(Spec.SiphonHealShare)));
	}

	double HealthOf(const UAbilitySystemComponent& Unit)
	{
		return Unit.GetNumericAttribute(UVeyraVitalsSet::GetHealthAttribute());
	}

	const AActor* LivingBody(const UAbilitySystemComponent* Unit)
	{
		const AActor* Body = Unit ? Unit->GetAvatarActor() : nullptr;
		return Body && VeyraTargeting::IsAlive(Body) ? Body : nullptr;
	}
}

bool UVeyraTetherSubsystem::Tether(UAbilitySystemComponent& Source, UAbilitySystemComponent& Target, const FVeyraTetherSpec& Spec)
{
	UWorld* World = GetWorld();
	const AActor* SourceBody = LivingBody(&Source);
	const AActor* TargetBody = LivingBody(&Target);
	if (!World || !IsValidSpec(Spec))
	{
		UE_LOG(LogVeyraCombat, Warning, TEXT("Refused tether %s from %s: its spec is invalid."), *Spec.Id.ToString(), *GetNameSafe(Source.GetOwner()));
		return false;
	}
	if (!SourceBody || !TargetBody || SourceBody == TargetBody)
	{
		UE_LOG(LogVeyraCombat, Verbose, TEXT("Refused tether %s from %s to %s: a unit without a living body, or a unit tethered to itself."),
			*Spec.Id.ToString(), *GetNameSafe(Source.GetOwner()), *GetNameSafe(Target.GetOwner()));
		return false;
	}
	const int32 Older = Links.IndexOfByPredicate([&Source, &Spec](const FLink& Link) { return Link.Source.Get() == &Source && Link.Spec.Id == Spec.Id; });
	if (Older != INDEX_NONE)
	{
		End(Older, EVeyraTetherEndReason::Replaced);
	}
	for (const FVeyraStatusSpec& Status : Spec.TargetStatuses)
	{
		FVeyraStatusSpec Held = Status;
		Held.DurationSeconds = Spec.DurationSeconds;
		VeyraCombat::ApplyStatus(Source, Target, Held);
	}
	FLink& Link = Links.AddDefaulted_GetRef();
	Link.Source = &Source;
	Link.Target = &Target;
	Link.Spec = Spec;
	Link.EndsAt = World->GetTimeSeconds() + Spec.DurationSeconds;
	Link.NextPulseAt = World->GetTimeSeconds() + Spec.SiphonIntervalSeconds;
	if (!World->GetTimerManager().IsTimerActive(CheckTimer))
	{
		World->GetTimerManager().SetTimer(CheckTimer, this, &UVeyraTetherSubsystem::Check,
			static_cast<float>(UVeyraCombatTuningSubsystem::Get().Tethers.CheckSeconds), /*bLoop*/ true);
	}
	return true;
}

void UVeyraTetherSubsystem::Release(const UAbilitySystemComponent& Source, const FVeyraContentId& Id)
{
	const int32 Index = Links.IndexOfByPredicate([&Source, &Id](const FLink& Link) { return Link.Source.Get() == &Source && Link.Spec.Id == Id; });
	if (Index != INDEX_NONE)
	{
		End(Index, EVeyraTetherEndReason::Released);
	}
}

bool UVeyraTetherSubsystem::IsTethered(const UAbilitySystemComponent& Source, const FVeyraContentId& Id) const
{
	return Links.ContainsByPredicate([&Source, &Id](const FLink& Link) { return Link.Source.Get() == &Source && Link.Spec.Id == Id; });
}

bool UVeyraTetherSubsystem::IsTetheredBy(const AActor& Body, EVeyraTeam Side) const
{
	return Links.ContainsByPredicate([&Body, Side](const FLink& Link)
	{
		const UAbilitySystemComponent* Target = Link.Target.Get();
		const UAbilitySystemComponent* Source = Link.Source.Get();
		return Target && Source && Target->GetAvatarActor() == &Body && VeyraTeams::TeamOf(Source->GetOwner()) == Side;
	});
}

void UVeyraTetherSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CheckTimer);
	}
	Links.Reset();
	Super::Deinitialize();
}

void UVeyraTetherSubsystem::Check()
{
	const UWorld* World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;
	// Siphons pulse while their tethers hold, and once more as their time runs out (ADR-065 §8). The pulses are taken
	// first and dealt after, since a pulse can end a life, and a death's listeners can change the tethers.
	TArray<FPulse> Pulses;
	for (FLink& Link : Links)
	{
		const TOptional<EVeyraTetherEndReason> Reason = Judge(Link, Now);
		if (Link.Spec.SiphonDamage.IsValid() && Now >= Link.NextPulseAt && (!Reason.IsSet() || Reason.GetValue() == EVeyraTetherEndReason::Expired))
		{
			Pulses.Add(FPulse{ Link.Source, Link.Target, Link.Spec.SiphonDamage, Link.Spec.SiphonHealShare });
			Link.NextPulseAt += Link.Spec.SiphonIntervalSeconds;
		}
	}
	for (const FPulse& Pulse : Pulses)
	{
		Siphon(Pulse);
	}
	// Downward, so a tether an ending one's listener adds is judged next time.
	for (int32 Index = Links.Num() - 1; Index >= 0; --Index)
	{
		if (!Links.IsValidIndex(Index))
		{
			continue;
		}
		const TOptional<EVeyraTetherEndReason> Reason = Judge(Links[Index], Now);
		if (!Reason.IsSet())
		{
			continue;
		}
		if (Reason.GetValue() == EVeyraTetherEndReason::Stretched && Links[Index].Spec.SnapDistance > 0.0)
		{
			// Stretched, it snaps the target back toward its source once, then lets go (Roster Bible: Patch's E).
			const FLink& Link = Links[Index];
			const FVector Toward = (Link.Source->GetAvatarActor()->GetActorLocation() - Link.Target->GetAvatarActor()->GetActorLocation()).GetSafeNormal2D();
			VeyraCombat::Displace(*Link.Source, *Link.Target, FVeyraDisplacement{ Toward, Link.Spec.SnapDistance, Link.Spec.SnapSpeed });
		}
		if (Links.IsValidIndex(Index))
		{
			End(Index, Reason.GetValue());
		}
	}
	if (Links.IsEmpty() && World)
	{
		GetWorld()->GetTimerManager().ClearTimer(CheckTimer);
	}
}

void UVeyraTetherSubsystem::Siphon(const FPulse& Pulse)
{
	UAbilitySystemComponent* Source = Pulse.Source.Get();
	UAbilitySystemComponent* Target = Pulse.Target.Get();
	if (!Source || !Target)
	{
		return;
	}
	// What the target lost, after its resistances and any shield, is what the source restores its share of.
	const double Before = HealthOf(*Target);
	if (!VeyraCombat::DealPreparedDamage(Pulse.Damage, *Target, {}))
	{
		return;
	}
	const double Lost = FMath::Max(0.0, Before - HealthOf(*Target));
	if (Lost > 0.0 && Pulse.HealShare > 0.0)
	{
		VeyraCombat::RestoreHealthFrom(*Source, *Source, Lost * Pulse.HealShare);
	}
}

TOptional<EVeyraTetherEndReason> UVeyraTetherSubsystem::Judge(const FLink& Link, double Now) const
{
	const AActor* SourceBody = LivingBody(Link.Source.Get());
	const AActor* TargetBody = LivingBody(Link.Target.Get());
	if (!SourceBody || !TargetBody)
	{
		return EVeyraTetherEndReason::Died;
	}
	if (Now >= Link.EndsAt)
	{
		return EVeyraTetherEndReason::Expired;
	}
	if (VeyraTargeting::EdgeToEdgeDistance(*SourceBody, *TargetBody) > Link.Spec.MaxRange)
	{
		return EVeyraTetherEndReason::Stretched;
	}
	if (VeyraTargeting::AreHostile(SourceBody, TargetBody) && VeyraTargeting::IsUntargetable(*TargetBody))
	{
		return EVeyraTetherEndReason::Untargetable;
	}
	return {};
}

void UVeyraTetherSubsystem::End(int32 Index, EVeyraTetherEndReason Reason)
{
	const FLink Link = Links[Index];
	Links.RemoveAt(Index);
	if (UAbilitySystemComponent* Target = Link.Target.Get())
	{
		for (const FVeyraStatusSpec& Status : Link.Spec.TargetStatuses)
		{
			VeyraCombat::RemoveStatus(*Target, Status.Id);
		}
	}
	UE_LOG(LogVeyraCombat, Verbose, TEXT("Tether %s from %s ended (%d)."), *Link.Spec.Id.ToString(),
		*GetNameSafe(Link.Source.IsValid() ? Link.Source->GetOwner() : nullptr), static_cast<int32>(Reason));
	OnTetherEnded.Broadcast(FVeyraTetherEnd{ Link.Source, Link.Target, Link.Spec.Id, Reason });
}
