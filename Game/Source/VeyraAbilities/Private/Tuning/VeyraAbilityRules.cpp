// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Tuning/VeyraAbilitiesTuning.h"

namespace VeyraAbilityRules
{
namespace
{
	/** Collects one tuning's problems under JSON pointers. */
	struct FAbilityTuningChecker
	{
		const FVeyraAbilitiesTuning& Tuning;
		TConstArrayView<int32> RankCounts;
		TArray<FString> Problems;

		void Problem(const FString& Pointer, const FString& Message)
		{
			Problems.Add(FString::Printf(TEXT("%s: %s"), *Pointer, *Message));
		}

		void CheckByRank(const FString& Pointer, TConstArrayView<double> ByRank)
		{
			if (ByRank.Num() != 1 && !RankCounts.Contains(ByRank.Num()))
			{
				TArray<FString> Allowed = { TEXT("1") };
				for (const int32 Count : RankCounts)
				{
					Allowed.Add(LexToString(Count));
				}
				Problem(Pointer, FString::Printf(TEXT("has %d values; it needs one for every rank, or one per rank (%s)"), ByRank.Num(),
					*FString::Join(Allowed, TEXT(" or "))));
			}
		}

		void CheckStatusIds(const FString& Pointer, TConstArrayView<FVeyraContentId> Ids)
		{
			for (int32 Index = 0; Index < Ids.Num(); ++Index)
			{
				if (!Tuning.Statuses.Contains(Ids[Index]))
				{
					Problem(FString::Printf(TEXT("%s/%d"), *Pointer, Index), FString::Printf(TEXT("names status \"%s\", which /statuses does not define"),
						*Ids[Index].ToString()));
				}
			}
		}

		void CheckCast(const FString& Pointer, const FVeyraCastTuning& Cast)
		{
			CheckByRank(Pointer + TEXT("/cooldownSecondsByRank"), Cast.CooldownSecondsByRank);
			CheckByRank(Pointer + TEXT("/resourceCostByRank"), Cast.ResourceCostByRank);
			if (Cast.RecastWindow.Num() > 1)
			{
				Problem(Pointer + TEXT("/recastWindow"), TEXT("holds at most one follow-up (ADR-018 §1)"));
			}
			for (int32 Index = 0; Index < Cast.RecastWindow.Num(); ++Index)
			{
				const FVeyraRecastTuning& Recast = Cast.RecastWindow[Index];
				const FString RecastPointer = FString::Printf(TEXT("%s/recastWindow/%d"), *Pointer, Index);
				if (!Defines(Tuning, Recast.Ability))
				{
					Problem(RecastPointer + TEXT("/ability"), FString::Printf(TEXT("names ability \"%s\", which no archetype defines"), *Recast.Ability.ToString()));
				}
				if (!(Recast.WindowSeconds > 0.0))
				{
					Problem(RecastPointer + TEXT("/windowSeconds"), TEXT("must be above 0"));
				}
				// ADR-030 §7: a held status only for TargetHeld, a fall window only for TargetFalls.
				const bool bHeld = Recast.OpensWhen == EVeyraRecastCondition::TargetHeld;
				const bool bFalls = Recast.OpensWhen == EVeyraRecastCondition::TargetFalls;
				if (Recast.HeldStatus.Num() != (bHeld ? 1 : 0))
				{
					Problem(RecastPointer + TEXT("/heldStatus"), TEXT("names one status for TargetHeld, and none otherwise"));
				}
				CheckStatusIds(RecastPointer + TEXT("/heldStatus"), Recast.HeldStatus);
				if (bFalls ? !(Recast.FallsWithinSeconds > 0.0) : Recast.FallsWithinSeconds != 0.0)
				{
					Problem(RecastPointer + TEXT("/fallsWithinSeconds"), TEXT("is above 0 for TargetFalls, and 0 otherwise"));
				}
				// ADR-032 §5: only a follow-up that always opens may arm.
				if (Recast.ArmingSeconds < 0.0 || (Recast.ArmingSeconds > 0.0 && Recast.OpensWhen != EVeyraRecastCondition::Always))
				{
					Problem(RecastPointer + TEXT("/armingSeconds"), TEXT("is at least 0, and above 0 only for Always"));
				}
			}
			if (Cast.TargetMustHold.Num() > 1)
			{
				Problem(Pointer + TEXT("/targetMustHold"), TEXT("names at most one status"));
			}
			CheckStatusIds(Pointer + TEXT("/targetMustHold"), Cast.TargetMustHold);
			if (Cast.TargetMustNotHold.Num() > 1)
			{
				Problem(Pointer + TEXT("/targetMustNotHold"), TEXT("names at most one status"));
			}
			// A share of the current resource, and a least the caster must hold (ADR-033 §3).
			if (Cast.CurrentResourceFraction.Num() > 1 || Cast.CurrentResourceFraction.ContainsByPredicate([](double Share) { return !(Share > 0.0 && Share <= 1.0); }))
			{
				Problem(Pointer + TEXT("/currentResourceFraction"), TEXT("holds at most one share, above 0 and at most 1"));
			}
			if (Cast.MinimumResource.Num() > 1 || Cast.MinimumResource.ContainsByPredicate([](double Least) { return !(Least > 0.0); }))
			{
				Problem(Pointer + TEXT("/minimumResource"), TEXT("holds at most one amount, above 0"));
			}
			CheckStatusIds(Pointer + TEXT("/targetMustNotHold"), Cast.TargetMustNotHold);
			if (Cast.TakedownRefund.Num() > 1 || Cast.TakedownRefund.ContainsByPredicate([](double Fraction) { return !(Fraction > 0.0 && Fraction <= 1.0); }))
			{
				Problem(Pointer + TEXT("/takedownRefund"), TEXT("holds at most one fraction, above 0 and at most 1"));
			}
			// Cooldowns scaled while a status holds (ADR-034 §8).
			for (int32 Index = 0; Index < Cast.CooldownWhile.Num(); ++Index)
			{
				const FVeyraCooldownWhileTuning& While = Cast.CooldownWhile[Index];
				const FString WhilePointer = FString::Printf(TEXT("%s/cooldownWhile/%d"), *Pointer, Index);
				CheckStatusIds(WhilePointer + TEXT("/status"), { While.Status });
				if (!(While.Multiplier > 0.0 && While.Multiplier <= 1.0))
				{
					Problem(WhilePointer + TEXT("/multiplier"), TEXT("must be above 0 and at most 1"));
				}
			}
			// A cooldown held under another ability's ID, one step only (ADR-035 §1).
			if (Cast.CooldownOf.Num() > 1)
			{
				Problem(Pointer + TEXT("/cooldownOf"), TEXT("names at most one ability"));
			}
			for (int32 Index = 0; Index < Cast.CooldownOf.Num(); ++Index)
			{
				const FVeyraCastTuning* Other = FindCast(Tuning, Cast.CooldownOf[Index]);
				if (!Other || Other == &Cast || !Other->CooldownOf.IsEmpty())
				{
					Problem(FString::Printf(TEXT("%s/cooldownOf/%d"), *Pointer, Index), FString::Printf(TEXT("names \"%s\": another ability with a cast, which shares no cooldown itself"),
						*Cast.CooldownOf[Index].ToString()));
				}
			}
			CheckStatusIds(Pointer + TEXT("/refusedWhile"), Cast.RefusedWhile);
		}

		void CheckDamage(const FString& Pointer, TConstArrayView<FVeyraDamageTuning> DamageList)
		{
			TArray<EVeyraDamageType, TInlineAllocator<3>> Types;
			for (int32 Index = 0; Index < DamageList.Num(); ++Index)
			{
				const FVeyraDamageTuning& Damage = DamageList[Index];
				const FString DamagePointer = FString::Printf(TEXT("%s/%d"), *Pointer, Index);
				CheckByRank(DamagePointer + TEXT("/amountByRank"), Damage.AmountByRank);
				if (Types.Contains(Damage.Type))
				{
					Problem(DamagePointer + TEXT("/type"), TEXT("repeats a damage type; one event has one component per type (Combat Bible §25)"));
				}
				Types.Add(Damage.Type);
			}
		}

		void CheckEffects(const FString& Pointer, const FVeyraEffectBundleTuning& Effects)
		{
			CheckDamage(Pointer + TEXT("/damage"), Effects.Damage);
			CheckStatusIds(Pointer + TEXT("/statuses"), Effects.Statuses);
			CheckStatusIds(Pointer + TEXT("/displacementUnlessStatuses"), Effects.DisplacementUnlessStatuses);
			for (int32 Index = 0; Index < Effects.Displacement.Num(); ++Index)
			{
				CheckStatusIds(FString::Printf(TEXT("%s/displacement/%d/collisionStatuses"), *Pointer, Index), Effects.Displacement[Index].CollisionStatuses);
			}
			if (!Effects.DisplacementUnlessStatuses.IsEmpty() && Effects.Displacement.IsEmpty())
			{
				Problem(Pointer + TEXT("/displacementUnlessStatuses"), TEXT("spares units a displacement, so the effects need one"));
			}
			if (!Effects.MissingHealthDamage.IsEmpty() && Effects.Damage.IsEmpty())
			{
				Problem(Pointer + TEXT("/missingHealthDamage"), TEXT("joins the hit's damage, so the effects need damage too"));
			}
			for (int32 Index = 0; Index < Effects.Reactions.Num(); ++Index)
			{
				const FVeyraReactionTuning& Reaction = Effects.Reactions[Index];
				const FString ReactionPointer = FString::Printf(TEXT("%s/reactions/%d"), *Pointer, Index);
				CheckStatusIds(ReactionPointer + TEXT("/status"), { Reaction.Status });
				CheckDamage(ReactionPointer + TEXT("/damage"), Reaction.Damage);
				CheckStatusIds(ReactionPointer + TEXT("/statuses"), Reaction.Statuses);
				CheckStatusIds(ReactionPointer + TEXT("/replaces"), Reaction.Replaces);
				if (Reaction.Damage.IsEmpty() && Reaction.Statuses.IsEmpty() && Reaction.Consume == EVeyraReactionConsume::Keep)
				{
					Problem(ReactionPointer, TEXT("does nothing: it needs damage, statuses or to consume what it reacts to"));
				}
				for (const FVeyraContentId& Replaced : Reaction.Replaces)
				{
					if (!Effects.Statuses.Contains(Replaced))
					{
						Problem(ReactionPointer + TEXT("/replaces"), FString::Printf(TEXT("replaces \"%s\", which the effects do not give"), *Replaced.ToString()));
					}
				}
				// A burst around the reacting target (ADR-034 §6).
				if (Reaction.Burst.Num() > 1)
				{
					Problem(ReactionPointer + TEXT("/burst"), TEXT("holds at most one burst"));
				}
				for (int32 BurstIndex = 0; BurstIndex < Reaction.Burst.Num(); ++BurstIndex)
				{
					const FVeyraReactionBurstTuning& Burst = Reaction.Burst[BurstIndex];
					const FString BurstPointer = FString::Printf(TEXT("%s/burst/%d"), *ReactionPointer, BurstIndex);
					for (const FString& ShapeProblem : VeyraShapes::Validate(Burst.Shape))
					{
						Problem(BurstPointer + TEXT("/shape"), ShapeProblem);
					}
					CheckDamage(BurstPointer + TEXT("/damage"), Burst.Damage);
					CheckStatusIds(BurstPointer + TEXT("/statuses"), Burst.Statuses);
					if (Burst.Damage.IsEmpty() && Burst.Statuses.IsEmpty())
					{
						Problem(BurstPointer, TEXT("does nothing: it needs damage or statuses"));
					}
				}
			}
			TArray<EVeyraUnitKind> Kinds;
			for (int32 Index = 0; Index < Effects.UnitKindMultipliers.Num(); ++Index)
			{
				const FVeyraUnitKindMultiplierTuning& Entry = Effects.UnitKindMultipliers[Index];
				const bool bRepeated = Kinds.Contains(Entry.Kind);
				Kinds.Add(Entry.Kind);
				if (!(Entry.Multiplier >= 1.0) || !FMath::IsFinite(Entry.Multiplier) || Entry.Kind == EVeyraUnitKind::Structure || bRepeated)
				{
					Problem(FString::Printf(TEXT("%s/unitKindMultipliers/%d"), *Pointer, Index),
						TEXT("a finite multiplier of at least 1, once per kind, never against structures (Combat Bible §33)"));
				}
			}
		}

