// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Delivery/VeyraEffectDelivery.h"

#include "Statuses/VeyraStatusComponent.h"
#include "Units/VeyraUnit.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Attributes/VeyraOffenceSet.h"
#include "Abilities/VeyraGameplayAbility.h"
#include "Attributes/VeyraVitalsSet.h"
#include "Delivery/VeyraShieldRewardSubsystem.h"
#include "Engine/World.h"
#include "Targeting/VeyraTargeting.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "VeyraCombatVerbs.h"

namespace VeyraEffectDelivery
{
namespace
{
	/** The displacement Tuning gives Unit, measured from Frame. */
	TOptional<FVeyraDisplacement> DisplacementFor(const FVeyraDisplacementTuning& Tuning, const AActor& Unit, const FVeyraEffectFrame& Frame, double CasterRadius,
		int32 SourceLevel)
	{
		const FVector FromOrigin = (Unit.GetActorLocation() - Frame.Origin).GetSafeNormal2D();
		const FVector Facing = Frame.Direction.GetSafeNormal2D();
		// On the ground, with X forward and Y to the right, the right of (x, y) is (-y, x).
		const FVector Right(-Facing.Y, Facing.X, 0.0);
		FVeyraDisplacement Displacement{ FVector::ZeroVector, Tuning.Distance, Tuning.Speed };
		Displacement.CollisionStatuses = StatusSpecs(Tuning.CollisionStatuses, SourceLevel);
		switch (Tuning.Direction)
		{
		case EVeyraDisplacementDirection::TowardOrigin:
		{
			// A Pull stops at the origin, or at the caster's edge when the caster is the origin.
			const double Gap = FVector::Dist2D(Unit.GetActorLocation(), Frame.Origin) - (Frame.bOriginIsCaster ? Unit.GetSimpleCollisionRadius() + CasterRadius : 0.0);
			Displacement.Direction = -FromOrigin;
			Displacement.Distance = FMath::Min(Tuning.Distance, Gap);
			break;
		}
		case EVeyraDisplacementDirection::AwayFromOrigin:
			Displacement.Direction = FromOrigin.IsNearlyZero() ? Facing : FromOrigin;
			break;
		case EVeyraDisplacementDirection::AcrossCastLeft:
			Displacement.Direction = -Right;
			break;
		case EVeyraDisplacementDirection::AcrossCastRight:
			Displacement.Direction = Right;
			break;
		case EVeyraDisplacementDirection::AsideFromPath:
		{
			// To whichever side of the path's line the unit is on; straight ahead of it goes right.
			const double Side = FVector::DotProduct(Unit.GetActorLocation() - Frame.Origin, Right);
			Displacement.Direction = Side < 0.0 ? -Right : Right;
			break;
		}
		}
		if (Displacement.Direction.IsNearlyZero() || !(Displacement.Distance > 0.0))
		{
			return {};
		}
		return Displacement;
	}
}

double DamageAmount(const UAbilitySystemComponent& Caster, const FVeyraDamageTuning& Damage, int32 Rank)
{
	// The formula an ability's numbers show too (ADR-065 §7).
	return VeyraAbilityRules::DamageAmount(Damage, Rank, Caster.GetNumericAttribute(UVeyraOffenceSet::GetPhysicalPowerAttribute()),
		Caster.GetNumericAttribute(UVeyraOffenceSet::GetMagicPowerAttribute()));
}

TArray<FVeyraStatusSpec> StatusSpecs(TConstArrayView<FVeyraContentId> Ids, int32 SourceLevel)
{
	TArray<FVeyraStatusSpec> Specs;
	for (const FVeyraContentId& StatusId : Ids)
	{
		if (const TOptional<FVeyraStatusSpec> Status = UVeyraAbilitiesTuningSubsystem::FindStatus(StatusId, SourceLevel))
		{
			Specs.Add(Status.GetValue());
		}
	}
	return Specs;
}

FVeyraPreparedEffects Prepare(UAbilitySystemComponent& Caster, const FVeyraEffectBundleTuning& Effects, int32 Rank)
{
	FVeyraPreparedEffects Prepared;
	// Level-scaled statuses read the caster's Level at Commit, as the rest of the hit does (Combat Bible §50).
	const int32 Level = UVeyraGameplayAbility::GetCasterLevel(Caster);
	if (!Effects.Damage.IsEmpty())
	{
		FVeyraRawDamageEvent Raw;
		for (const FVeyraDamageTuning& Damage : Effects.Damage)
		{
			Raw.Components.Add({ Damage.Type, DamageAmount(Caster, Damage, Rank) });
		}
		Prepared.Damage = VeyraCombat::PrepareDamage(Caster, Raw);
		Prepared.RawDamage = Raw.Components;
	}
	Prepared.UnitKindMultipliers = Effects.UnitKindMultipliers;
	Prepared.DisplacementUnlessStatuses = Effects.DisplacementUnlessStatuses;
	Prepared.Statuses = StatusSpecs(Effects.Statuses, Level);
	if (!Effects.Displacement.IsEmpty())
	{
		Prepared.Displacement = Effects.Displacement[0];
	}
	if (!Effects.MissingHealthDamage.IsEmpty())
	{
		Prepared.MissingHealthDamage = Effects.MissingHealthDamage[0];
	}
	for (const FVeyraReactionTuning& Reaction : Effects.Reactions)
	{
		FVeyraPreparedReaction& Ready = Prepared.Reactions.AddDefaulted_GetRef();
		Ready.Status = Reaction.Status;
		Ready.bConsume = Reaction.Consume == EVeyraReactionConsume::Consume;
		Ready.bPerStack = Reaction.Scaling == EVeyraReactionScaling::PerStack;
		for (const FVeyraDamageTuning& Damage : Reaction.Damage)
		{
			Ready.Damage.Add({ Damage.Type, DamageAmount(Caster, Damage, Rank) });
		}
		Ready.Statuses = StatusSpecs(Reaction.Statuses, Level);
		Ready.Replaces = Reaction.Replaces;
		for (const FVeyraReactionBurstTuning& Burst : Reaction.Burst)
		{
			FVeyraPreparedBurst& Around = Ready.Burst.AddDefaulted_GetRef();
			Around.Shape = Burst.Shape;
			Around.Statuses = StatusSpecs(Burst.Statuses, Level);
			if (!Burst.Damage.IsEmpty())
			{
				FVeyraRawDamageEvent Raw;
				for (const FVeyraDamageTuning& Damage : Burst.Damage)
				{
					Raw.Components.Add({ Damage.Type, DamageAmount(Caster, Damage, Rank) });
				}
				Around.Damage = VeyraCombat::PrepareDamage(Caster, Raw);
			}
		}
	}
	// A bundle whose only damage is its reactions' prepares that damage now too, so it keeps the caster's
	// offence at Commit (Combat Bible §50): each reaction type at 0, its amounts joining at impact.
	if (!Prepared.Damage.IsValid())
	{
		FVeyraRawDamageEvent Raw;
		for (const FVeyraPreparedReaction& Ready : Prepared.Reactions)
		{
			for (const FVeyraDamageComponent& Component : Ready.Damage)
			{
				if (!Raw.Components.ContainsByPredicate([&Component](const FVeyraDamageComponent& Each) { return Each.Type == Component.Type; }))
				{
					Raw.Components.Add({ Component.Type, 0.0 });
				}
			}
		}
		if (!Raw.Components.IsEmpty())
		{
			Prepared.ReactionDamage = VeyraCombat::PrepareDamage(Caster, Raw);
		}
	}
	return Prepared;
}

FActiveGameplayEffectHandle GrantShield(UAbilitySystemComponent& Caster, UAbilitySystemComponent& Holder, const FVeyraShieldTuning& Shield, int32 Rank)
{
	const FVeyraShieldGrant Grant = ShieldGrant(Caster, Shield, Rank);
	const FActiveGameplayEffectHandle Granted = VeyraCombat::GrantShield(Caster, Holder, Grant);
	if (!Granted.IsValid())
	{
		return Granted;
	}
	UWorld* World = Holder.GetWorld();
	UVeyraShieldRewardSubsystem* Rewards = World ? World->GetSubsystem<UVeyraShieldRewardSubsystem>() : nullptr;
	for (const FVeyraAbsorbedRewardTuning& Reward : Shield.AbsorbedReward)
	{
		if (Rewards)
		{
			Rewards->Watch(Caster, Holder, Grant, Reward);
		}
	}
	return Granted;
}

FVeyraSecondaryImpact SecondaryImpact(const UAbilitySystemComponent& Caster, const FVeyraSecondaryImpactTuning& Impact, int32 Rank)
{
	FVeyraSecondaryImpact Ready;
	Ready.Priority = Impact.Priority;
	Ready.Shape = Impact.Shape;
	for (const FVeyraDamageTuning& Damage : Impact.Damage)
	{
		Ready.Damage.Components.Add({ Damage.Type, DamageAmount(Caster, Damage, Rank) });
	}
	Ready.Statuses = StatusSpecs(Impact.Statuses, UVeyraGameplayAbility::GetCasterLevel(Caster));
	return Ready;
}

bool IsEmpty(const FVeyraPreparedEffects& Effects)
{
	return !Effects.Damage.IsValid() && Effects.Statuses.IsEmpty() && !Effects.Displacement.IsSet() && Effects.Reactions.IsEmpty();
}

bool WouldLandOn(const FVeyraPreparedEffects& Effects, const UAbilitySystemComponent& Target)
{
	if (Effects.Damage.IsValid() || !Effects.Statuses.IsEmpty() || Effects.Displacement.IsSet())
	{
		return true;
	}
	// Reactions only: something lands only where the unit holds a status one of them reacts to (ADR-026 §1).
	const UVeyraStatusComponent* Ledger = Target.GetOwner() ? Target.GetOwner()->FindComponentByClass<UVeyraStatusComponent>() : nullptr;
	return Ledger && Effects.Reactions.ContainsByPredicate([Ledger](const FVeyraPreparedReaction& Reaction) {
		return Ledger->GetLedger().Entries.ContainsByPredicate([&Reaction](const FVeyraStatusEntry& Entry) { return Entry.Id == Reaction.Status; });
	});
}

FVeyraShieldGrant ShieldGrant(const UAbilitySystemComponent& Caster, const FVeyraShieldTuning& Shield, int32 Rank)
{
	const double MaxHealth = Caster.GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute());
	FVeyraShieldGrant Grant;
	Grant.Id = Shield.Id;
	Grant.Category = Shield.Category;
	Grant.Amount = VeyraAbilityRules::ValueAtRank(Shield.AmountByRank, Rank) + MaxHealth * Shield.MaxHealthRatio
		+ Caster.GetNumericAttribute(UVeyraOffenceSet::GetMagicPowerAttribute()) * Shield.MagicPowerRatio;
	Grant.DurationSeconds = Shield.DurationSeconds;
	Grant.Reapply = Shield.Reapply;
	Grant.MaxAmount = FMath::Max(Grant.Amount, MaxHealth * Shield.MaxAmountMaxHealthRatio);
	if (!Shield.CapGroup.IsEmpty())
	{
		Grant.CapGroup = Shield.CapGroup[0].Id;
		Grant.CapGroupTotal = MaxHealth * Shield.CapGroup[0].TotalMaxHealthRatio;
	}
	return Grant;
}

