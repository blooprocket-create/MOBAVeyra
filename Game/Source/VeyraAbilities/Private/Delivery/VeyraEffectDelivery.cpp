// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Delivery/VeyraEffectDelivery.h"

#include "Statuses/VeyraStatusComponent.h"
#include "Units/VeyraUnit.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Attributes/VeyraOffenceSet.h"
#include "Attributes/VeyraVitalsSet.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"

namespace VeyraEffectDelivery
{
namespace
{
	/** The displacement Tuning gives Unit, measured from Frame. */
	TOptional<FVeyraDisplacement> DisplacementFor(const FVeyraDisplacementTuning& Tuning, const AActor& Unit, const FVeyraEffectFrame& Frame, double CasterRadius)
	{
		const FVector FromOrigin = (Unit.GetActorLocation() - Frame.Origin).GetSafeNormal2D();
		const FVector Facing = Frame.Direction.GetSafeNormal2D();
		// On the ground, with X forward and Y to the right, the right of (x, y) is (-y, x).
		const FVector Right(-Facing.Y, Facing.X, 0.0);
		FVeyraDisplacement Displacement{ FVector::ZeroVector, Tuning.Distance, Tuning.Speed };
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
	return VeyraAbilityRules::ValueAtRank(Damage.AmountByRank, Rank)
		+ Caster.GetNumericAttribute(UVeyraOffenceSet::GetPhysicalPowerAttribute()) * Damage.PhysicalPowerRatio
		+ Caster.GetNumericAttribute(UVeyraOffenceSet::GetMagicPowerAttribute()) * Damage.MagicPowerRatio;
}

TArray<FVeyraStatusSpec> StatusSpecs(TConstArrayView<FVeyraContentId> Ids)
{
	TArray<FVeyraStatusSpec> Specs;
	for (const FVeyraContentId& StatusId : Ids)
	{
		if (const TOptional<FVeyraStatusSpec> Status = UVeyraAbilitiesTuningSubsystem::FindStatus(StatusId))
		{
			Specs.Add(Status.GetValue());
		}
	}
	return Specs;
}

FVeyraPreparedEffects Prepare(UAbilitySystemComponent& Caster, const FVeyraEffectBundleTuning& Effects, int32 Rank)
{
	FVeyraPreparedEffects Prepared;
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
	Prepared.Statuses = StatusSpecs(Effects.Statuses);
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
		Ready.Statuses = StatusSpecs(Reaction.Statuses);
		Ready.Replaces = Reaction.Replaces;
	}
	return Prepared;
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
	Ready.Statuses = StatusSpecs(Impact.Statuses);
	return Ready;
}

bool IsEmpty(const FVeyraPreparedEffects& Effects)
{
	return !Effects.Damage.IsValid() && Effects.Statuses.IsEmpty() && !Effects.Displacement.IsSet() && Effects.Reactions.IsEmpty();
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
	if (!Target || (!Source.bSkipSpellShield && VeyraCombat::BlockAbilityHit(*Target, Caster)))
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
	const UVeyraStatusComponent* Ledger = Target->GetOwner() ? Target->GetOwner()->FindComponentByClass<UVeyraStatusComponent>() : nullptr;
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
		FVeyraRawDamageEvent Raw;
		Raw.Components = ReactionDamage;
		VeyraCombat::DealDamage(Caster, *Target, Raw);
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
		VeyraCombat::DealPreparedDamage(Effects.Damage, *Target, AddedAtImpact);
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
		if (const TOptional<FVeyraDisplacement> Displacement = DisplacementFor(Effects.Displacement.GetValue(), Unit, Frame, CasterRadius))
		{
			Hit.bDisplaced = VeyraCombat::Displace(Caster, *Target, Displacement.GetValue());
		}
	}
	if (Source.Ability.IsValid())
	{
		UVeyraAbilityEventSubsystem::Announce(Unit.GetWorld(), Hit);
	}
}
}
