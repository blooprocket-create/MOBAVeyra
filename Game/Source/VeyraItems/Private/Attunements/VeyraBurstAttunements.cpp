// Copyright © 2026 Wayfinder Studios. All rights reserved.

// ADR-051 §3's Attunements on UVeyraAttunementSubsystem: No Allegiance, Clean Break, Through the Guard, No One Coming and
// Reenactment. Each acts only on enemy Vanguards, from the holder's own hits, with every number from Items.json.

#include "AbilitySystemComponent.h"
#include "Absorption/VeyraAbsorptionLedger.h"
#include "Attributes/VeyraOffenceSet.h"
#include "Attunements/VeyraAttunementSubsystem.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Inventory/VeyraInventoryComponent.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Statuses/VeyraStatusTypes.h"
#include "Targeting/VeyraTargeting.h"
#include "Teams/VeyraTeam.h"
#include "Tuning/VeyraItemsTuningSubsystem.h"
#include "Units/VeyraUnit.h"
#include "VeyraCombatVerbs.h"

namespace
{
	/** A bonus Physical hit's amount for Holder (ADR-051 §3). */
	double BonusHitOf(const FVeyraBonusHitTuning& Bonus, const UAbilitySystemComponent& Holder)
	{
		return Bonus.Base + Bonus.PhysicalPowerRatio * Holder.GetNumericAttribute(UVeyraOffenceSet::GetPhysicalPowerAttribute());
	}

	/** Deals a bonus hit as a proc, which no Attunement answers as an action (ADR-023 §3). */
	void DealBonus(UAbilitySystemComponent& Holder, UAbilitySystemComponent& Target, EVeyraDamageType Type, double Amount, double Penetration = 0.0)
	{
		if (!(Amount > 0.0))
		{
			return;
		}
		FVeyraRawDamageEvent Bonus;
		Bonus.Components.Add({ Type, Amount });
		Bonus.Delivery = EVeyraDamageDelivery::Proc;
		Bonus.PhysicalPenetration.Flat = Penetration;
		VeyraCombat::DealDamage(Holder, Target, Bonus);
	}

	/** Whether an allied Vanguard of Target's, living, stands within Radius of it. */
	bool HasAllyNear(const UWorld& World, const UAbilitySystemComponent& Target, double Radius)
	{
		const AActor* Body = Target.GetAvatarActor();
		const EVeyraTeam Side = VeyraTeams::TeamOf(Target.GetOwner());
		if (!Body)
		{
			return false;
		}
		for (TActorIterator<APawn> It(const_cast<UWorld*>(&World)); It; ++It)
		{
			const APawn* Other = *It;
			if (Other != Body && VeyraUnits::IsVanguard(Other) && VeyraTeams::TeamOf(Other) == Side && VeyraTargeting::IsAlive(Other)
				&& FVector::Dist2D(Other->GetActorLocation(), Body->GetActorLocation()) <= Radius)
			{
				return true;
			}
		}
		return false;
	}
}

TArray<FVeyraContentId, TInlineAllocator<6>> UVeyraAttunementSubsystem::HeldBy(const UAbilitySystemComponent& Holder) const
{
	TArray<FVeyraContentId, TInlineAllocator<6>> Held;
	const AActor* Participant = Holder.GetOwner();
	const UVeyraInventoryComponent* Inventory = Participant ? Participant->FindComponentByClass<UVeyraInventoryComponent>() : nullptr;
	if (!Inventory)
	{
		return Held;
	}
	const FVeyraItemsTuning& Tuning = UVeyraItemsTuningSubsystem::Get();
	for (const FVeyraInventorySlot& Slot : Inventory->GetSlots())
	{
		if (const FVeyraItemDefinition* Item = Slot.IsEmpty() ? nullptr : Tuning.Items.Find(Slot.Item))
		{
			for (const FVeyraContentId& Attunement : Item->Attunement)
			{
				Held.AddUnique(Attunement);
			}
		}
	}
	return Held;
}