		void CheckStatuses()
		{
			for (const TPair<FVeyraContentId, FVeyraStatusTuning>& Status : Tuning.Statuses)
			{
				const FString Pointer = FString::Printf(TEXT("/statuses/%s"), *Status.Key.ToString());
				const bool bTicks = Status.Value.Kind == EVeyraStatusKind::DamageOverTime;
				if (Status.Value.DamageOverTime.Num() != (bTicks ? 1 : 0))
				{
					Problem(Pointer + TEXT("/damageOverTime"), TEXT("holds one entry for a DamageOverTime status, and none for any other kind"));
					continue;
				}
				for (const FString& StatusProblem : VeyraStatuses::Validate(ToStatusSpec(Status.Key, Status.Value)))
				{
					Problem(Pointer, StatusProblem);
				}
				// A status that turns into another at its most stacks has stacks to fill, and another to become (ADR-026 §2).
				const TArray<FVeyraContentId>& Becomes = Status.Value.AtMaxStacks;
				CheckStatusIds(Pointer + TEXT("/atMaxStacks"), Becomes);
				if (Becomes.Num() > 1 || (!Becomes.IsEmpty() && (Status.Value.MaxStacks < 2 || Becomes[0] == Status.Key)))
				{
					Problem(Pointer + TEXT("/atMaxStacks"), TEXT("names at most one other status, for a status that stacks"));
				}
			}
		}

		void CheckTargetedDamage(const FString& Pointer, const FVeyraTargetedDamageAbilityTuning& Targeted)
		{
			CheckStatusIds(Pointer + TEXT("/statuses"), Targeted.Statuses);
			if (!(Targeted.DamageAmount > 0.0) && !(Targeted.DamagePerLevel > 0.0) && Targeted.Statuses.IsEmpty())
			{
				Problem(Pointer + TEXT("/damageAmount"), TEXT("deals no damage and applies no status, so it does nothing"));
			}
			if (Targeted.TargetKinds.Contains(EVeyraUnitKind::Structure))
			{
				Problem(Pointer + TEXT("/targetKinds"), TEXT("an ability does not target structures (Combat Bible §33)"));
			}
		}

		void CheckFluxSpells()
		{
			const FVeyraFluxSpellsTuning& Spells = Tuning.FluxSpells;
			for (int32 Index = 0; Index < Spells.Roster.Num(); ++Index)
			{
				const FVeyraContentId& Spell = Spells.Roster[Index];
				const FString Pointer = FString::Printf(TEXT("/fluxSpells/roster/%d"), Index);
				if (!Defines(Tuning, Spell))
				{
					Problem(Pointer, FString::Printf(TEXT("names \"%s\", which no archetype map defines"), *Spell.ToString()));
					continue;
				}
				if (Spells.Roster.IndexOfByKey(Spell) != Index)
				{
					Problem(Pointer, FString::Printf(TEXT("lists \"%s\" twice"), *Spell.ToString()));
				}
				// A spell has no ranks (ADR-015 §1): its every rank list holds one value.
				for (const FString& RankProblem : ValidateRanks(Tuning, Spell, 1))
				{
					Problem(Pointer, FString::Printf(TEXT("\"%s\" has ranks: %s"), *Spell.ToString(), *RankProblem));
				}
			}
		}

		void CheckArea(const FString& Pointer, const FVeyraAreaAbilityTuning& Area)
		{
			CheckCast(Pointer + TEXT("/cast"), Area.Cast);
			if (Area.Origin == EVeyraAreaOrigin::TargetPoint && !(Area.Cast.CastRange > 0.0))
			{
				Problem(Pointer + TEXT("/cast/castRange"), TEXT("must be above 0 for an area at a ground point"));
			}
			if (Area.ChannelTicks > 1 && !(Area.ChannelSeconds > 0.0))
			{
				Problem(Pointer + TEXT("/channelSeconds"), TEXT("must be above 0 when the area hits more than once"));
			}
			if ((Area.Reveal.Radius > 0.0) != (Area.Reveal.DurationSeconds > 0.0))
			{
				Problem(Pointer + TEXT("/reveal"), TEXT("radius and durationSeconds are both above 0, or both 0 for no reveal"));
			}
			CheckZones(Pointer + TEXT("/zones"), Area.Zones);
			// Landing on its caster's lingering area or marker names one ability that leaves one; no other origin names any.
			const bool bOnLingering = Area.Origin == EVeyraAreaOrigin::CastersLingeringArea;
			const bool bOnMarker = Area.Origin == EVeyraAreaOrigin::CastersMarker;
			const FVeyraAreaAbilityTuning* Named = Area.OriginAbility.Num() == 1 ? Tuning.Area.Find(Area.OriginAbility[0]) : nullptr;
			const FVeyraSkillshotAbilityTuning* Walling = Area.OriginAbility.Num() == 1 ? Tuning.Skillshot.Find(Area.OriginAbility[0]) : nullptr;
			const bool bNamesMarker = Area.OriginAbility.Num() == 1 && (LeavesMarker(Area.OriginAbility[0]) || (Walling && !Walling->EndWall.IsEmpty()));
			if (bOnLingering ? !(Named && !Named->Linger.IsEmpty()) : bOnMarker ? !bNamesMarker : !Area.OriginAbility.IsEmpty())
			{
				Problem(Pointer + TEXT("/originAbility"), TEXT("names exactly one area that lingers for the CastersLingeringArea origin, one ability that leaves a marker or a wall for CastersMarker, and none for any other"));
			}
			CheckStatusIds(Pointer + TEXT("/consumesCasterStatuses"), Area.ConsumesCasterStatuses);
			CheckStatusIds(Pointer + TEXT("/casterStatuses"), Area.CasterStatuses);
			if (Area.ChannelMovement == EVeyraCastMovement::Free && Area.ChannelTicks < 2)
			{
				Problem(Pointer + TEXT("/channelMovement"), TEXT("only a channel of two ticks or more lets its caster move"));
			}
			if (Area.HealOnHit.Num() > 1)
			{
				Problem(Pointer + TEXT("/healOnHit"), TEXT("holds at most one"));
			}
			for (int32 Index = 0; Index < Area.HealOnHit.Num(); ++Index)
			{
				const FVeyraHealOnHitTuning& Heal = Area.HealOnHit[Index];
				if (!(Heal.MaxHealthRatioPerHit > 0.0) || Heal.CapMaxHealthRatio < Heal.MaxHealthRatioPerHit)
				{
					Problem(FString::Printf(TEXT("%s/healOnHit/%d"), *Pointer, Index), TEXT("maxHealthRatioPerHit is above 0 and capMaxHealthRatio at least it"));
				}
			}
			if (Area.Linger.Num() > 1)
			{
				Problem(Pointer + TEXT("/linger"), TEXT("holds at most one lingering area (ADR-018 §5)"));
			}
			for (int32 Index = 0; Index < Area.Linger.Num(); ++Index)
			{
				// A field's extent is its area's circle (ADR-033 §5).
				const TArray<FVeyraMovementFieldTuning>& Field = Area.Linger[Index].MovementField;
				if (Field.Num() > 1 || Field.ContainsByPredicate([](const FVeyraMovementFieldTuning& Each) { return !(Each.Pull > 0.0); })
					|| (!Field.IsEmpty() && (Area.Zones.IsEmpty() || Area.Zones.Last().Shape.Kind != EVeyraShapeKind::Circle)))
				{
					Problem(FString::Printf(TEXT("%s/linger/%d/movementField"), *Pointer, Index), TEXT("holds at most one, its pull above 0, on an area whose outermost zone is a circle"));
				}
				const FVeyraLingerTuning& Linger = Area.Linger[Index];
				const FString LingerPointer = FString::Printf(TEXT("%s/linger/%d"), *Pointer, Index);
				// A delayed area lingers where it lands, once it lands (ADR-027 §6); a channelled one never does.
				if (Area.ChannelTicks > 1)
				{
					Problem(LingerPointer, TEXT("lingers after an area that hits once, not a channelled one"));
				}
				if (!(Linger.DurationSeconds > 0.0) || !(Linger.PulseSeconds > 0.0) || Linger.PulseSeconds > Linger.DurationSeconds)
				{
					Problem(LingerPointer, TEXT("durationSeconds and pulseSeconds are above 0, and a pulse is no longer than the area lasts"));
				}
				CheckStatusIds(LingerPointer + TEXT("/casterStatuses"), Linger.CasterStatuses);
				CheckStatusIds(LingerPointer + TEXT("/allyStatuses"), Linger.AllyStatuses);
				CheckStatusIds(LingerPointer + TEXT("/enemyStatuses"), Linger.EnemyStatuses);
				// What it does at each pulse and as it ends, and a warning for the end (ADR-026 §4).
				if (Linger.PulseEffects.Num() > 1 || Linger.EndEffects.Num() > 1)
				{
					Problem(LingerPointer, TEXT("pulseEffects and endEffects hold at most one bundle each"));
				}
				for (int32 Bundle = 0; Bundle < Linger.PulseEffects.Num(); ++Bundle)
				{
					CheckEffects(FString::Printf(TEXT("%s/pulseEffects/%d"), *LingerPointer, Bundle), Linger.PulseEffects[Bundle]);
				}
				for (int32 Bundle = 0; Bundle < Linger.EndEffects.Num(); ++Bundle)
				{
					CheckEffects(FString::Printf(TEXT("%s/endEffects/%d"), *LingerPointer, Bundle), Linger.EndEffects[Bundle]);
				}
				if ((Linger.EndWarningSeconds > 0.0) == Linger.EndEffects.IsEmpty() || Linger.EndWarningSeconds > Linger.DurationSeconds)
				{
					Problem(LingerPointer + TEXT("/endWarningSeconds"), TEXT("above 0 exactly when the area has endEffects, and no longer than it lasts"));
				}
			}
			// A delayed area may land sooner inside its caster's lingering area of another ability (ADR-026 §4).
			for (int32 Index = 0; Index < Area.DelayWithin.Num(); ++Index)
			{
				const FVeyraAreaDelayWithinTuning& Within = Area.DelayWithin[Index];
				const FString WithinPointer = FString::Printf(TEXT("%s/delayWithin/%d"), *Pointer, Index);
				const FVeyraAreaAbilityTuning* Lingering = Tuning.Area.Find(Within.Ability);
				if (!Lingering || Lingering->Linger.IsEmpty())
				{
					Problem(WithinPointer + TEXT("/ability"), FString::Printf(TEXT("names \"%s\", which is no area ability that lingers"), *Within.Ability.ToString()));
				}
				if (!(Area.DelaySeconds > 0.0) || !(Within.DelaySeconds > 0.0))
				{
					Problem(WithinPointer + TEXT("/delaySeconds"), TEXT("must be above 0, for an area that is delayed"));
				}
			}
		}

