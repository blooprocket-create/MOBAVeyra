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

		void CheckEffects(const FString& Pointer, const FVeyraEffectBundleTuning& Effects)
		{
			TArray<EVeyraDamageType, TInlineAllocator<3>> Types;
			for (int32 Index = 0; Index < Effects.Damage.Num(); ++Index)
			{
				const FVeyraDamageTuning& Damage = Effects.Damage[Index];
				const FString DamagePointer = FString::Printf(TEXT("%s/damage/%d"), *Pointer, Index);
				CheckByRank(DamagePointer + TEXT("/amountByRank"), Damage.AmountByRank);
				if (Types.Contains(Damage.Type))
				{
					Problem(DamagePointer + TEXT("/type"), TEXT("repeats a damage type; one event has one component per type (Combat Bible §25)"));
				}
				Types.Add(Damage.Type);
			}
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
			for (int32 Index = 0; Index < Area.Zones.Num(); ++Index)
			{
				const FString ZonePointer = FString::Printf(TEXT("%s/zones/%d"), *Pointer, Index);
				for (const FString& ShapeProblem : VeyraShapes::Validate(Area.Zones[Index].Shape))
				{
					Problem(ZonePointer + TEXT("/shape"), ShapeProblem);
				}
				CheckEffects(ZonePointer + TEXT("/effects"), Area.Zones[Index].Effects);
			}
		}

		void CheckSelfBuff(const FString& Pointer, const FVeyraSelfBuffAbilityTuning& Buff)
		{
			CheckCast(Pointer + TEXT("/cast"), Buff.Cast);
			CheckStatusIds(Pointer + TEXT("/statuses"), Buff.Statuses);
			for (int32 Index = 0; Index < Buff.Shields.Num(); ++Index)
			{
				CheckByRank(FString::Printf(TEXT("%s/shields/%d/amountByRank"), *Pointer, Index), Buff.Shields[Index].AmountByRank);
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
	Checker.CheckEachIdInOneArchetype();
	return Checker.Problems;
}
}