void UVeyraAttunementSubsystem::OnCastCommitted(const FVeyraCastEvent& Cast)
{
	if (const UAbilitySystemComponent* Caster = Cast.Caster.Get())
	{
		LastCast.Add(Caster, Cast.Ability);
	}
}

TOptional<UVeyraAttunementSubsystem::FAction> UVeyraAttunementSubsystem::ActionOf(const FVeyraDamageDealtEvent& Event, const UAbilitySystemComponent& Holder) const
{
	// A basic attack, or an ability hit named by the holder's latest committed cast; procs and effects over time are
	// no actions (ADR-051 §9.1).
	if (Event.Delivery == EVeyraDamageDelivery::BasicAttack)
	{
		FAction Basic;
		Basic.bBasicAttack = true;
		return Basic;
	}
	if (Event.Delivery == EVeyraDamageDelivery::Ability)
	{
		FAction Ability;
		Ability.Ability = LastCast.FindRef(&Holder);
		return Ability;
	}
	return {};
}

void UVeyraAttunementSubsystem::NoAllegiance(const FVeyraContentId& Attunement, const FVeyraDamageDealtEvent& Event, UAbilitySystemComponent& Holder,
	UAbilitySystemComponent& Target, double Now, TOptional<double> LastDealt)
{
	const TOptional<FAction> Action = ActionOf(Event, Holder);
	if (!Action || !VeyraTargeting::IsAlive(Target.GetOwner()))
	{
		return;
	}
	const FVeyraNoAllegianceTuning& Tuning = UVeyraItemsTuningSubsystem::Get().NoAllegiance.FindChecked(Attunement);
	Openings.RemoveAllSwap([Now](const FOpening& Entry) { return Entry.Until <= Now || !Entry.Holder.IsValid() || !Entry.Target.IsValid(); });
	const int32 Open = Openings.IndexOfByPredicate([&Holder, &Target, &Attunement](const FOpening& Entry) {
		return Entry.Holder.Get() == &Holder && Entry.Target.Get() == &Target && Entry.Attunement == Attunement;
	});
	if (Open != INDEX_NONE)
	{
		// The same action again pays nothing: setup, then a different commitment (Item Bible §8).
		if (Openings[Open].Opener == Action.GetValue())
		{
			return;
		}
		Openings.RemoveAtSwap(Open);
		DealBonus(Holder, Target, EVeyraDamageType::Physical, BonusHitOf(Tuning.Bonus, Holder), Tuning.Penetration);
		return;
	}
	// Only after a quiet period without damaging an enemy Vanguard does an action open one.
	if (LastDealt.IsSet() && Now - LastDealt.GetValue() < Tuning.QuietSeconds)
	{
		return;
	}
	FOpening& Opening = Openings.AddDefaulted_GetRef();
	Opening.Holder = &Holder;
	Opening.Target = &Target;
	Opening.Attunement = Attunement;
	Opening.Opener = Action.GetValue();
	Opening.Until = Now + Tuning.OpeningSeconds;
}

void UVeyraAttunementSubsystem::TallyForCleanBreak(const FVeyraContentId& Attunement, double Amount, UAbilitySystemComponent& Holder, UAbilitySystemComponent& Target,
	double Now)
{
	// What the holder personally dealt the Vanguard lately, procs included (Item Bible §8).
	const FVeyraCleanBreakTuning& Tuning = UVeyraItemsTuningSubsystem::Get().CleanBreak.FindChecked(Attunement);
	FTally* Tally = Tallies.FindByPredicate([&Holder, &Target, &Attunement](const FTally& Entry) {
		return Entry.Holder.Get() == &Holder && Entry.Target.Get() == &Target && Entry.Attunement == Attunement;
	});
	if (!Tally)
	{
		Tally = &Tallies.AddDefaulted_GetRef();
		Tally->Holder = &Holder;
		Tally->Target = &Target;
		Tally->Attunement = Attunement;
	}
	Tally->Hits.RemoveAll([Now, &Tuning](const TPair<double, double>& Hit) { return Hit.Key < Now - Tuning.WindowSeconds; });
	Tally->Hits.Emplace(Now, Amount);
}

