// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Attunements/VeyraAttunementSubsystem.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Absorption/VeyraAbsorptionLedger.h"
#include "Attacks/VeyraBasicAttackComponent.h"
#include "Attributes/VeyraOffenceSet.h"
#include "Attributes/VeyraVitalsSet.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"
#include "Inventory/VeyraInventoryComponent.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Shapes/VeyraShapes.h"
#include "Shop/VeyraShopSubsystem.h"
#include "Statuses/VeyraStatusTypes.h"
#include "Targeting/VeyraTargeting.h"
#include "Quests/VeyraQuestRules.h"
#include "Tuning/VeyraCombatTuningSubsystem.h"
#include "Tuning/VeyraItemsTuningSubsystem.h"
#include "Units/VeyraUnit.h"
#include "VeyraCombatVerbs.h"
#include "VeyraItemsLog.h"

void UVeyraAttunementSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (UVeyraCombatEventSubsystem* Events = Collection.InitializeDependency<UVeyraCombatEventSubsystem>())
	{
		DamageDealtHandle = Events->OnDamageDealt.AddUObject(this, &UVeyraAttunementSubsystem::OnDamageDealt);
		DeathHandle = Events->OnDeath.AddUObject(this, &UVeyraAttunementSubsystem::OnDeath);
	}
}

void UVeyraAttunementSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	// Tempered by Conflict charges on the server, as often as its most frequent entry asks.
	double CheckSeconds = 0.0;
	for (const TPair<FVeyraContentId, FVeyraTemperedByConflictTuning>& Entry : UVeyraItemsTuningSubsystem::Get().TemperedByConflict)
	{
		CheckSeconds = CheckSeconds > 0.0 ? FMath::Min(CheckSeconds, Entry.Value.CheckSeconds) : Entry.Value.CheckSeconds;
	}
	if (InWorld.GetNetMode() != NM_Client && CheckSeconds > 0.0)
	{
		InWorld.GetTimerManager().SetTimer(TemperingTimer, FTimerDelegate::CreateUObject(this, &UVeyraAttunementSubsystem::UpdateTempering),
			static_cast<float>(CheckSeconds), /*bLoop*/ true);
	}
}

void UVeyraAttunementSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TemperingTimer);
		World->GetTimerManager().ClearTimer(CurrentTimer);
		if (UVeyraCombatEventSubsystem* Events = World->GetSubsystem<UVeyraCombatEventSubsystem>())
		{
			Events->OnDamageDealt.Remove(DamageDealtHandle);
			Events->OnDeath.Remove(DeathHandle);
		}
	}
	Super::Deinitialize();
}

void UVeyraAttunementSubsystem::OnDamageDealt(const FVeyraDamageDealtEvent& Event)
{
	UAbilitySystemComponent* Holder = Event.Source.Get();
	UAbilitySystemComponent* Target = Event.Target.Get();
	// The target's side: when a unit last took damage from an enemy Vanguard (ADR-025 §5).
	if (Holder && Target && Event.Total() > 0.0 && VeyraUnits::IsVanguard(Holder->GetOwner()) && VeyraTargeting::AreHostile(Holder->GetOwner(), Target->GetOwner()))
	{
		VanguardDamageTakenAt.Add(Target, GetWorld()->GetTimeSeconds());
	}
	const AActor* Participant = Holder ? Holder->GetOwner() : nullptr;
	const UVeyraInventoryComponent* Inventory = Participant ? Participant->FindComponentByClass<UVeyraInventoryComponent>() : nullptr;
	// Each acts on damage that reached an enemy; all but Endless Cleave only on an enemy Vanguard (Item Bible §8–§9).
	if (!Inventory || !Target || !Participant->HasAuthority() || Event.Total() <= 0.0)
	{
		return;
	}
	const bool bVanguard = VeyraUnits::IsVanguard(Target->GetOwner());
	const FVeyraItemsTuning& Tuning = UVeyraItemsTuningSubsystem::Get();
	TArray<FVeyraContentId, TInlineAllocator<6>> Held;
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
	const double Now = GetWorld()->GetTimeSeconds();
	const auto Over = [Now](const FTimed& Entry) { return Entry.Until <= Now || !Entry.Holder.IsValid(); };
	Cooldowns.RemoveAllSwap(Over);
	Primes.RemoveAllSwap(Over);
	// Held is this hit's own copy: a proc one of these deals sends an event of its own through here.
	for (const FVeyraContentId& Attunement : Held)
	{
		if (Tuning.EndlessCleave.Contains(Attunement))
		{
			EndlessCleave(Attunement, Event, *Holder, *Target);
		}
		else if (!bVanguard)
		{
			continue;
		}
		else if (Tuning.ReprisalGuard.Contains(Attunement))
		{
			ReprisalGuard(Attunement, Event, *Holder, Now);
		}
		else if (Tuning.Drag.Contains(Attunement))
		{
			Drag(Attunement, Event, *Holder, *Target);
		}
		else if (Tuning.Convergence.Contains(Attunement))
		{
			Convergence(Attunement, Event, *Holder, *Target, Now);
		}
		else if (Tuning.Fracture.Contains(Attunement))
		{
			Fracture(Attunement, Event, *Holder, *Target);
		}
		else if (Tuning.TemperedByConflict.Contains(Attunement))
		{
			TemperedByConflict(Attunement, Event, *Holder, *Target, Now);
		}
	}
}