		void CheckSecondaryImpact(const FString& Pointer, const FVeyraSecondaryImpactTuning& Impact)
		{
			for (const FString& ShapeProblem : VeyraShapes::Validate(Impact.Shape))
			{
				Problem(Pointer + TEXT("/shape"), ShapeProblem);
			}
			CheckDamage(Pointer + TEXT("/damage"), Impact.Damage);
			CheckStatusIds(Pointer + TEXT("/statuses"), Impact.Statuses);
		}

		void CheckSelfBuff(const FString& Pointer, const FVeyraSelfBuffAbilityTuning& Buff)
		{
			if (Buff.Marker.Num() > 1)
			{
				Problem(Pointer + TEXT("/marker"), TEXT("holds at most one"));
			}
			for (int32 Index = 0; Index < Buff.Marker.Num(); ++Index)
			{
				const FVeyraBuffMarkerTuning& Marker = Buff.Marker[Index];
				const FString MarkerPointer = FString::Printf(TEXT("%s/marker/%d"), *Pointer, Index);
				if (!(Marker.LifetimeSeconds > 0.0) || Marker.HitsToDestroy < 0)
				{
					Problem(MarkerPointer, TEXT("a marker lasts above 0 seconds and takes 0 or more hits"));
				}
				CheckZones(MarkerPointer + TEXT("/burstZones"), Marker.BurstZones);
			}
			CheckCast(Pointer + TEXT("/cast"), Buff.Cast);
			if (Buff.AttackSecondaryImpact.Num() > 1)
			{
				Problem(Pointer + TEXT("/attackSecondaryImpact"), TEXT("holds at most one"));
			}
			// An ally's buff is cast at the ally; what stays with the caster (its slots, its end payload
			// and its stance) belongs to a buff of its own (ADR-027 §4).
			if (Buff.Recipient == EVeyraBuffRecipient::CasterOrAlly
				&& (!(Buff.Cast.CastRange > 0.0) || !Buff.Variants.IsEmpty() || !Buff.EndPayload.IsEmpty() || Buff.Recast == EVeyraRecast::EndsEarly))
			{
				Problem(Pointer + TEXT("/recipient"), TEXT("an ally's buff has a cast range above 0, and no variants, end payload or early end"));
			}
			CheckZones(Pointer + TEXT("/recipientZones"), Buff.RecipientZones);
			// A drain ends the buff early, so the buff is its caster's own (ADR-033 §6).
			if (Buff.Drain.Num() > 1 || Buff.Drain.ContainsByPredicate([](const FVeyraDrainTuning& Each) {
					return !(Each.PerSecond > 0.0) || !(Each.IntervalSeconds > 0.0) || !(Each.MaxSeconds >= Each.IntervalSeconds);
				}) || (!Buff.Drain.IsEmpty() && Buff.Recipient != EVeyraBuffRecipient::Caster))
			{
				Problem(Pointer + TEXT("/drain"), TEXT("holds at most one, on its caster's own buff, perSecond and intervalSeconds above 0 and maxSeconds at least the interval"));
			}
			// What its shield holds, and its burst, follow the shield (ADR-032 §3).
			if ((!Buff.ShieldHolds.IsEmpty() || !Buff.ShieldEndZones.IsEmpty()) && Buff.Shields.IsEmpty())
			{
				Problem(Pointer, TEXT("shieldHolds and shieldEndZones need a shield"));
			}
			CheckStatusIds(Pointer + TEXT("/shieldHolds"), Buff.ShieldHolds);
			CheckZones(Pointer + TEXT("/shieldEndZones"), Buff.ShieldEndZones);
			for (int32 Index = 0; Index < Buff.AttackSecondaryImpact.Num(); ++Index)
			{
				const FString ImpactPointer = FString::Printf(TEXT("%s/attackSecondaryImpact/%d"), *Pointer, Index);
				if (!(Buff.AttackSecondaryImpact[Index].Seconds > 0.0))
				{
					Problem(ImpactPointer + TEXT("/seconds"), TEXT("must be above 0"));
				}
				CheckSecondaryImpact(ImpactPointer + TEXT("/impact"), Buff.AttackSecondaryImpact[Index].Impact);
			}
			for (int32 Index = 0; Index < Buff.Variants.Num(); ++Index)
			{
				const FVeyraVariantTuning& Variant = Buff.Variants[Index];
				const FString VariantPointer = FString::Printf(TEXT("%s/variants/%d"), *Pointer, Index);
				if (!Defines(Tuning, Variant.Ability))
				{
					Problem(VariantPointer + TEXT("/ability"), FString::Printf(TEXT("names ability \"%s\", which no archetype defines"), *Variant.Ability.ToString()));
				}
				if (!(Variant.DurationSeconds > 0.0))
				{
					Problem(VariantPointer + TEXT("/durationSeconds"), TEXT("must be above 0"));
				}
				if (Variant.Slot != EVeyraAbilitySlot::Q && Variant.Slot != EVeyraAbilitySlot::W && Variant.Slot != EVeyraAbilitySlot::E && Variant.Slot != EVeyraAbilitySlot::R)
				{
					Problem(VariantPointer + TEXT("/slot"), TEXT("must be Q, W, E or R: a variant holds an ability's slot"));
				}
			}
			CheckStatusIds(Pointer + TEXT("/statuses"), Buff.Statuses);
			for (int32 Index = 0; Index < Buff.Shields.Num(); ++Index)
			{
				CheckShield(FString::Printf(TEXT("%s/shields/%d"), *Pointer, Index), Buff.Shields[Index]);
			}
			for (int32 Index = 0; Index < Buff.Aura.Num(); ++Index)
			{
				const FVeyraAuraTuning& Aura = Buff.Aura[Index];
				const FString AuraPointer = FString::Printf(TEXT("%s/aura/%d"), *Pointer, Index);
				if (Aura.RefreshSeconds > Aura.DurationSeconds)
				{
					Problem(AuraPointer + TEXT("/refreshSeconds"), TEXT("must be at most the aura's duration"));
				}
				CheckStatusIds(AuraPointer + TEXT("/allyStatuses"), Aura.AllyStatuses);
				CheckStatusIds(AuraPointer + TEXT("/enemyStatuses"), Aura.EnemyStatuses);
				CheckStatusIds(AuraPointer + TEXT("/allyFluxbornStatuses"), Aura.AllyFluxbornStatuses);
			}
			if (Buff.TemporaryHealth.Num() > 1 || Buff.EndPayload.Num() > 1)
			{
				Problem(Pointer, TEXT("temporaryHealth and endPayload each hold at most one"));
			}
			for (int32 Index = 0; Index < Buff.TemporaryHealth.Num(); ++Index)
			{
				const FVeyraTemporaryHealthTuning& Temporary = Buff.TemporaryHealth[Index];
				const FString TemporaryPointer = FString::Printf(TEXT("%s/temporaryHealth/%d"), *Pointer, Index);
				CheckByRank(TemporaryPointer + TEXT("/amountByRank"), Temporary.AmountByRank);
				if (!(Temporary.DurationSeconds > 0.0) || Temporary.MaxHealthRatio < 0.0)
				{
					Problem(TemporaryPointer, TEXT("durationSeconds is above 0 and maxHealthRatio at least 0"));
				}
			}
			for (int32 Index = 0; Index < Buff.EndPayload.Num(); ++Index)
			{
				const FVeyraEndPayloadTuning& Payload = Buff.EndPayload[Index];
				const FString PayloadPointer = FString::Printf(TEXT("%s/endPayload/%d"), *Pointer, Index);
				CheckStatusIds(PayloadPointer + TEXT("/status"), { Payload.Status });
				if (!(Payload.AfterSeconds > 0.0) || !(Payload.Radius > 0.0) || !(Payload.BaseSeconds > 0.0) || Payload.SecondsPerHit < 0.0
					|| Payload.MaxSeconds < Payload.BaseSeconds || Payload.MinHits < 0)
				{
					Problem(PayloadPointer, TEXT("afterSeconds, radius and baseSeconds are above 0, secondsPerHit at least 0, and maxSeconds at least baseSeconds"));
				}
				if (Payload.Displacement.Num() > 1 || Payload.Displacement.ContainsByPredicate([](const FVeyraDisplacementTuning& Push) { return Push.Direction != EVeyraDisplacementDirection::AwayFromOrigin; }))
				{
					Problem(PayloadPointer + TEXT("/displacement"), TEXT("holds at most one, AwayFromOrigin: it pushes away from the holder"));
				}
				for (const FVeyraDisplacementTuning& Push : Payload.Displacement)
				{
					CheckStatusIds(PayloadPointer + TEXT("/displacement/0/collisionStatuses"), Push.CollisionStatuses);
				}
			}
			for (int32 Index = 0; Index < Buff.Heal.Num(); ++Index)
			{
				CheckStatusIds(FString::Printf(TEXT("%s/heal/%d/statuses"), *Pointer, Index), Buff.Heal[Index].Statuses);
			}
			// Its companion's part lasts while its caster's own statuses do, and needs the companion (ADR-034 §7).
			CheckStatusIds(Pointer + TEXT("/companionStatuses"), Buff.CompanionStatuses);
			const bool bCompanionPart = !Buff.CompanionStatuses.IsEmpty() || !Buff.Chain.IsEmpty() || Buff.CompanionDeath == EVeyraCompanionDeath::Ends;
			if (bCompanionPart && (Buff.Statuses.IsEmpty() || Buff.Recipient != EVeyraBuffRecipient::Caster || Buff.Cast.NeedsCompanion != EVeyraCompanionNeed::Living))
			{
				Problem(Pointer, TEXT("companionStatuses, a chain and companionDeath Ends need the caster's own statuses and a cast that needs its living companion"));
			}
			if (Buff.Chain.Num() > 1)
			{
				Problem(Pointer + TEXT("/chain"), TEXT("holds at most one"));
			}
			for (int32 Index = 0; Index < Buff.Chain.Num(); ++Index)
			{
				const FVeyraChainTuning& Chain = Buff.Chain[Index];
				const FString ChainPointer = FString::Printf(TEXT("%s/chain/%d"), *Pointer, Index);
				if (!(Chain.Width > 0.0) || !(Chain.PulseSeconds > 0.0) || !(Chain.PerEnemySeconds >= Chain.PulseSeconds))
				{
					Problem(ChainPointer, TEXT("width and pulseSeconds are above 0, and perEnemySeconds at least pulseSeconds"));
				}
				CheckEffects(ChainPointer + TEXT("/effects"), Chain.Effects);
			}
		}