void UVeyraAttunementSubsystem::CleanBreak(const FVeyraDeathEvent& Death)
{
	const UAbilitySystemComponent* Victim = Death.Victim.Get();
	Tallies.RemoveAllSwap([](const FTally& Entry) { return !Entry.Holder.IsValid() || !Entry.Target.IsValid(); });
	if (!Victim || !VeyraUnits::IsVanguard(Victim->GetOwner()))
	{
		return;
	}
	const FVeyraItemsTuning& Tuning = UVeyraItemsTuningSubsystem::Get();
	TArray<FTally> Ended;
	for (int32 Index = Tallies.Num() - 1; Index >= 0; --Index)
	{
		if (Tallies[Index].Target.Get() == Victim)
		{
			Ended.Add(MoveTemp(Tallies[Index]));
			Tallies.RemoveAtSwap(Index);
		}
	}
	for (const FTally& Tally : Ended)
	{
		UAbilitySystemComponent* Holder = const_cast<UAbilitySystemComponent*>(Tally.Holder.Get());
		const FVeyraCleanBreakTuning* CleanBreak = Tuning.CleanBreak.Find(Tally.Attunement);
		if (!Holder || !CleanBreak || !HeldBy(*Holder).Contains(Tally.Attunement) || !VeyraTargeting::IsAlive(Holder->GetOwner()))
		{
			continue;
		}
		// It died soon after the holder's last hit on it.
		double Dealt = 0.0;
		double LastHit = -1.0;
		for (const TPair<double, double>& Hit : Tally.Hits)
		{
			if (Hit.Key >= Death.DiedAtSeconds - CleanBreak->WindowSeconds)
			{
				Dealt += Hit.Value;
				LastHit = FMath::Max(LastHit, Hit.Key);
			}
		}
		if (LastHit < 0.0)
		{
			continue;
		}
		// A burst of decaying Movement Speed: every stack at once, each falling away in turn.
		FVeyraStatusSpec Speed;
		Speed.Id = Tally.Attunement;
		Speed.Kind = EVeyraStatusKind::MoveSpeed;
		Speed.Stacking = EVeyraStackingPolicy::Stacking;
		Speed.Magnitude = CleanBreak->SpeedPerStack;
		Speed.MaxStacks = CleanBreak->SpeedStacks;
		Speed.DurationSeconds = CleanBreak->SpeedStackSeconds;
		Speed.StackDecaySeconds = CleanBreak->SpeedStackSeconds;
		for (int32 Stack = 0; Stack < CleanBreak->SpeedStacks; ++Stack)
		{
			VeyraCombat::ApplyStatus(*Holder, *Holder, Speed);
		}
		// And a shield sized by what it dealt, replacing the last rather than stacking (Item Bible §8).
		const double Amount = FMath::Min(CleanBreak->ShieldShare * Dealt, CleanBreak->ShieldCap);
		if (Amount > 0.0)
		{
			FVeyraShieldGrant Shield;
			Shield.Id = Tally.Attunement;
			Shield.Amount = Amount;
			Shield.MaxAmount = Amount;
			Shield.DurationSeconds = CleanBreak->ShieldSeconds;
			Shield.Reapply = EVeyraShieldReapply::Replace;
			VeyraCombat::GrantShield(*Holder, *Holder, Shield);
		}
	}
}