TOptional<double> UVeyraAttunementSubsystem::GetVanguardDamageTakenAt(const UAbilitySystemComponent& Holder) const
{
	const double* At = VanguardDamageTakenAt.Find(&Holder);
	return At ? TOptional<double>(*At) : TOptional<double>();
}

void UVeyraAttunementSubsystem::OnDeath(const FVeyraDeathEvent& Death)
{
	AActor* Participant = VeyraQuests::LaneFluxbornLastHitter(Death);
	const UVeyraInventoryComponent* Inventory = Participant ? Participant->FindComponentByClass<UVeyraInventoryComponent>() : nullptr;
	UVeyraShopSubsystem* Shop = GetWorld()->GetSubsystem<UVeyraShopSubsystem>();
	if (!Inventory || !Shop || !Participant->HasAuthority())
	{
		return;
	}
	const FVeyraItemsTuning& Tuning = UVeyraItemsTuningSubsystem::Get();
	TArray<TPair<FVeyraContentId, double>, TInlineAllocator<2>> Stored;
	for (const FVeyraInventorySlot& Slot : Inventory->GetSlots())
	{
		const FVeyraItemDefinition* Item = Slot.IsEmpty() ? nullptr : Tuning.Items.Find(Slot.Item);
		if (!Item)
		{
			continue;
		}
		for (const FVeyraContentId& Attunement : Item->Attunement)
		{
			if (const FVeyraResidualCurrentTuning* Residual = Tuning.ResidualCurrent.Find(Attunement))
			{
				Stored.Emplace(Attunement, FMath::Min(Slot.Current + Residual->CurrentPerLastHit, Residual->CurrentCap));
			}
		}
	}
	for (const TPair<FVeyraContentId, double>& Entry : Stored)
	{
		Shop->SetCurrent(*Participant, Entry.Key, Entry.Value);
	}
	if (!Stored.IsEmpty() && !GetWorld()->GetTimerManager().IsTimerActive(CurrentTimer))
	{
		GetWorld()->GetTimerManager().SetTimer(CurrentTimer, FTimerDelegate::CreateUObject(this, &UVeyraAttunementSubsystem::UpdateCurrent),
			static_cast<float>(UVeyraCombatTuningSubsystem::Get().Regeneration.TickSeconds), /*bLoop*/ true);
	}
}