void Apply(UAbilitySystemComponent& Caster, AActor& Unit, const FVeyraPreparedEffects& Effects, const FVeyraEffectFrame& Frame,
	const FVeyraAbilityHitSource& Source)
{
	UAbilitySystemComponent* Target = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Unit);
	// A Spell Shield blocks the whole hit: no damage, status or displacement, and no hit (Combat Bible §19).
	// A hit that would do nothing to this unit leaves it be, as reactions to statuses it does not hold.
	if (!Target || (!Source.bSkipSpellShield && WouldLandOn(Effects, *Target) && VeyraCombat::BlockAbilityHit(*Target, Caster)))
	{
		return;
	}
	FVeyraAbilityHit Hit;
	Hit.Caster = &Caster;
	Hit.Target = &Unit;
	Hit.Ability = Source.Ability;
	Hit.CastId = Source.CastId;
	Hit.bCasterShielded = Source.bCasterShielded;
	// Reactions read the statuses the target held as the hit landed; what they consume goes first, so
	// the hit's own statuses land afresh (ADR-026 §1).
	FVeyraDamageComponents ReactionDamage;
	TArray<FVeyraStatusSpec> ReactionStatuses;
	TArray<FVeyraContentId> Replaced;
	TArray<const FVeyraPreparedBurst*, TInlineAllocator<1>> Bursts;
	const UVeyraStatusComponent* Ledger = Target->GetOwner() ? Target->GetOwner()->FindComponentByClass<UVeyraStatusComponent>() : nullptr;
	if (Ledger)
	{
		for (const FVeyraStatusEntry& Entry : Ledger->GetLedger().Entries)
		{
			if (!Hit.HeldFromCaster.Contains(Entry.Id) && Ledger->HasFrom(Entry.Id, Caster))
			{
				Hit.HeldFromCaster.Add(Entry.Id);
			}
		}
	}
	if (Ledger && !Effects.Reactions.IsEmpty())
	{
		TArray<FVeyraContentId, TInlineAllocator<2>> Consumed;
		for (const FVeyraPreparedReaction& Reaction : Effects.Reactions)
		{
			int32 Stacks = 0;
			for (const FVeyraStatusEntry& Entry : Ledger->GetLedger().Entries)
			{
				Stacks += Entry.Id == Reaction.Status ? Entry.Stacks : 0;
			}
			if (Stacks == 0)
			{
				continue;
			}
			for (const FVeyraDamageComponent& Component : Reaction.Damage)
			{
				ReactionDamage.Add({ Component.Type, Component.Amount * (Reaction.bPerStack ? Stacks : 1) });
			}
			ReactionStatuses.Append(Reaction.Statuses);
			Replaced.Append(Reaction.Replaces);
			for (const FVeyraPreparedBurst& Burst : Reaction.Burst)
			{
				Bursts.Add(&Burst);
			}
			if (Reaction.bConsume)
			{
				Consumed.AddUnique(Reaction.Status);
			}
		}
		for (const FVeyraContentId& Status : Consumed)
		{
			VeyraCombat::RemoveStatus(*Target, Status);
		}
	}
	Hit.bDamaging = Effects.Damage.IsValid() || !ReactionDamage.IsEmpty();
	if (!Effects.Damage.IsValid() && !ReactionDamage.IsEmpty())
	{
		if (Effects.ReactionDamage.IsValid())
		{
			FVeyraPreparedDamage Reaction = Effects.ReactionDamage;
			Reaction.ProjectileFrom = Source.ProjectileFrom;
			VeyraCombat::DealPreparedDamage(Reaction, *Target, ReactionDamage);
		}
		else
		{
			// Effects prepared by hand, without a bundle: the caster's offence now.
			FVeyraRawDamageEvent Raw;
			Raw.Components = ReactionDamage;
			Raw.ProjectileFrom = Source.ProjectileFrom;
			VeyraCombat::DealDamage(Caster, *Target, Raw);
		}
	}
	if (Effects.Damage.IsValid())
	{
		// The target's own values join the hit as it lands (Combat Bible §50), and its reactions' damage.
		TArray<FVeyraDamageComponent, TInlineAllocator<1>> AddedAtImpact(ReactionDamage);
		if (Effects.MissingHealthDamage.IsSet())
		{
			const FVeyraMissingHealthDamageTuning& Missing = Effects.MissingHealthDamage.GetValue();
			AddedAtImpact.Add({ Missing.Type, Missing.MissingHealthRatio * VeyraCombat::GetMissingHealth(*Target) });
		}
		// More against some kinds of unit: the extra share of each prepared component joins the hit (ADR-018 §6).
		const TOptional<EVeyraUnitKind> Kind = VeyraUnits::KindOf(&Unit);
		const FVeyraUnitKindMultiplierTuning* Multiplier = Kind.IsSet()
			? Effects.UnitKindMultipliers.FindByPredicate([&Kind](const FVeyraUnitKindMultiplierTuning& Entry) { return Entry.Kind == Kind.GetValue(); })
			: nullptr;
		if (Multiplier && Multiplier->Multiplier > 1.0)
		{
			for (const FVeyraDamageComponent& Component : Effects.RawDamage)
			{
				AddedAtImpact.Add({ Component.Type, Component.Amount * (Multiplier->Multiplier - 1.0) });
			}
		}
		// The copy shares the prepared spec; only whether a projectile carries it differs (ADR-037 §4).
		FVeyraPreparedDamage Damage = Effects.Damage;
		Damage.ProjectileFrom = Source.ProjectileFrom;
		VeyraCombat::DealPreparedDamage(Damage, *Target, AddedAtImpact);
	}
	for (const FVeyraStatusSpec& Status : Effects.Statuses)
	{
		if (Replaced.Contains(Status.Id))
		{
			continue;
		}
		const bool bApplied = VeyraCombat::ApplyStatus(Caster, *Target, Status);
		Hit.bStunned |= bApplied && Status.Kind == EVeyraStatusKind::Stun;
	}
	for (const FVeyraStatusSpec& Status : ReactionStatuses)
	{
		const bool bApplied = VeyraCombat::ApplyStatus(Caster, *Target, Status);
		Hit.bStunned |= bApplied && Status.Kind == EVeyraStatusKind::Stun;
	}
	// One displacement per target per cast: a unit carrying a sparing status is not moved again (Roster Bible §1).
	const UVeyraStatusComponent* Held = Target->GetOwner() ? Target->GetOwner()->FindComponentByClass<UVeyraStatusComponent>() : nullptr;
	const bool bSpared = Held && Effects.DisplacementUnlessStatuses.ContainsByPredicate([Held](const FVeyraContentId& Id) {
		return Held->GetLedger().Entries.ContainsByPredicate([&Id](const FVeyraStatusEntry& Entry) { return Entry.Id == Id; });
	});
	if (Effects.Displacement.IsSet() && !bSpared)
	{
		const AActor* CasterBody = Caster.GetAvatarActor();
		const double CasterRadius = CasterBody ? CasterBody->GetSimpleCollisionRadius() : 0.0;
		if (const TOptional<FVeyraDisplacement> Displacement = DisplacementFor(Effects.Displacement.GetValue(), Unit, Frame, CasterRadius,
				UVeyraGameplayAbility::GetCasterLevel(Caster)))
		{
			Hit.bDisplaced = VeyraCombat::Displace(Caster, *Target, Displacement.GetValue());
		}
	}
	// A reaction's burst reaches the caster's other enemies around the target, as a secondary impact does,
	// never re-entering the hit pipeline (ADR-034 §6).
	const AActor* CasterBody = Caster.GetAvatarActor();
	UWorld* World = Unit.GetWorld();
	for (const FVeyraPreparedBurst* Burst : Bursts)
	{
		if (!CasterBody || !World)
		{
			break;
		}
		const FVector Away = (Unit.GetActorLocation() - CasterBody->GetActorLocation()).GetSafeNormal2D();
		const TArray<AActor*> Around = VeyraShapes::GatherUnits(*World, FVeyraPlacedShape{ Burst->Shape, Unit.GetActorLocation(), Away.IsNearlyZero() ? FVector::ForwardVector : Away },
			[CasterBody, &Unit](const AActor& Other) { return &Other != &Unit && VeyraTargeting::CanHitEnemy(CasterBody, Other) && VeyraTargeting::IsAlive(&Other); });
		for (AActor* Other : Around)
		{
			UAbilitySystemComponent* OtherAbilities = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Other);
			// A burst is an ability's hit on each unit it reaches: a Spell Shield blocks it whole, once.
			if (!OtherAbilities || VeyraCombat::BlockAbilityHit(*OtherAbilities, Caster))
			{
				continue;
			}
			if (Burst->Damage.IsValid())
			{
				VeyraCombat::DealPreparedDamage(Burst->Damage, *OtherAbilities);
			}
			for (const FVeyraStatusSpec& Status : Burst->Statuses)
			{
				VeyraCombat::ApplyStatus(Caster, *OtherAbilities, Status);
			}
		}
	}
	if (Source.Ability.IsValid())
	{
		UVeyraAbilityEventSubsystem::Announce(Unit.GetWorld(), Hit);
	}
}
}