void UVeyraAttunementSubsystem::OnDamageResolved(const FVeyraDamageResolution& Resolution)
{
	UAbilitySystemComponent* Holder = Resolution.Source.Get();
	UAbilitySystemComponent* Target = Resolution.Target.Get();
	const UWorld* World = GetWorld();
	if (!Holder || !Target || !World)
	{
		return;
	}
	const double Now = World->GetTimeSeconds();
	// Clean Break counts what the holder's hit cost an enemy Vanguard as it resolves: a killing blow resolves before its death.
	if (VeyraUnits::IsVanguard(Target->GetOwner()) && VeyraTargeting::AreHostile(Holder->GetOwner(), Target->GetOwner()))
	{
		double Cost = Resolution.HealthLost + Resolution.TemporaryHealthSpent;
		for (const FVeyraShieldShare& Share : Resolution.Shields)
		{
			Cost += Share.Absorbed;
		}
		const FVeyraItemsTuning& Items = UVeyraItemsTuningSubsystem::Get();
		for (const FVeyraContentId& Attunement : Cost > 0.0 ? HeldBy(*Holder) : TArray<FVeyraContentId, TInlineAllocator<6>>())
		{
			if (Items.CleanBreak.Contains(Attunement))
			{
				TallyForCleanBreak(Attunement, Cost, *Holder, *Target, Now);
			}
		}
	}
	if (Resolution.Shields.IsEmpty())
	{
		return;
	}
	Brands.RemoveAllSwap([Now](const FBrand& Entry) { return Entry.Until <= Now || !Entry.Holder.IsValid() || !Entry.Target.IsValid(); });
	// Another source breaking a branded shield loses its Breach (Item Bible §8).
	for (const FVeyraShieldShare& Share : Resolution.Shields)
	{
		if (Share.bBroken)
		{
			Brands.RemoveAllSwap([Holder, Target, &Share](const FBrand& Entry) {
				return Entry.Target.Get() == Target && Entry.Provider.Get() == Share.Provider.Get() && Entry.Shield == Share.Id && Entry.Holder.Get() != Holder;
			});
		}
	}
	if (!VeyraUnits::IsVanguard(Target->GetOwner()) || !VeyraTargeting::AreHostile(Holder->GetOwner(), Target->GetOwner()))
	{
		return;
	}
	const FVeyraItemsTuning& Tuning = UVeyraItemsTuningSubsystem::Get();
	double Detonation = 0.0;
	for (const FVeyraContentId& Attunement : HeldBy(*Holder))
	{
		const FVeyraThroughTheGuardTuning* Guard = Tuning.ThroughTheGuard.Find(Attunement);
		if (!Guard)
		{
			continue;
		}
		for (const FVeyraShieldShare& Share : Resolution.Shields)
		{
			// Only a shield another Vanguard granted qualifies; a self-granted one never does.
			const UAbilitySystemComponent* Provider = Share.Provider.Get();
			if (!Provider || Provider == Target || !VeyraUnits::IsVanguard(Provider->GetOwner()))
			{
				continue;
			}
			FBrand* Brand = Brands.FindByPredicate([Holder, Target, Provider, &Share, &Attunement](const FBrand& Entry) {
				return Entry.Holder.Get() == Holder && Entry.Target.Get() == Target && Entry.Provider.Get() == Provider && Entry.Shield == Share.Id
					&& Entry.Attunement == Attunement;
			});
			if (!Brand)
			{
				Brand = &Brands.AddDefaulted_GetRef();
				Brand->Holder = Holder;
				Brand->Target = Target;
				Brand->Provider = Provider;
				Brand->Shield = Share.Id;
				Brand->Attunement = Attunement;
				Brand->Until = Now + Guard->BrandSeconds;
			}
			Brand->Breach += Guard->BreachShare * Share.Absorbed;
			// Broken by the holder in time, the Breach detonates.
			if (Share.bBroken)
			{
				Detonation += Guard->BreachConversion * Brand->Breach;
				Brands.RemoveAtSwap(static_cast<int32>(Brand - Brands.GetData()));
			}
		}
	}
	DealBonus(*Holder, *Target, EVeyraDamageType::Physical, Detonation);
}