void UVeyraAttunementSubsystem::UpdateCurrent()
{
	UWorld* World = GetWorld();
	UVeyraShopSubsystem* Shop = World ? World->GetSubsystem<UVeyraShopSubsystem>() : nullptr;
	if (!Shop)
	{
		return;
	}
	const FVeyraItemsTuning& Tuning = UVeyraItemsTuningSubsystem::Get();
	const double Now = World->GetTimeSeconds();
	// Each call pays for the tick to come, and the amplification outlasts it until the next call renews it.
	const double TickSeconds = UVeyraCombatTuningSubsystem::Get().Regeneration.TickSeconds;
	constexpr double StatusTicks = 2.0;
	bool bAnyStored = false;
	TArray<APlayerState*, TInlineAllocator<10>> Participants;
	for (TActorIterator<APlayerState> It(World); It; ++It)
	{
		Participants.Add(*It);
	}
	for (APlayerState* Participant : Participants)
	{
		const UVeyraInventoryComponent* Inventory = Participant->FindComponentByClass<UVeyraInventoryComponent>();
		UAbilitySystemComponent* Holder = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Participant);
		if (!Inventory || !Holder)
		{
			continue;
		}
		TArray<TPair<FVeyraContentId, double>, TInlineAllocator<2>> Held;
		for (const FVeyraInventorySlot& Slot : Inventory->GetSlots())
		{
			const FVeyraItemDefinition* Item = Slot.IsEmpty() ? nullptr : Tuning.Items.Find(Slot.Item);
			if (!Item)
			{
				continue;
			}
			for (const FVeyraContentId& Attunement : Item->Attunement)
			{
				if (Tuning.ResidualCurrent.Contains(Attunement))
				{
					Held.Emplace(Attunement, Slot.Current);
				}
			}
		}
		const double* TakenAt = VanguardDamageTakenAt.Find(Holder);
		const bool bMissingHealth = Holder->GetNumericAttribute(UVeyraVitalsSet::GetHealthAttribute()) < Holder->GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute());
		for (const TPair<FVeyraContentId, double>& Entry : Held)
		{
			const FVeyraResidualCurrentTuning& Residual = Tuning.ResidualCurrent.FindChecked(Entry.Key);
			const bool bQuiet = !TakenAt || Now - *TakenAt >= Residual.QuietSeconds;
			// Spent only while it restores something: a holder at full Health keeps its Current (ADR-025 §8).
			if (bQuiet && bMissingHealth && Entry.Value > 0.0 && VeyraTargeting::IsAlive(Participant))
			{
				const double Left = Entry.Value - FMath::Min(Entry.Value, Residual.CurrentPerSecond * TickSeconds);
				Shop->SetCurrent(*Participant, Entry.Key, Left);
				FVeyraStatusSpec Amplified;
				Amplified.Id = Entry.Key;
				Amplified.Kind = EVeyraStatusKind::HealthRegeneration;
				Amplified.Magnitude = Residual.RegenerationAmplification - 1.0;
				Amplified.DurationSeconds = TickSeconds * StatusTicks;
				VeyraCombat::ApplyStatus(*Holder, *Holder, Amplified);
				bAnyStored |= Left > 0.0;
			}
			else
			{
				// Enemy-Vanguard damage, full Health or death suspends it; the Current keeps (Item Bible §10).
				VeyraCombat::RemoveStatus(*Holder, Entry.Key);
				bAnyStored |= Entry.Value > 0.0;
			}
		}
	}
	if (!bAnyStored)
	{
		World->GetTimerManager().ClearTimer(CurrentTimer);
	}
}

void UVeyraAttunementSubsystem::ReprisalGuard(const FVeyraContentId& Attunement, const FVeyraDamageDealtEvent& Event, UAbilitySystemComponent& Holder, double Now)
{
	// The triggering attack or ability (Item Bible §8): not a proc a hit spreads, nor a tick.
	const bool bTriggers = Event.Delivery == EVeyraDamageDelivery::BasicAttack || Event.Delivery == EVeyraDamageDelivery::Ability;
	const bool bCooling = Cooldowns.ContainsByPredicate([&Holder, &Attunement](const FTimed& Entry) {
		return Entry.Holder.Get() == &Holder && Entry.Attunement == Attunement;
	});
	if (!bTriggers || bCooling || !VeyraTargeting::IsAlive(Holder.GetOwner()))
	{
		return;
	}
	const FVeyraReprisalGuardTuning& Guard = UVeyraItemsTuningSubsystem::Get().ReprisalGuard.FindChecked(Attunement);
	FVeyraShieldGrant Grant;
	Grant.Id = Attunement;
	Grant.Amount = FMath::Min(Event.Total() * Guard.DamageFraction, Guard.MaxShield);
	Grant.MaxAmount = Guard.MaxShield;
	Grant.DurationSeconds = Guard.ShieldSeconds;
	if (!VeyraCombat::GrantShield(Holder, Holder, Grant).IsValid())
	{
		return;
	}
	// Then the Attunement cools down.
	FTimed& Cooling = Cooldowns.AddDefaulted_GetRef();
	Cooling.Holder = &Holder;
	Cooling.Attunement = Attunement;
	Cooling.Until = Now + Guard.CooldownSeconds;
	UE_LOG(LogVeyraItems, Verbose, TEXT("%s's %s shields it for %g."), *GetNameSafe(Holder.GetOwner()), *Attunement.ToString(), Grant.Amount);
}

