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
			}
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
			if (!Effects.MissingHealthDamage.IsEmpty() && Effects.Damage.IsEmpty())
			{
				Problem(Pointer + TEXT("/missingHealthDamage"), TEXT("joins the hit's damage, so the effects need damage too"));
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
			if (Area.Linger.Num() > 1)
			{
				Problem(Pointer + TEXT("/linger"), TEXT("holds at most one lingering area (ADR-018 §5)"));
			}
			for (int32 Index = 0; Index < Area.Linger.Num(); ++Index)
			{
				const FVeyraLingerTuning& Linger = Area.Linger[Index];
				const FString LingerPointer = FString::Printf(TEXT("%s/linger/%d"), *Pointer, Index);
				if (Area.DelaySeconds > 0.0 || Area.ChannelTicks > 1)
				{
					Problem(LingerPointer, TEXT("lingers after an area that hits at once, not a delayed or channelled one"));
				}
				if (!(Linger.DurationSeconds > 0.0) || !(Linger.PulseSeconds > 0.0) || Linger.PulseSeconds > Linger.DurationSeconds)
				{
					Problem(LingerPointer, TEXT("durationSeconds and pulseSeconds are above 0, and a pulse is no longer than the area lasts"));
				}
				CheckStatusIds(LingerPointer + TEXT("/casterStatuses"), Linger.CasterStatuses);
				CheckStatusIds(LingerPointer + TEXT("/allyStatuses"), Linger.AllyStatuses);
				CheckStatusIds(LingerPointer + TEXT("/enemyStatuses"), Linger.EnemyStatuses);
			}
		}

		void CheckSelfBuff(const FString& Pointer, const FVeyraSelfBuffAbilityTuning& Buff)
		{
			CheckCast(Pointer + TEXT("/cast"), Buff.Cast);
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
			}
			for (int32 Index = 0; Index < Buff.Heal.Num(); ++Index)
			{
				CheckStatusIds(FString::Printf(TEXT("%s/heal/%d/statuses"), *Pointer, Index), Buff.Heal[Index].Statuses);
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
			}
		}

		void CheckShield(const FString& Pointer, const FVeyraShieldTuning& Shield)
		{
			CheckByRank(Pointer + TEXT("/amountByRank"), Shield.AmountByRank);
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
			CheckCast(Pointer + TEXT("/cast"), Skillshot.Cast);
			CheckEffects(Pointer + TEXT("/effects"), Skillshot.Effects);
			CheckEffects(Pointer + TEXT("/passThroughEffects"), Skillshot.PassThroughEffects);
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
		}

		void CheckEmpoweredAttack(const FString& Pointer, const FVeyraEmpoweredAttackAbilityTuning& Empowered)
		{
			CheckCast(Pointer + TEXT("/cast"), Empowered.Cast);
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
				const FVeyraSecondaryImpactTuning& Impact = Empowered.SecondaryImpact[Index];
				const FString ImpactPointer = FString::Printf(TEXT("%s/secondaryImpact/%d"), *Pointer, Index);
				for (const FString& ShapeProblem : VeyraShapes::Validate(Impact.Shape))
				{
					Problem(ImpactPointer + TEXT("/shape"), ShapeProblem);
				}
				CheckDamage(ImpactPointer + TEXT("/damage"), Impact.Damage);
				CheckStatusIds(ImpactPointer + TEXT("/statuses"), Impact.Statuses);
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
	Spec.TakedownExtensionSeconds = Status.TakedownExtensionSeconds;
	Spec.TakedownExtensionMaxSeconds = Status.TakedownExtensionMaxSeconds;
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
		|| Tuning.Dash.Contains(Ability) || Tuning.EmpoweredAttack.Contains(Ability);
}

double CooldownSeconds(const FVeyraAbilitiesTuning& Tuning, const FVeyraContentId& Ability, int32 Rank)
{
	if (const FVeyraTargetedDamageAbilityTuning* Targeted = Tuning.TargetedDamage.Find(Ability))
	{
		return Targeted->CooldownSeconds;
	}
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
	// Targeted damage abilities keep one value for every rank.
	return Checker.Problems;
}
}