void UVeyraAttunementSubsystem::NoOneComing(const FVeyraContentId& Attunement, const FVeyraDamageDealtEvent& Event, UAbilitySystemComponent& Holder,
	UAbilitySystemComponent& Target, double Now)
{
	const UWorld* World = GetWorld();
	if (Event.Delivery == EVeyraDamageDelivery::Proc || !World || !VeyraTargeting::IsAlive(Target.GetOwner()))
	{
		return;
	}
	const FVeyraNoOneComingTuning& Tuning = UVeyraItemsTuningSubsystem::Get().NoOneComing.FindChecked(Attunement);
	Abandons.RemoveAllSwap([Now](const FAbandoned& Entry) { return Entry.IsOver(Now) || !Entry.Holder.IsValid() || !Entry.Target.IsValid(); });
	const int32 Marked = Abandons.IndexOfByPredicate([&Holder, &Target, &Attunement](const FAbandoned& Entry) {
		return Entry.Holder.Get() == &Holder && Entry.Target.Get() == &Target && Entry.Attunement == Attunement;
	});
	if (Marked != INDEX_NONE)
	{
		FAbandoned& Mark = Abandons[Marked];
		// Locked in, the holder's next hit finishes it (Item Bible §8), and the chase ends with the mark.
		if (Mark.LockedUntil > Now)
		{
			Abandons.RemoveAtSwap(Marked);
			EndChaseUnlessMarked(Holder, Attunement, Now);
			DealBonus(Holder, Target, EVeyraDamageType::Physical, BonusHitOf(Tuning.Bonus, Holder));
			return;
		}
		Mark.Dealt += Event.Total();
		if (Mark.Dealt >= Tuning.LockDamage)
		{
			Mark.LockedUntil = Now + Tuning.LockSeconds;
		}
		return;
	}
	// A new mark needs the Vanguard alone.
	if (HasAllyNear(*World, Target, Tuning.ProtectionRadius))
	{
		return;
	}
	FAbandoned& Mark = Abandons.AddDefaulted_GetRef();
	Mark.Holder = &Holder;
	Mark.Target = &Target;
	Mark.Attunement = Attunement;
	Mark.Until = Now + Tuning.MarkSeconds;
	Mark.Dealt = Event.Total();
	if (Mark.Dealt >= Tuning.LockDamage)
	{
		Mark.LockedUntil = Now + Tuning.LockSeconds;
	}
	// The holder closes in faster while the mark lasts (ADR-051 §9.3).
	FVeyraStatusSpec Speed;
	Speed.Id = Attunement;
	Speed.Kind = EVeyraStatusKind::MoveSpeedTowardEnemyVanguards;
	Speed.Stacking = EVeyraStackingPolicy::UniqueRefresh;
	Speed.Magnitude = Tuning.Speed;
	Speed.DurationSeconds = Tuning.MarkSeconds;
	VeyraCombat::ApplyStatus(Holder, Holder, Speed);
}

void UVeyraAttunementSubsystem::UpdateAbandoned()
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const double Now = World->GetTimeSeconds();
	const FVeyraItemsTuning& Tuning = UVeyraItemsTuningSubsystem::Get();
	for (int32 Index = Abandons.Num() - 1; Index >= 0; --Index)
	{
		const FAbandoned& Mark = Abandons[Index];
		const UAbilitySystemComponent* Target = Mark.Target.Get();
		const FVeyraNoOneComingTuning* NoOneComing = Tuning.NoOneComing.Find(Mark.Attunement);
		// Before it locks in, an ally arriving breaks it; once locked, the finishing window stays.
		const bool bRescued = Target && NoOneComing && Mark.LockedUntil <= 0.0 && HasAllyNear(*World, *Target, NoOneComing->ProtectionRadius);
		if (!Target || !Mark.Holder.IsValid() || Mark.IsOver(Now) || bRescued)
		{
			UAbilitySystemComponent* Holder = const_cast<UAbilitySystemComponent*>(Mark.Holder.Get());
			const FVeyraContentId Attunement = Mark.Attunement;
			Abandons.RemoveAtSwap(Index);
			if (Holder)
			{
				EndChaseUnlessMarked(*Holder, Attunement, Now);
			}
		}
	}
}