void UVeyraAttunementSubsystem::Drag(const FVeyraContentId& Attunement, const FVeyraDamageDealtEvent& Event, UAbilitySystemComponent& Holder,
	UAbilitySystemComponent& Target)
{
	// Damaging abilities briefly slow (Item Bible §9), their damage over time too, as League's Rylai's
	// Crystal Scepter (ADR-023 §9).
	const bool bAbilityDamage = Event.Delivery == EVeyraDamageDelivery::Ability || Event.Delivery == EVeyraDamageDelivery::Periodic;
	if (!bAbilityDamage || !VeyraTargeting::IsAlive(Target.GetOwner()))
	{
		return;
	}
	const FVeyraDragTuning& Tuning = UVeyraItemsTuningSubsystem::Get().Drag.FindChecked(Attunement);
	FVeyraStatusSpec Slow;
	Slow.Id = Attunement;
	Slow.Kind = EVeyraStatusKind::Slow;
	Slow.Stacking = EVeyraStackingPolicy::UniqueRefresh;
	Slow.Magnitude = Tuning.Slow;
	Slow.DurationSeconds = Tuning.DurationSeconds;
	VeyraCombat::ApplyStatus(Holder, Target, Slow);
}

void UVeyraAttunementSubsystem::Convergence(const FVeyraContentId& Attunement, const FVeyraDamageDealtEvent& Event, UAbilitySystemComponent& Holder,
	UAbilitySystemComponent& Target, double Now)
{
	// One damaging ability primes the target; the holder's next one on it within the window consumes
	// the prime for bonus magic damage (Item Bible §9).
	if (Event.Delivery != EVeyraDamageDelivery::Ability || !VeyraTargeting::IsAlive(Target.GetOwner()))
	{
		return;
	}
	const FVeyraConvergenceTuning& Tuning = UVeyraItemsTuningSubsystem::Get().Convergence.FindChecked(Attunement);
	const int32 Primed = Primes.IndexOfByPredicate([&Holder, &Target, &Attunement](const FTimed& Entry) {
		return Entry.Holder.Get() == &Holder && Entry.Target.Get() == &Target && Entry.Attunement == Attunement;
	});
	if (Primed == INDEX_NONE)
	{
		FTimed& Prime = Primes.AddDefaulted_GetRef();
		Prime.Holder = &Holder;
		Prime.Target = &Target;
		Prime.Attunement = Attunement;
		Prime.Until = Now + Tuning.WindowSeconds;
		return;
	}
	Primes.RemoveAtSwap(Primed);
	// A proc, which neither primes nor consumes (ADR-023 §3).
	FVeyraRawDamageEvent Bonus;
	Bonus.Components.Add({ EVeyraDamageType::Magic, Tuning.BaseDamage + Tuning.MagicPowerRatio * Holder.GetNumericAttribute(UVeyraOffenceSet::GetMagicPowerAttribute()) });
	Bonus.Delivery = EVeyraDamageDelivery::Proc;
	VeyraCombat::DealDamage(Holder, Target, Bonus);
}

void UVeyraAttunementSubsystem::Fracture(const FVeyraContentId& Attunement, const FVeyraDamageDealtEvent& Event, UAbilitySystemComponent& Holder,
	UAbilitySystemComponent& Target)
{
	// Repeated magic damage progressively reduces Magic Resistance, up to a cap (Item Bible §9): each
	// instance with some adds a stack, and the stacks refresh and end together.
	if (Event.Of(EVeyraDamageType::Magic) <= 0.0 || !VeyraTargeting::IsAlive(Target.GetOwner()))
	{
		return;
	}
	const FVeyraStackingAttunementTuning& Tuning = UVeyraItemsTuningSubsystem::Get().Fracture.FindChecked(Attunement);
	FVeyraStatusSpec Shred;
	Shred.Id = Attunement;
	Shred.Kind = EVeyraStatusKind::MagicResistReduction;
	Shred.Stacking = EVeyraStackingPolicy::Stacking;
	Shred.Magnitude = Tuning.PerStack;
	Shred.MaxStacks = Tuning.MaxStacks;
	Shred.DurationSeconds = Tuning.DurationSeconds;
	VeyraCombat::ApplyStatus(Holder, Target, Shred);
}

