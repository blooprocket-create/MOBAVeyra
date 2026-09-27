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
		}

		void CheckStatuses()
		{
			for (const TPair<FVeyraContentId, FVeyraStatusTuning>& Status : Tuning.Statuses)
			{
				for (const FString& StatusProblem : VeyraStatuses::Validate(ToStatusSpec(Status.Key, Status.Value)))
				{
					Problem(FString::Printf(TEXT("/statuses/%s"), *Status.Key.ToString()), StatusProblem);
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
			CheckZones(Pointer + TEXT("/zones"), Area.Zones);
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

FVeyraStatusSpec ToStatusSpec(const FVeyraContentId& Id, const FVeyraStatusTuning& Status)
{
	FVeyraStatusSpec Spec;
	Spec.Id = Id;
	Spec.Kind = Status.Kind;
	Spec.Stacking = Status.Stacking;
	Spec.Magnitude = Status.Magnitude;
	Spec.DurationSeconds = Status.DurationSeconds;
	Spec.MaxStacks = Status.MaxStacks;
	Spec.TakedownExtensionSeconds = Status.TakedownExtensionSeconds;
	Spec.TakedownExtensionMaxSeconds = Status.TakedownExtensionMaxSeconds;
	return Spec;
}

TArray<FString> Validate(const FVeyraAbilitiesTuning& Tuning, TConstArrayView<int32> RankCounts)
{
	FAbilityTuningChecker Checker{ Tuning, RankCounts };
	Checker.CheckStatuses();
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