		void CheckCommand(const FString& Pointer, const FVeyraCommandAbilityTuning& Command)
		{
			CheckCast(Pointer + TEXT("/cast"), Command.Cast);
			CheckZones(Pointer + TEXT("/landingZones"), Command.LandingZones);
			if (Command.Order == EVeyraCompanionOrder::Hold
				&& (!(Command.Cast.CastRange > 0.0) || !(Command.LeapSpeed > 0.0) || !(Command.HoldSeconds > 0.0)))
			{
				Problem(Pointer, TEXT("a hold has a cast range, a leap speed and hold seconds above 0"));
			}
			const bool bHoldOrRecall = Command.Order == EVeyraCompanionOrder::Hold || Command.Order == EVeyraCompanionOrder::Recall;
			if (Command.Order != EVeyraCompanionOrder::Hold && (Command.LeapSpeed != 0.0 || Command.HoldSeconds != 0.0 || !Command.LandingZones.IsEmpty()))
			{
				Problem(Pointer, TEXT("only a hold leaps, holds and lands"));
			}
			// A summon forms one companion for a while, bound to the unit it names; a redirect binds it anew (ADR-035 §5).
			if (bHoldOrRecall && (!Command.Companion.IsEmpty() || Command.LifetimeSeconds != 0.0 || Command.BindTo != EVeyraCompanionBind::None))
			{
				Problem(Pointer, TEXT("a hold or a recall names no companion, lifetime or unit to bind"));
			}
			if (Command.Order == EVeyraCompanionOrder::Summon
				&& (Command.Companion.Num() != 1 || !(Command.LifetimeSeconds > 0.0) || Command.BindTo == EVeyraCompanionBind::None || !(Command.Cast.CastRange > 0.0)))
			{
				Problem(Pointer, TEXT("a summon names one companion, a lifetime above 0, the unit it binds and a cast range above 0"));
			}
			if (Command.Order == EVeyraCompanionOrder::Redirect
				&& (!Command.Companion.IsEmpty() || Command.LifetimeSeconds != 0.0 || Command.BindTo == EVeyraCompanionBind::None || !(Command.Cast.CastRange > 0.0)))
			{
				Problem(Pointer, TEXT("a redirect names no companion or lifetime, but the unit it binds and a cast range above 0"));
			}
			for (const FVeyraContentId& Companion : Command.Companion)
			{
				if (!Tuning.Companions.Contains(Companion))
				{
					Problem(Pointer + TEXT("/companion"), FString::Printf(TEXT("names \"%s\", which /companions does not define"), *Companion.ToString()));
				}
			}
		}

		void CheckZones(const FString& Pointer, TConstArrayView<FVeyraAreaZoneTuning> Zones)
		{
			for (int32 Index = 0; Index < Zones.Num(); ++Index)
			{
				const FString ZonePointer = FString::Printf(TEXT("%s/%d"), *Pointer, Index);
				for (const FString& ShapeProblem : VeyraShapes::Validate(Zones[Index].Shape))
				{
					Problem(ZonePointer + TEXT("/shape"), ShapeProblem);
				}
				CheckEffects(ZonePointer + TEXT("/effects"), Zones[Index].Effects);
				for (int32 ShieldIndex = 0; ShieldIndex < Zones[Index].CasterShieldPerVanguard.Num(); ++ShieldIndex)
				{
					CheckShield(FString::Printf(TEXT("%s/casterShieldPerVanguard/%d"), *ZonePointer, ShieldIndex), Zones[Index].CasterShieldPerVanguard[ShieldIndex]);
				}
				CheckStatusIds(ZonePointer + TEXT("/casterStatusesPerVanguard"), Zones[Index].CasterStatusesPerVanguard);
				CheckAllyEffects(ZonePointer + TEXT("/allyEffects"), Zones[Index].AllyEffects);
			}
		}

		/** What a zone or a ride's contact does for its caster's allies: a heal, statuses, or both (ADR-035 §4). */
		void CheckAllyEffects(const FString& Pointer, TConstArrayView<FVeyraZoneAllyEffectsTuning> AllyEffects)
		{
			if (AllyEffects.Num() > 1)
			{
				Problem(Pointer, TEXT("holds at most one"));
			}
			for (int32 AllyIndex = 0; AllyIndex < AllyEffects.Num(); ++AllyIndex)
			{
				const FVeyraZoneAllyEffectsTuning& Allies = AllyEffects[AllyIndex];
				const FString AllyPointer = FString::Printf(TEXT("%s/%d"), *Pointer, AllyIndex);
				CheckByRank(AllyPointer + TEXT("/healByRank"), Allies.HealByRank);
				CheckStatusIds(AllyPointer + TEXT("/statuses"), Allies.Statuses);
				const bool bHeals = Allies.HealMagicPowerRatio > 0.0 || Allies.HealByRank.ContainsByPredicate([](double Amount) { return Amount > 0.0; });
				if (Allies.HealMagicPowerRatio < 0.0 || Allies.HealByRank.ContainsByPredicate([](double Amount) { return Amount < 0.0; }) || (!bHeals && Allies.Statuses.IsEmpty()))
				{
					Problem(AllyPointer, TEXT("heals by at least 0 and a ratio of at least 0, and heals or gives a status"));
				}
			}
		}

		void CheckDismount(const FString& Pointer, const FVeyraDismountAbilityTuning& Dismount)
		{
			CheckCast(Pointer + TEXT("/cast"), Dismount.Cast);
		}

		void CheckShield(const FString& Pointer, const FVeyraShieldTuning& Shield)
		{
			CheckByRank(Pointer + TEXT("/amountByRank"), Shield.AmountByRank);
			if (Shield.AbsorbedReward.Num() > 1)
			{
				Problem(Pointer + TEXT("/absorbedReward"), TEXT("holds at most one"));
			}
			for (int32 Index = 0; Index < Shield.AbsorbedReward.Num(); ++Index)
			{
				const FVeyraAbsorbedRewardTuning& Reward = Shield.AbsorbedReward[Index];
				const FString RewardPointer = FString::Printf(TEXT("%s/absorbedReward/%d"), *Pointer, Index);
				if (!(Reward.Fraction > 0.0) || Reward.Fraction > 1.0 || Reward.Statuses.IsEmpty())
				{
					Problem(RewardPointer, TEXT("fraction is above 0 and at most 1, and it names a status"));
				}
				CheckStatusIds(RewardPointer + TEXT("/statuses"), Reward.Statuses);
			}
			for (const FVeyraShieldCapGroupTuning& Group : Shield.CapGroup)
			{
				if (Group.TotalMaxHealthRatio < Shield.MaxAmountMaxHealthRatio)
				{
					Problem(Pointer + TEXT("/capGroup/0/totalMaxHealthRatio"), TEXT("a group holds at least as much as one of its shields"));
				}
			}
		}