void UVeyraAttunementSubsystem::EndlessCleave(const FVeyraContentId& Attunement, const FVeyraDamageDealtEvent& Event, UAbilitySystemComponent& Holder,
	UAbilitySystemComponent& Target)
{
	// Every basic attack, on its primary target: an attack's own cleaves and impacts are procs, and a
	// structure is never cleaved around (Item Bible §8; Combat Bible §33).
	const AActor* Struck = Target.GetAvatarActor();
	// A participant's basic attack is its own, not its body's: it outlives the body.
	const AActor* Attacker = Holder.GetOwner();
	const UVeyraBasicAttackComponent* Attacks = Attacker ? Attacker->FindComponentByClass<UVeyraBasicAttackComponent>() : nullptr;
	if (Event.Delivery != EVeyraDamageDelivery::BasicAttack || !Struck || !Attacks || VeyraUnits::IsStructure(Struck))
	{
		return;
	}
	// A share of the attack's base damage as Physical damage, a smaller share from a ranged holder (ADR-023 §3).
	const FVeyraEndlessCleaveTuning& Tuning = UVeyraItemsTuningSubsystem::Get().EndlessCleave.FindChecked(Attunement);
	const FVeyraBasicAttackProfile& Profile = Attacks->GetProfile();
	const double Share = Profile.Projectile.IsEmpty() ? Tuning.MeleeFraction : Tuning.RangedFraction;
	const double Base = Holder.GetNumericAttribute(UVeyraOffenceSet::GetPhysicalPowerAttribute()) * Profile.PhysicalPowerRatio
		+ Holder.GetNumericAttribute(UVeyraOffenceSet::GetMagicPowerAttribute()) * Profile.MagicPowerRatio;
	if (Base * Share <= 0.0)
	{
		return;
	}
	FVeyraRawDamageEvent Splash;
	Splash.Components.Add({ EVeyraDamageType::Physical, Base * Share });
	Splash.Delivery = EVeyraDamageDelivery::Proc;
	const FVeyraPreparedDamage Prepared = VeyraCombat::PrepareDamage(Holder, Splash);
	FVeyraShape Around;
	Around.Kind = EVeyraShapeKind::Circle;
	Around.Radius = Tuning.Radius;
	// Sides belong to the participant, which outlives its body.
	const AActor* Side = Holder.GetOwner();
	const TArray<AActor*> Units = VeyraShapes::GatherUnits(*GetWorld(), FVeyraPlacedShape{ Around, Struck->GetActorLocation(), FVector::ForwardVector },
		[Side, Struck](const AActor& Unit) { return &Unit != Struck && VeyraTargeting::AreHostile(Side, &Unit); });
	for (AActor* Unit : Units)
	{
		if (UAbilitySystemComponent* Other = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Unit))
		{
			VeyraCombat::DealPreparedDamage(Prepared, *Other);
		}
	}
}