void UVeyraAttunementSubsystem::EndChaseUnlessMarked(UAbilitySystemComponent& Holder, const FVeyraContentId& Attunement, double Now)
{
	const bool bMarked = Abandons.ContainsByPredicate([&Holder, &Attunement, Now](const FAbandoned& Entry) {
		return Entry.Holder.Get() == &Holder && Entry.Attunement == Attunement && !Entry.IsOver(Now);
	});
	if (!bMarked)
	{
		VeyraCombat::RemoveStatus(Holder, Attunement);
	}
}

bool UVeyraAttunementSubsystem::IsAbandoned(const UAbilitySystemComponent& Holder, const UAbilitySystemComponent& Target, bool* bOutLocked) const
{
	const UWorld* World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;
	const FAbandoned* Mark = Abandons.FindByPredicate([&Holder, &Target, Now](const FAbandoned& Entry) {
		return Entry.Holder.Get() == &Holder && Entry.Target.Get() == &Target && !Entry.IsOver(Now);
	});
	if (bOutLocked)
	{
		*bOutLocked = Mark && Mark->LockedUntil > Now;
	}
	return Mark != nullptr;
}

void UVeyraAttunementSubsystem::Reenactment(const FVeyraContentId& Attunement, const FVeyraDamageDealtEvent& Event, UAbilitySystemComponent& Holder,
	UAbilitySystemComponent& Target, double Now, TOptional<double> LastDealt)
{
	const AActor* Body = Holder.GetAvatarActor();
	if (Event.Delivery == EVeyraDamageDelivery::Proc || !Body || !VeyraTargeting::IsAlive(Target.GetOwner()))
	{
		return;
	}
	const FVeyraReenactmentTuning& Tuning = UVeyraItemsTuningSubsystem::Get().Reenactment.FindChecked(Attunement);
	Memories.RemoveAllSwap([Now](const FMemory& Entry) { return Entry.Until <= Now || !Entry.Holder.IsValid() || !Entry.Target.IsValid(); });
	const int32 Remembered = Memories.IndexOfByPredicate([&Holder, &Target, &Attunement](const FMemory& Entry) {
		return Entry.Holder.Get() == &Holder && Entry.Target.Get() == &Target && Entry.Attunement == Attunement;
	});
	if (Remembered != INDEX_NONE)
	{
		// Only after repositioning does the memory shatter into its replay (Item Bible §9).
		if (FVector::Dist2D(Body->GetActorLocation(), Memories[Remembered].Where) < Tuning.Displacement)
		{
			return;
		}
		const double Wound = Memories[Remembered].Wound;
		Memories.RemoveAtSwap(Remembered);
		DealBonus(Holder, Target, EVeyraDamageType::Magic, Tuning.ReplayShare * Wound);
		return;
	}
	// The first ability wound after a quiet period is remembered: what it dealt, and where the holder stood (ADR-051 §9.4).
	if (Event.Delivery != EVeyraDamageDelivery::Ability || (LastDealt.IsSet() && Now - LastDealt.GetValue() < Tuning.QuietSeconds))
	{
		return;
	}
	FMemory& Memory = Memories.AddDefaulted_GetRef();
	Memory.Holder = &Holder;
	Memory.Target = &Target;
	Memory.Attunement = Attunement;
	Memory.Until = Now + Tuning.MemorySeconds;
	Memory.Wound = Event.Total();
	Memory.Where = Body->GetActorLocation();
}