		void CheckSkillshot(const FString& Pointer, const FVeyraSkillshotAbilityTuning& Skillshot)
		{
			if (Skillshot.ReturnIfHeld.Num() > 1)
			{
				Problem(Pointer + TEXT("/returnIfHeld"), TEXT("holds at most one"));
			}
			for (int32 Index = 0; Index < Skillshot.ReturnIfHeld.Num(); ++Index)
			{
				const FVeyraReturnShotTuning& Return = Skillshot.ReturnIfHeld[Index];
				const FString ReturnPointer = FString::Printf(TEXT("%s/returnIfHeld/%d"), *Pointer, Index);
				CheckStatusIds(ReturnPointer + TEXT("/status"), MakeArrayView(&Return.Status, 1));
				if (!(Return.Speed > 0.0) || !(Return.CooldownRefund > 0.0 && Return.CooldownRefund <= 1.0))
				{
					Problem(ReturnPointer, TEXT("its speed is above 0, and its refund above 0 and at most 1"));
				}
			}
			if (Skillshot.CasterDash.Num() > 1)
			{
				Problem(Pointer + TEXT("/casterDash"), TEXT("holds at most one"));
			}
			for (int32 Index = 0; Index < Skillshot.CasterDash.Num(); ++Index)
			{
				if (!(Skillshot.CasterDash[Index].Distance > 0.0) || !(Skillshot.CasterDash[Index].Speed > 0.0))
				{
					Problem(FString::Printf(TEXT("%s/casterDash/%d"), *Pointer, Index), TEXT("distance and speed are above 0"));
				}
			}
			CheckCast(Pointer + TEXT("/cast"), Skillshot.Cast);
			CheckEffects(Pointer + TEXT("/effects"), Skillshot.Effects);
			CheckEffects(Pointer + TEXT("/passThroughEffects"), Skillshot.PassThroughEffects);
			if (Skillshot.Mimic.Num() > 1)
			{
				Problem(Pointer + TEXT("/mimic"), TEXT("holds at most one"));
			}
			if (Skillshot.EndWall.Num() > 1 || Skillshot.EndWall.ContainsByPredicate([](const FVeyraWallTuning& Wall) {
					return !(Wall.Length > 0.0) || !(Wall.Thickness > 0.0) || !(Wall.LifetimeSeconds > 0.0);
				}))
			{
				Problem(Pointer + TEXT("/endWall"), TEXT("holds at most one, its length, thickness and lifetime above 0"));
			}
			for (int32 Index = 0; Index < Skillshot.Mimic.Num(); ++Index)
			{
				const FString MimicPointer = FString::Printf(TEXT("%s/mimic/%d"), *Pointer, Index);
				CheckEffects(MimicPointer + TEXT("/repeatEffects"), Skillshot.Mimic[Index].RepeatEffects);
				if (!LeavesMarker(Skillshot.Mimic[Index].MarkerAbility))
				{
					Problem(MimicPointer + TEXT("/markerAbility"), TEXT("names an ability that leaves a marker: a placement, or a self-buff with one"));
				}
			}
			const FVeyraEffectBundleTuning& PassThrough = Skillshot.PassThroughEffects;
			const bool bPassesThrough = !PassThrough.Damage.IsEmpty() || !PassThrough.Statuses.IsEmpty() || !PassThrough.Displacement.IsEmpty();
			if (bPassesThrough && Skillshot.Collision != EVeyraSkillshotCollision::FirstEnemyVanguard)
			{
				Problem(Pointer + TEXT("/passThroughEffects"), TEXT("only a FirstEnemyVanguard skillshot passes through units; leave it empty"));
			}
		}

		void CheckDash(const FString& Pointer, const FVeyraDashAbilityTuning& Dash)
		{
			CheckCast(Pointer + TEXT("/cast"), Dash.Cast);
			CheckZones(Pointer + TEXT("/startZones"), Dash.StartZones);
			CheckEffects(Pointer + TEXT("/contactEffects"), Dash.ContactEffects);
			CheckStatusIds(Pointer + TEXT("/contactSelfStatuses"), Dash.ContactSelfStatuses);
			CheckEffects(Pointer + TEXT("/hostEffects"), Dash.HostEffects);
			CheckZones(Pointer + TEXT("/endZones"), Dash.EndZones);
			const FVeyraEffectBundleTuning& Host = Dash.HostEffects;
			const bool bHasHostEffects = !Host.Damage.IsEmpty() || !Host.Statuses.IsEmpty() || !Host.Displacement.IsEmpty() || !Host.MissingHealthDamage.IsEmpty();
			if (bHasHostEffects && Dash.Direction != EVeyraDashDirection::AwayFromHost)
			{
				Problem(Pointer + TEXT("/hostEffects"), TEXT("only an AwayFromHost dash has a host to affect; leave it empty"));
			}
			// Through a target (ADR-030 §6): it needs a reach for its target, and passes the unit rather than stopping at it.
			if (Dash.Direction == EVeyraDashDirection::ThroughTarget && (!(Dash.Cast.CastRange > 0.0) || Dash.Contact != EVeyraDashContact::None))
			{
				Problem(Pointer + TEXT("/direction"), TEXT("a ThroughTarget dash has a cast range above 0 and no contact stop"));
			}
			if (Dash.Direction != EVeyraDashDirection::ThroughTarget && !Dash.TargetKinds.IsEmpty())
			{
				Problem(Pointer + TEXT("/targetKinds"), TEXT("only a ThroughTarget dash names the kinds it passes through"));
			}
			CheckTargetKinds(Pointer + TEXT("/targetKinds"), Dash.TargetKinds);
		}

		/** The kinds a targeted ability names: never a structure (Combat Bible §33). */
		void CheckTargetKinds(const FString& Pointer, TConstArrayView<EVeyraUnitKind> Kinds)
		{
			if (Kinds.Contains(EVeyraUnitKind::Structure))
			{
				Problem(Pointer, TEXT("an ability does not target structures (Combat Bible §33)"));
			}
		}

		void CheckTether(const FString& Pointer, const FVeyraTetherAbilityTuning& Tether)
		{
			CheckCast(Pointer + TEXT("/cast"), Tether.Cast);
			CheckTargetKinds(Pointer + TEXT("/targetKinds"), Tether.TargetKinds);
			CheckStatusIds(Pointer + TEXT("/targetStatuses"), Tether.TargetStatuses);
			if (!(Tether.MaxRange > 0.0) || !(Tether.DurationSeconds > 0.0))
			{
				Problem(Pointer, TEXT("maxRange and durationSeconds are above 0"));
			}
			if (Tether.MaxRange < Tether.Cast.CastRange)
			{
				Problem(Pointer + TEXT("/maxRange"), TEXT("is at least the cast's range, or the tether would stretch as it lands"));
			}
			if (Tether.SnapDistance > 0.0 ? !(Tether.SnapSpeed > 0.0) : Tether.SnapSpeed != 0.0)
			{
				Problem(Pointer + TEXT("/snapSpeed"), TEXT("is above 0 with a snap, and 0 without one"));
			}
		}

		void CheckAmbush(const FString& Pointer, const FVeyraAmbushAbilityTuning& Ambush)
		{
			CheckCast(Pointer + TEXT("/cast"), Ambush.Cast);
			CheckStatusIds(Pointer + TEXT("/vanishStatuses"), Ambush.VanishStatuses);
			CheckEffects(Pointer + TEXT("/effects"), Ambush.Effects);
			if (!(Ambush.Cast.CastRange > 0.0) || !(Ambush.RecentSeconds > 0.0) || !(Ambush.VanishSeconds > 0.0) || !(Ambush.BesideDistance >= 0.0))
			{
				Problem(Pointer, TEXT("an ambush has a cast range, a recent window and a vanish above 0, and a distance beside its target of at least 0"));
			}
		}

		void CheckStance(const FString& Pointer, const FVeyraContentId& Id, const FVeyraStanceAbilityTuning& Stance)
		{
			CheckCast(Pointer + TEXT("/cast"), Stance.Cast);
			if (Stance.Slots.IsEmpty())
			{
				Problem(Pointer + TEXT("/slots"), TEXT("holds at least one slot"));
			}
			TArray<EVeyraAbilitySlot, TInlineAllocator<4>> Held;
			for (int32 Index = 0; Index < Stance.Slots.Num(); ++Index)
			{
				const FVeyraStanceSlotTuning& Slot = Stance.Slots[Index];
				const FString SlotPointer = FString::Printf(TEXT("%s/slots/%d"), *Pointer, Index);
				// It takes the place of an ability of the kit's, its ultimate too (ADR-035 §1); the Vanguard's rules keep
				// the stance's own slot its own.
				if (Slot.Slot != EVeyraAbilitySlot::Q && Slot.Slot != EVeyraAbilitySlot::W && Slot.Slot != EVeyraAbilitySlot::E && Slot.Slot != EVeyraAbilitySlot::R)
				{
					Problem(SlotPointer + TEXT("/slot"), TEXT("must be Q, W, E or R: a stance holds the slots of its kit's abilities"));
				}
				if (Held.Contains(Slot.Slot))
				{
					Problem(SlotPointer + TEXT("/slot"), TEXT("is already held; a stance holds each slot once"));
				}
				Held.Add(Slot.Slot);
				if (Slot.Ability == Id || !Defines(Tuning, Slot.Ability))
				{
					Problem(SlotPointer + TEXT("/ability"), FString::Printf(TEXT("names ability \"%s\", which is the stance itself or no archetype defines"),
						*Slot.Ability.ToString()));
				}
			}
		}

		/** Whether Ability leaves a marker of its caster's: a placement, or a self-buff with one (ADR-031 §4). */
		bool LeavesMarker(const FVeyraContentId& Ability) const
		{
			const FVeyraSelfBuffAbilityTuning* Buff = Tuning.SelfBuff.Find(Ability);
			return Tuning.Placement.Contains(Ability) || (Buff && !Buff->Marker.IsEmpty());
		}

		void CheckPlacement(const FString& Pointer, const FVeyraPlacementAbilityTuning& Placement)
		{
			CheckCast(Pointer + TEXT("/cast"), Placement.Cast);
			if (!(Placement.Cast.CastRange > 0.0) || !(Placement.Marker.LifetimeSeconds > 0.0) || Placement.Marker.HitsToDestroy < 0)
			{
				Problem(Pointer, TEXT("a placement has a cast range and a marker lifetime above 0, and hits to destroy of at least 0"));
			}
			if (!Placement.Marker.BurstZones.IsEmpty())
			{
				Problem(Pointer + TEXT("/marker/burstZones"), TEXT("must be empty: a placed marker bursts with nothing"));
			}
		}