void UVeyraAttunementSubsystem::UpdateTempering()
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const FVeyraItemsTuning& Tuning = UVeyraItemsTuningSubsystem::Get();
	const double Now = World->GetTimeSeconds();
	Tempering.RemoveAllSwap([](const FTempering& Entry) { return !Entry.Holder.IsValid() || !Entry.Target.IsValid(); });
	TArray<APlayerState*, TInlineAllocator<10>> Participants;
	for (TActorIterator<APlayerState> It(World); It; ++It)
	{
		Participants.Add(*It);
	}
	for (APlayerState* Participant : Participants)
	{
		const UVeyraInventoryComponent* Inventory = Participant->FindComponentByClass<UVeyraInventoryComponent>();
		UAbilitySystemComponent* Holder = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Participant);
		if (!Inventory || !Holder)
		{
			continue;
		}
		for (const FVeyraInventorySlot& Slot : Inventory->GetSlots())
		{
			const FVeyraItemDefinition* Item = Slot.IsEmpty() ? nullptr : Tuning.Items.Find(Slot.Item);
			if (!Item)
			{
				continue;
			}
			for (const FVeyraContentId& Attunement : Item->Attunement)
			{
				const FVeyraTemperedByConflictTuning* Charge = Tuning.TemperedByConflict.Find(Attunement);
				if (!Charge)
				{
					continue;
				}
				const AActor* Body = Holder->GetAvatarActor();
				const bool bHolderStands = Body && VeyraTargeting::IsAlive(Participant);
				for (APlayerState* Enemy : Participants)
				{
					UAbilitySystemComponent* Other = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Enemy);
					if (!Other || !VeyraTargeting::AreHostile(Participant, Enemy))
					{
						continue;
					}
					FTempering* Entry = Tempering.FindByPredicate([Holder, Other, &Attunement](const FTempering& Each) {
						return Each.Holder.Get() == Holder && Each.Target.Get() == Other && Each.Attunement == Attunement;
					});
					if (!Entry)
					{
						Entry = &Tempering.AddDefaulted_GetRef();
						Entry->Holder = Holder;
						Entry->Target = Other;
						Entry->Attunement = Attunement;
					}
					// It charges while the enemy stays within the radius, and not while that enemy's cooldown runs.
					const AActor* EnemyBody = Other->GetAvatarActor();
					const bool bNear = bHolderStands && EnemyBody && VeyraTargeting::IsAlive(Enemy)
						&& FVector::Dist2D(Body->GetActorLocation(), EnemyBody->GetActorLocation()) <= Charge->Radius;
					if (!bNear || Now < Entry->ReadyAt)
					{
						Entry->NearSince = -1.0;
						Entry->bTempered = false;
						continue;
					}
					if (Entry->NearSince < 0.0)
					{
						Entry->NearSince = Now;
					}
					Entry->bTempered |= Now - Entry->NearSince >= Charge->ChargeSeconds;
				}
			}
		}
	}
}

bool UVeyraAttunementSubsystem::IsTempered(const UAbilitySystemComponent& Holder, const UAbilitySystemComponent& Target) const
{
	return Tempering.ContainsByPredicate([&Holder, &Target](const FTempering& Each) {
		return Each.bTempered && Each.Holder.Get() == &Holder && Each.Target.Get() == &Target;
	});
}

void UVeyraAttunementSubsystem::TemperedByConflict(const FVeyraContentId& Attunement, const FVeyraDamageDealtEvent& Event, UAbilitySystemComponent& Holder,
	UAbilitySystemComponent& Target, double Now)
{
	// The holder's next basic attack on a Tempered enemy consumes it (Item Bible §8).
	FTempering* Entry = Tempering.FindByPredicate([&Holder, &Target, &Attunement](const FTempering& Each) {
		return Each.bTempered && Each.Holder.Get() == &Holder && Each.Target.Get() == &Target && Each.Attunement == Attunement;
	});
	AActor* Participant = Holder.GetOwner();
	if (Event.Delivery != EVeyraDamageDelivery::BasicAttack || !Entry || !Participant || !VeyraTargeting::IsAlive(Target.GetOwner()))
	{
		return;
	}
	const FVeyraTemperedByConflictTuning& Tuning = UVeyraItemsTuningSubsystem::Get().TemperedByConflict.FindChecked(Attunement);
	Entry->bTempered = false;
	Entry->NearSince = -1.0;
	Entry->ReadyAt = Now + Tuning.CooldownSeconds;
	// Bonus Physical damage, as a proc, from the holder's Max Health; a share of it becomes permanent Max
	// Health on the item, which leaves with it (ADR-023 §3).
	const double Bonus = Tuning.BaseDamage + Tuning.MaxHealthFraction * Holder.GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute());
	FVeyraRawDamageEvent Damage;
	Damage.Components.Add({ EVeyraDamageType::Physical, Bonus });
	Damage.Delivery = EVeyraDamageDelivery::Proc;
	VeyraCombat::DealDamage(Holder, Target, Damage);
	if (UVeyraShopSubsystem* Shop = GetWorld()->GetSubsystem<UVeyraShopSubsystem>())
	{
		Shop->GrowHealth(*Participant, Attunement, Bonus * Tuning.HealthGainFraction);
	}
}