		void CheckBlink(const FString& Pointer, const FVeyraBlinkAbilityTuning& Blink)
		{
			CheckCast(Pointer + TEXT("/cast"), Blink.Cast);
			CheckEffects(Pointer + TEXT("/effects"), Blink.Effects);
			CheckZones(Pointer + TEXT("/departureZones"), Blink.DepartureZones);
			CheckZones(Pointer + TEXT("/companionDepartureZones"), Blink.CompanionDepartureZones);
			// To its own companion: no enemy, no marker; a swap with it may erupt where the companion left (ADR-034 §5).
			if (Blink.To == EVeyraBlinkTo::OwnCompanion)
			{
				if (!Blink.MarkerAbility.IsEmpty() || !Blink.TargetKinds.IsEmpty() || !Blink.Effects.Damage.IsEmpty() || !Blink.Effects.Statuses.IsEmpty()
					|| Blink.Cast.NeedsCompanion != EVeyraCompanionNeed::Living)
				{
					Problem(Pointer, TEXT("a blink to its own companion names no marker or target kinds, lands no effects, and needs its living companion"));
				}
				if (!Blink.CompanionDepartureZones.IsEmpty() && Blink.Swap != EVeyraBlinkSwap::Swap)
				{
					Problem(Pointer + TEXT("/companionDepartureZones"), TEXT("erupt where the companion left, so the blink swaps with it"));
				}
				return;
			}
			if (!Blink.CompanionDepartureZones.IsEmpty())
			{
				Problem(Pointer + TEXT("/companionDepartureZones"), TEXT("only a swap with the caster's companion has them"));
			}
			const bool bToEnemy = Blink.To != EVeyraBlinkTo::OwnMarker;
			const bool bToMarker = Blink.To != EVeyraBlinkTo::EnemyUnit;
			if (bToMarker && (Blink.MarkerAbility.IsEmpty() || !LeavesMarker(Blink.MarkerAbility[0])))
			{
				Problem(Pointer + TEXT("/markerAbility"), TEXT("names one ability that leaves a marker, a placement or a self-buff with one, for a blink to it"));
			}
			if (!bToMarker && !Blink.MarkerAbility.IsEmpty())
			{
				Problem(Pointer + TEXT("/markerAbility"), TEXT("must be empty for a blink only to an enemy"));
			}
			if (bToEnemy && !(Blink.Cast.CastRange > 0.0))
			{
				Problem(Pointer + TEXT("/cast/castRange"), TEXT("must be above 0 for a blink that may name an enemy"));
			}
			if (!bToEnemy && (!Blink.TargetKinds.IsEmpty() || !Blink.Effects.Damage.IsEmpty() || !Blink.Effects.Statuses.IsEmpty()))
			{
				Problem(Pointer, TEXT("a blink only to its own marker names no target kinds and lands no effects"));
			}
			if (Blink.Swap == EVeyraBlinkSwap::Swap && !bToMarker)
			{
				Problem(Pointer + TEXT("/swap"), TEXT("must be None for a blink only to an enemy: there is no marker to swap with"));
			}
			if (!(Blink.BesideDistance >= 0.0))
			{
				Problem(Pointer + TEXT("/besideDistance"), TEXT("must be at least 0"));
			}
		}

		void CheckCompanion(const FString& Pointer, const FVeyraCompanionTuning& Companion)
		{
			for (const FString& Each : VeyraBasicAttacks::Validate(Companion.BasicAttack))
			{
				Problem(Pointer + TEXT("/basicAttack"), Each);
			}
			// It fights, follows and holds within its leash, so the leash reaches past where it keeps.
			if (!(Companion.LeashRange > Companion.FollowDistance))
			{
				Problem(Pointer + TEXT("/leashRange"), TEXT("must be beyond followDistance"));
			}
			if (!(Companion.AcquireRange >= Companion.BasicAttack.Range))
			{
				Problem(Pointer + TEXT("/acquireRange"), TEXT("must reach at least as far as its basic attack"));
			}
			if (Companion.Stats.MaxResource != 0.0 || Companion.Growth.MaxResource != 0.0)
			{
				Problem(Pointer + TEXT("/stats/maxResource"), TEXT("a companion has no resource"));
			}
			// Its body forms only with these, so a definition without them could never form.
			if (!(Companion.Stats.MaxHealth > 0.0) || !(Companion.Stats.AttackSpeed > 0.0) || !(Companion.Stats.MoveSpeed > 0.0))
			{
				Problem(Pointer + TEXT("/stats"), TEXT("maxHealth, attackSpeed and moveSpeed are above 0"));
			}
			// An escort's pulse and an attack's statuses (ADR-035 §5).
			if (Companion.Escort.Num() > 1)
			{
				Problem(Pointer + TEXT("/escort"), TEXT("holds at most one"));
			}
			for (int32 Index = 0; Index < Companion.Escort.Num(); ++Index)
			{
				const FVeyraEscortTuning& Escort = Companion.Escort[Index];
				const FString EscortPointer = FString::Printf(TEXT("%s/escort/%d"), *Pointer, Index);
				if (!(Escort.PulseSeconds > 0.0) || Escort.HealAmount < 0.0 || Escort.HealMagicPowerRatio < 0.0)
				{
					Problem(EscortPointer, TEXT("pulseSeconds is above 0, and healAmount and healMagicPowerRatio at least 0"));
				}
				CheckStatusIds(EscortPointer + TEXT("/statuses"), Escort.Statuses);
			}
			CheckStatusIds(Pointer + TEXT("/attackStatuses"), Companion.AttackStatuses);
		}

		void CheckRide(const FString& Pointer, const FVeyraRideAbilityTuning& Ride)
		{
			CheckCast(Pointer + TEXT("/cast"), Ride.Cast);
			CheckStatusIds(Pointer + TEXT("/riderStatuses"), Ride.RiderStatuses);
			if (!(Ride.SetSpeed > 0.0) || !(Ride.TurnRateDegreesPerSecond > 0.0) || !(Ride.DurationSeconds > 0.0) || Ride.DecaySeconds < 0.0)
			{
				Problem(Pointer, TEXT("setSpeed, turnRateDegreesPerSecond and durationSeconds are above 0, and decaySeconds at least 0"));
			}
			TArray<EVeyraAbilitySlot> Slots;
			for (int32 Index = 0; Index < Ride.Mounted.Num(); ++Index)
			{
				const FVeyraRideSlotTuning& Mounted = Ride.Mounted[Index];
				const FString MountedPointer = FString::Printf(TEXT("%s/mounted/%d"), *Pointer, Index);
				const bool bBasic = Mounted.Slot == EVeyraAbilitySlot::Q || Mounted.Slot == EVeyraAbilitySlot::W || Mounted.Slot == EVeyraAbilitySlot::E;
				if (!bBasic || Slots.Contains(Mounted.Slot))
				{
					Problem(MountedPointer + TEXT("/slot"), TEXT("a basic ability's slot, Q, W or E, once each (Combat Bible §56)"));
				}
				Slots.Add(Mounted.Slot);
				if (!Defines(Tuning, Mounted.Ability))
				{
					Problem(MountedPointer + TEXT("/ability"), FString::Printf(TEXT("names ability \"%s\", which no archetype defines"), *Mounted.Ability.ToString()));
				}
			}
			if (Ride.Vehicle.Num() > 1)
			{
				Problem(Pointer + TEXT("/vehicle"), TEXT("holds at most one"));
			}
			// A recast that fires at expiry ends the ride itself, so it must be a dash that leaves it.
			for (const FVeyraRecastTuning& Recast : Ride.Cast.RecastWindow)
			{
				const FVeyraDashAbilityTuning* Exit = Tuning.Dash.Find(Recast.Ability);
				if (Recast.OnExpiry == EVeyraRecastExpiry::Cast && (!Exit || Exit->RideExit != EVeyraRideExit::Leave))
				{
					Problem(Pointer + TEXT("/cast/recastWindow"), TEXT("a recast that fires at expiry is a dash that leaves the ride (rideExit: Leave)"));
				}
			}
			for (const FVeyraContentId& Vehicle : Ride.Vehicle)
			{
				if (!Tuning.Skillshot.Contains(Vehicle))
				{
					Problem(Pointer + TEXT("/vehicle"), FString::Printf(TEXT("names \"%s\", which /skillshot does not define"), *Vehicle.ToString()));
				}
			}
			CheckZones(Pointer + TEXT("/crashZones"), Ride.CrashZones);
			// What its body meets, and what it lays along its path (ADR-035 §6).
			if (Ride.Contact.Num() > 1 || Ride.Trail.Num() > 1)
			{
				Problem(Pointer, TEXT("holds at most one contact and one trail"));
			}
			for (int32 Index = 0; Index < Ride.Contact.Num(); ++Index)
			{
				const FVeyraRideContactTuning& Contact = Ride.Contact[Index];
				const FString ContactPointer = FString::Printf(TEXT("%s/contact/%d"), *Pointer, Index);
				if (Contact.Reach < 0.0 || !(Contact.PulseSeconds > 0.0))
				{
					Problem(ContactPointer, TEXT("reach is at least 0 and pulseSeconds above 0"));
				}
				CheckEffects(ContactPointer + TEXT("/enemyEffects"), Contact.EnemyEffects);
				CheckTargetKinds(ContactPointer + TEXT("/enemyKinds"), Contact.EnemyKinds);
				CheckAllyEffects(ContactPointer + TEXT("/allyEffects"), Contact.AllyEffects);
			}
			for (int32 Index = 0; Index < Ride.Trail.Num(); ++Index)
			{
				const FVeyraRideTrailTuning& Trail = Ride.Trail[Index];
				const FString TrailPointer = FString::Printf(TEXT("%s/trail/%d"), *Pointer, Index);
				// It looks at least once per spacing at its rider's set speed, so one area a look keeps pace.
				if (!(Trail.Spacing > 0.0) || !(Trail.PulseSeconds > 0.0) || Trail.PulseSeconds * Ride.SetSpeed > Trail.Spacing)
				{
					Problem(TrailPointer, TEXT("spacing and pulseSeconds are above 0, and a pulse at the ride's set speed covers no more than its spacing"));
				}
				const FVeyraAreaAbilityTuning* Area = Tuning.Area.Find(Trail.Area);
				if (!Area || Area->Linger.IsEmpty() || Area->DelaySeconds > 0.0 || Area->ChannelTicks > 1)
				{
					Problem(TrailPointer + TEXT("/area"), FString::Printf(TEXT("names \"%s\": an area ability that lands at once and lingers"), *Trail.Area.ToString()));
				}
			}
		}

		void CheckAttach(const FString& Pointer, const FVeyraAttachAbilityTuning& Attach)
		{
			CheckCast(Pointer + TEXT("/cast"), Attach.Cast);
			CheckTargetKinds(Pointer + TEXT("/targetKinds"), Attach.TargetKinds);
			CheckStatusIds(Pointer + TEXT("/hostStatuses"), Attach.HostStatuses);
			CheckEffects(Pointer + TEXT("/hostEffects"), Attach.HostEffects);
			if (!(Attach.LeapSpeed > 0.0) || !(Attach.AttachSeconds > 0.0))
			{
				Problem(Pointer, TEXT("leapSpeed and attachSeconds are above 0"));
			}
		}

		void CheckVolley(const FString& Pointer, const FVeyraVolleyAbilityTuning& Volley)
		{
			CheckCast(Pointer + TEXT("/cast"), Volley.Cast);
			if (!Tuning.Skillshot.Contains(Volley.Shot))
			{
				Problem(Pointer + TEXT("/shot"), FString::Printf(TEXT("names \"%s\", which /skillshot does not define"), *Volley.Shot.ToString()));
			}
			if (Volley.Shots < 1)
			{
				Problem(Pointer + TEXT("/shots"), TEXT("must be at least 1"));
			}
			if (!(Volley.DurationSeconds > 0.0) || !(Volley.LaneHalfAngleDegrees >= 0.0) || Volley.LaneHalfAngleDegrees > 180.0)
			{
				Problem(Pointer, TEXT("durationSeconds is above 0, and laneHalfAngleDegrees within [0, 180]"));
			}
			CheckStatusIds(Pointer + TEXT("/casterStatuses"), Volley.CasterStatuses);
			if (Volley.Bonus.Num() > 1)
			{
				Problem(Pointer + TEXT("/bonus"), TEXT("holds at most one"));
			}
			for (int32 Index = 0; Index < Volley.Bonus.Num(); ++Index)
			{
				CheckStatusIds(FString::Printf(TEXT("%s/bonus/%d/status"), *Pointer, Index), { Volley.Bonus[Index].Status });
				if (Volley.Bonus[Index].MaxShots < 1)
				{
					Problem(FString::Printf(TEXT("%s/bonus/%d/maxShots"), *Pointer, Index), TEXT("must be at least 1"));
				}
			}
		}

		void CheckEmpoweredAttack(const FString& Pointer, const FVeyraEmpoweredAttackAbilityTuning& Empowered)
		{
			CheckCast(Pointer + TEXT("/cast"), Empowered.Cast);
			if (Empowered.Attacks < 1)
			{
				Problem(Pointer + TEXT("/attacks"), TEXT("must be at least 1"));
			}
			if (!(Empowered.WindupScale > 0.0) || Empowered.WindupScale > 1.0)
			{
				Problem(Pointer + TEXT("/windupScale"), TEXT("must be above 0 and at most 1: an empowerment may quicken a windup, never slow it"));
			}
			CheckDamage(Pointer + TEXT("/damage"), Empowered.Damage);
			CheckStatusIds(Pointer + TEXT("/statuses"), Empowered.Statuses);
			CheckByRank(Pointer + TEXT("/armorPenetrationByRank"), Empowered.ArmorPenetrationByRank);
			if (Empowered.ArmorPenetrationByRank.ContainsByPredicate([](double Fraction) { return Fraction > 1.0; }))
			{
				Problem(Pointer + TEXT("/armorPenetrationByRank"), TEXT("each value is a fraction of Armor, at most 1"));
			}
			for (int32 Index = 0; Index < Empowered.Cleave.Num(); ++Index)
			{
				CheckStatusIds(FString::Printf(TEXT("%s/cleave/%d/statuses"), *Pointer, Index), Empowered.Cleave[Index].Statuses);
			}
			for (int32 Index = 0; Index < Empowered.SecondaryImpact.Num(); ++Index)
			{
				CheckSecondaryImpact(FString::Printf(TEXT("%s/secondaryImpact/%d"), *Pointer, Index), Empowered.SecondaryImpact[Index]);
			}
		}

		void CheckEachIdInOneArchetype()
		{
			TMap<FVeyraContentId, FString> Archetypes;
			const auto Note = [this, &Archetypes](const FVeyraContentId& Id, const TCHAR* Map) {
				if (const FString* Earlier = Archetypes.Find(Id))
				{
					Problem(FString::Printf(TEXT("/%s/%s"), Map, *Id.ToString()), FString::Printf(TEXT("is also in /%s; an ability has one archetype"), **Earlier));
				}
				else
				{
					Archetypes.Add(Id, Map);
				}
			};
			for (const TPair<FVeyraContentId, FVeyraTargetedDamageAbilityTuning>& Entry : Tuning.TargetedDamage)
			{
				Note(Entry.Key, TEXT("targetedDamage"));
			}
			for (const TPair<FVeyraContentId, FVeyraAreaAbilityTuning>& Entry : Tuning.Area)
			{
				Note(Entry.Key, TEXT("area"));
			}
			for (const TPair<FVeyraContentId, FVeyraSelfBuffAbilityTuning>& Entry : Tuning.SelfBuff)
			{
				Note(Entry.Key, TEXT("selfBuff"));
			}
			for (const TPair<FVeyraContentId, FVeyraSkillshotAbilityTuning>& Entry : Tuning.Skillshot)
			{
				Note(Entry.Key, TEXT("skillshot"));
			}
			for (const TPair<FVeyraContentId, FVeyraDashAbilityTuning>& Entry : Tuning.Dash)
			{
				Note(Entry.Key, TEXT("dash"));
			}
			for (const TPair<FVeyraContentId, FVeyraEmpoweredAttackAbilityTuning>& Entry : Tuning.EmpoweredAttack)
			{
				Note(Entry.Key, TEXT("empoweredAttack"));
			}
			for (const TPair<FVeyraContentId, FVeyraVolleyAbilityTuning>& Entry : Tuning.Volley)
			{
				Note(Entry.Key, TEXT("volley"));
			}
			for (const TPair<FVeyraContentId, FVeyraTetherAbilityTuning>& Entry : Tuning.Tether)
			{
				Note(Entry.Key, TEXT("tether"));
			}
			for (const TPair<FVeyraContentId, FVeyraAttachAbilityTuning>& Entry : Tuning.Attach)
			{
				Note(Entry.Key, TEXT("attach"));
			}
			for (const TPair<FVeyraContentId, FVeyraRideAbilityTuning>& Entry : Tuning.Ride)
			{
				Note(Entry.Key, TEXT("ride"));
			}
			for (const TPair<FVeyraContentId, FVeyraAmbushAbilityTuning>& Entry : Tuning.Ambush)
			{
				Note(Entry.Key, TEXT("ambush"));
			}
			for (const TPair<FVeyraContentId, FVeyraStanceAbilityTuning>& Entry : Tuning.Stance)
			{
				Note(Entry.Key, TEXT("stance"));
			}
			for (const TPair<FVeyraContentId, FVeyraPlacementAbilityTuning>& Entry : Tuning.Placement)
			{
				Note(Entry.Key, TEXT("placement"));
			}
			for (const TPair<FVeyraContentId, FVeyraBlinkAbilityTuning>& Entry : Tuning.Blink)
			{
				Note(Entry.Key, TEXT("blink"));
			}
			for (const TPair<FVeyraContentId, FVeyraCommandAbilityTuning>& Entry : Tuning.Command)
			{
				Note(Entry.Key, TEXT("command"));
			}
			for (const TPair<FVeyraContentId, FVeyraDismountAbilityTuning>& Entry : Tuning.Dismount)
			{
				Note(Entry.Key, TEXT("dismount"));
			}
		}
	};
}

double ValueAtRank(TConstArrayView<double> ByRank, int32 Rank)
{
	if (ByRank.Num() == 1)
	{
		return ByRank[0];
	}
	return ByRank.IsValidIndex(Rank - 1) ? ByRank[Rank - 1] : 0.0;
}

FVeyraStatusSpec ToStatusSpec(const FVeyraContentId& Id, const FVeyraStatusTuning& Status, int32 SourceLevel)
{
	FVeyraStatusSpec Spec;
	Spec.Id = Id;
	Spec.Kind = Status.Kind;
	Spec.Stacking = Status.Stacking;
	Spec.Magnitude = Status.Magnitude;
	Spec.DurationSeconds = Status.DurationSeconds;
	Spec.MaxStacks = Status.MaxStacks;
	Spec.StackDecaySeconds = Status.StackDecaySeconds;
	Spec.ArcDegrees = Status.ArcDegrees;
	Spec.UnitKinds = Status.UnitKinds;
	Spec.LandsOn = Status.LandsOn;
	Spec.TakedownExtensionSeconds = Status.TakedownExtensionSeconds;
	Spec.TakedownExtensionMaxSeconds = Status.TakedownExtensionMaxSeconds;
	Spec.AttackCharges = Status.AttackCharges;
	if (!Status.DamageOverTime.IsEmpty())
	{
		const FVeyraDamageOverTimeTuning& Ticks = Status.DamageOverTime[0];
		Spec.DamageType = Ticks.DamageType;
		Spec.TickSeconds = Ticks.TickSeconds;
		Spec.Magnitude = AtLevel(Status.Magnitude, Ticks.DamagePerLevel, SourceLevel);
	}
	return Spec;
}

double AtLevel(double Base, double PerLevel, int32 Level)
{
	return Base + PerLevel * FMath::Max(0, Level - 1);
}

TArray<FString> Validate(const FVeyraAbilitiesTuning& Tuning, TConstArrayView<int32> RankCounts)
{
	FAbilityTuningChecker Checker{ Tuning, RankCounts };
	Checker.CheckStatuses();
	for (const TPair<FVeyraContentId, FVeyraTargetedDamageAbilityTuning>& Entry : Tuning.TargetedDamage)
	{
		Checker.CheckTargetedDamage(TEXT("/targetedDamage/") + Entry.Key.ToString(), Entry.Value);
	}
	for (const TPair<FVeyraContentId, FVeyraAreaAbilityTuning>& Entry : Tuning.Area)
	{
		Checker.CheckArea(TEXT("/area/") + Entry.Key.ToString(), Entry.Value);
	}
	for (const TPair<FVeyraContentId, FVeyraSelfBuffAbilityTuning>& Entry : Tuning.SelfBuff)
	{
		Checker.CheckSelfBuff(TEXT("/selfBuff/") + Entry.Key.ToString(), Entry.Value);
	}
	for (const TPair<FVeyraContentId, FVeyraSkillshotAbilityTuning>& Entry : Tuning.Skillshot)
	{
		Checker.CheckSkillshot(TEXT("/skillshot/") + Entry.Key.ToString(), Entry.Value);
	}
	for (const TPair<FVeyraContentId, FVeyraDashAbilityTuning>& Entry : Tuning.Dash)
	{
		Checker.CheckDash(TEXT("/dash/") + Entry.Key.ToString(), Entry.Value);
	}
	for (const TPair<FVeyraContentId, FVeyraEmpoweredAttackAbilityTuning>& Entry : Tuning.EmpoweredAttack)
	{
		Checker.CheckEmpoweredAttack(TEXT("/empoweredAttack/") + Entry.Key.ToString(), Entry.Value);
	}
	for (const TPair<FVeyraContentId, FVeyraVolleyAbilityTuning>& Entry : Tuning.Volley)
	{
		Checker.CheckVolley(TEXT("/volley/") + Entry.Key.ToString(), Entry.Value);
	}
	for (const TPair<FVeyraContentId, FVeyraTetherAbilityTuning>& Entry : Tuning.Tether)
	{
		Checker.CheckTether(TEXT("/tether/") + Entry.Key.ToString(), Entry.Value);
	}
	for (const TPair<FVeyraContentId, FVeyraAttachAbilityTuning>& Entry : Tuning.Attach)
	{
		Checker.CheckAttach(TEXT("/attach/") + Entry.Key.ToString(), Entry.Value);
	}
	for (const TPair<FVeyraContentId, FVeyraRideAbilityTuning>& Entry : Tuning.Ride)
	{
		Checker.CheckRide(TEXT("/ride/") + Entry.Key.ToString(), Entry.Value);
	}
	for (const TPair<FVeyraContentId, FVeyraAmbushAbilityTuning>& Entry : Tuning.Ambush)
	{
		Checker.CheckAmbush(TEXT("/ambush/") + Entry.Key.ToString(), Entry.Value);
	}
	for (const TPair<FVeyraContentId, FVeyraStanceAbilityTuning>& Entry : Tuning.Stance)
	{
		Checker.CheckStance(TEXT("/stance/") + Entry.Key.ToString(), Entry.Key, Entry.Value);
	}
	for (const TPair<FVeyraContentId, FVeyraPlacementAbilityTuning>& Entry : Tuning.Placement)
	{
		Checker.CheckPlacement(TEXT("/placement/") + Entry.Key.ToString(), Entry.Value);
	}
	for (const TPair<FVeyraContentId, FVeyraBlinkAbilityTuning>& Entry : Tuning.Blink)
	{
		Checker.CheckBlink(TEXT("/blink/") + Entry.Key.ToString(), Entry.Value);
	}
	for (const TPair<FVeyraContentId, FVeyraCommandAbilityTuning>& Entry : Tuning.Command)
	{
		Checker.CheckCommand(TEXT("/command/") + Entry.Key.ToString(), Entry.Value);
	}
	for (const TPair<FVeyraContentId, FVeyraDismountAbilityTuning>& Entry : Tuning.Dismount)
	{
		Checker.CheckDismount(TEXT("/dismount/") + Entry.Key.ToString(), Entry.Value);
	}
	for (const TPair<FVeyraContentId, FVeyraCompanionTuning>& Entry : Tuning.Companions)
	{
		Checker.CheckCompanion(TEXT("/companions/") + Entry.Key.ToString(), Entry.Value);
	}
	Checker.CheckEachIdInOneArchetype();
	Checker.CheckFluxSpells();
	return Checker.Problems;
}

FVector DashHeading(const FVeyraDashAbilityTuning& Dash, const FVector& CastDirection)
{
	return Dash.Direction == EVeyraDashDirection::AwayFromPoint ? -CastDirection : CastDirection;
}

bool Defines(const FVeyraAbilitiesTuning& Tuning, const FVeyraContentId& Ability)
{
	return Tuning.TargetedDamage.Contains(Ability) || Tuning.Area.Contains(Ability) || Tuning.SelfBuff.Contains(Ability) || Tuning.Skillshot.Contains(Ability)
		|| Tuning.Dash.Contains(Ability) || Tuning.EmpoweredAttack.Contains(Ability) || Tuning.Volley.Contains(Ability)
		|| Tuning.Tether.Contains(Ability) || Tuning.Attach.Contains(Ability) || Tuning.Ride.Contains(Ability) || Tuning.Ambush.Contains(Ability)
		|| Tuning.Stance.Contains(Ability) || Tuning.Placement.Contains(Ability) || Tuning.Blink.Contains(Ability) || Tuning.Command.Contains(Ability)
		|| Tuning.Dismount.Contains(Ability);
}

const FVeyraCastTuning* FindCast(const FVeyraAbilitiesTuning& Tuning, const FVeyraContentId& Ability)
{
	const FVeyraCastTuning* Cast = nullptr;
	if (const FVeyraAreaAbilityTuning* Area = Tuning.Area.Find(Ability))
	{
		Cast = &Area->Cast;
	}
	else if (const FVeyraSelfBuffAbilityTuning* Buff = Tuning.SelfBuff.Find(Ability))
	{
		Cast = &Buff->Cast;
	}
	else if (const FVeyraSkillshotAbilityTuning* Skillshot = Tuning.Skillshot.Find(Ability))
	{
		Cast = &Skillshot->Cast;
	}
	else if (const FVeyraDashAbilityTuning* Dash = Tuning.Dash.Find(Ability))
	{
		Cast = &Dash->Cast;
	}
	else if (const FVeyraEmpoweredAttackAbilityTuning* Empowered = Tuning.EmpoweredAttack.Find(Ability))
	{
		Cast = &Empowered->Cast;
	}
	else if (const FVeyraVolleyAbilityTuning* Volley = Tuning.Volley.Find(Ability))
	{
		Cast = &Volley->Cast;
	}
	else if (const FVeyraTetherAbilityTuning* Tether = Tuning.Tether.Find(Ability))
	{
		Cast = &Tether->Cast;
	}
	else if (const FVeyraAttachAbilityTuning* Attach = Tuning.Attach.Find(Ability))
	{
		Cast = &Attach->Cast;
	}
	else if (const FVeyraRideAbilityTuning* Ride = Tuning.Ride.Find(Ability))
	{
		Cast = &Ride->Cast;
	}
	else if (const FVeyraAmbushAbilityTuning* Ambush = Tuning.Ambush.Find(Ability))
	{
		Cast = &Ambush->Cast;
	}
	else if (const FVeyraStanceAbilityTuning* Stance = Tuning.Stance.Find(Ability))
	{
		Cast = &Stance->Cast;
	}
	else if (const FVeyraPlacementAbilityTuning* Placement = Tuning.Placement.Find(Ability))
	{
		Cast = &Placement->Cast;
	}
	else if (const FVeyraBlinkAbilityTuning* Blink = Tuning.Blink.Find(Ability))
	{
		Cast = &Blink->Cast;
	}
	else if (const FVeyraCommandAbilityTuning* Command = Tuning.Command.Find(Ability))
	{
		Cast = &Command->Cast;
	}
	else if (const FVeyraDismountAbilityTuning* Dismount = Tuning.Dismount.Find(Ability))
	{
		Cast = &Dismount->Cast;
	}
	return Cast;
}

double ResourceCost(const FVeyraAbilitiesTuning& Tuning, const FVeyraContentId& Ability, int32 Rank)
{
	if (const FVeyraTargetedDamageAbilityTuning* Targeted = Tuning.TargetedDamage.Find(Ability))
	{
		return Targeted->ResourceCost;
	}
	const FVeyraCastTuning* Cast = FindCast(Tuning, Ability);
	return Cast ? ValueAtRank(Cast->ResourceCostByRank, Rank) : 0.0;
}

double CooldownSeconds(const FVeyraAbilitiesTuning& Tuning, const FVeyraContentId& Ability, int32 Rank)
{
	if (const FVeyraTargetedDamageAbilityTuning* Targeted = Tuning.TargetedDamage.Find(Ability))
	{
		return Targeted->CooldownSeconds;
	}
	const FVeyraCastTuning* Cast = FindCast(Tuning, Ability);
	return Cast ? ValueAtRank(Cast->CooldownSecondsByRank, Rank) : 0.0;
}

TArray<FString> ValidateRanks(const FVeyraAbilitiesTuning& Tuning, const FVeyraContentId& Ability, int32 RankCount)
{
	const int32 RankCounts[] = { RankCount };
	FAbilityTuningChecker Checker{ Tuning, RankCounts };
	const FString Key = Ability.ToString();
	if (const FVeyraAreaAbilityTuning* Area = Tuning.Area.Find(Ability))
	{
		Checker.CheckArea(TEXT("/area/") + Key, *Area);
	}
	if (const FVeyraSelfBuffAbilityTuning* Buff = Tuning.SelfBuff.Find(Ability))
	{
		Checker.CheckSelfBuff(TEXT("/selfBuff/") + Key, *Buff);
	}
	if (const FVeyraSkillshotAbilityTuning* Skillshot = Tuning.Skillshot.Find(Ability))
	{
		Checker.CheckSkillshot(TEXT("/skillshot/") + Key, *Skillshot);
	}
	if (const FVeyraDashAbilityTuning* Dash = Tuning.Dash.Find(Ability))
	{
		Checker.CheckDash(TEXT("/dash/") + Key, *Dash);
	}
	if (const FVeyraEmpoweredAttackAbilityTuning* Empowered = Tuning.EmpoweredAttack.Find(Ability))
	{
		Checker.CheckEmpoweredAttack(TEXT("/empoweredAttack/") + Key, *Empowered);
	}
	if (const FVeyraVolleyAbilityTuning* Volley = Tuning.Volley.Find(Ability))
	{
		Checker.CheckVolley(TEXT("/volley/") + Key, *Volley);
	}
	if (const FVeyraTetherAbilityTuning* Tether = Tuning.Tether.Find(Ability))
	{
		Checker.CheckTether(TEXT("/tether/") + Key, *Tether);
	}
	if (const FVeyraAttachAbilityTuning* Attach = Tuning.Attach.Find(Ability))
	{
		Checker.CheckAttach(TEXT("/attach/") + Key, *Attach);
	}
	if (const FVeyraRideAbilityTuning* Ride = Tuning.Ride.Find(Ability))
	{
		Checker.CheckRide(TEXT("/ride/") + Key, *Ride);
	}
	if (const FVeyraAmbushAbilityTuning* Ambush = Tuning.Ambush.Find(Ability))
	{
		Checker.CheckAmbush(TEXT("/ambush/") + Key, *Ambush);
	}
	if (const FVeyraStanceAbilityTuning* Stance = Tuning.Stance.Find(Ability))
	{
		Checker.CheckStance(TEXT("/stance/") + Key, Ability, *Stance);
	}
	if (const FVeyraPlacementAbilityTuning* Placement = Tuning.Placement.Find(Ability))
	{
		Checker.CheckPlacement(TEXT("/placement/") + Key, *Placement);
	}
	if (const FVeyraBlinkAbilityTuning* Blink = Tuning.Blink.Find(Ability))
	{
		Checker.CheckBlink(TEXT("/blink/") + Key, *Blink);
	}
	if (const FVeyraCommandAbilityTuning* Command = Tuning.Command.Find(Ability))
	{
		Checker.CheckCommand(TEXT("/command/") + Key, *Command);
	}
	if (const FVeyraDismountAbilityTuning* Dismount = Tuning.Dismount.Find(Ability))
	{
		Checker.CheckDismount(TEXT("/dismount/") + Key, *Dismount);
	}
	// Targeted damage abilities keep one value for every rank.
	return Checker.Problems;
}
}
