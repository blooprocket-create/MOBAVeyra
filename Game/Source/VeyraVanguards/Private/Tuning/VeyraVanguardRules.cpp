// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Tuning/VeyraVanguardsTuning.h"

namespace VeyraVanguardRules
{
double DeadReckoningRatio(const FVeyraDeadReckoningTuning& Reckoning, double Banked)
{
	return Reckoning.StepUnits > 0.0 ? Reckoning.PhysicalPowerRatioPerStep * (Banked / Reckoning.StepUnits) : 0.0;
}

TArray<FString> Validate(const FVeyraVanguardsTuning& Tuning, const FVeyraAbilitiesTuning& Abilities, int32 BasicAbilityMaxRank, int32 UltimateMaxRank)
{
	TArray<FString> Problems;
	const auto Problem = [&Problems](const FString& Pointer, const FString& Message) { Problems.Add(FString::Printf(TEXT("%s: %s"), *Pointer, *Message)); };

	// A passive runs by the one passive map that defines it (ADR-008 §5).
	TMap<FVeyraContentId, FString> PassiveMaps;
	const auto RegisterPassive = [&PassiveMaps, &Problem](const FVeyraContentId& Id, const TCHAR* Map) {
		if (const FString* Earlier = PassiveMaps.Find(Id))
		{
			Problem(FString::Printf(TEXT("/%s/%s"), Map, *Id.ToString()), FString::Printf(TEXT("is also defined in /%s; a passive belongs to one passive map"), **Earlier));
			return;
		}
		PassiveMaps.Add(Id, Map);
	};
	for (const TPair<FVeyraContentId, FVeyraDeepFoundationTuning>& Entry : Tuning.DeepFoundation)
	{
		RegisterPassive(Entry.Key, TEXT("deepFoundation"));
	}
	for (const TPair<FVeyraContentId, FVeyraHitChainTuning>& Entry : Tuning.HitChain)
	{
		RegisterPassive(Entry.Key, TEXT("hitChain"));
	}
	for (const TPair<FVeyraContentId, FVeyraGatheringLightTuning>& Entry : Tuning.GatheringLight)
	{
		RegisterPassive(Entry.Key, TEXT("gatheringLight"));
	}
	for (const TPair<FVeyraContentId, FVeyraBreachTuning>& Entry : Tuning.Breach)
	{
		RegisterPassive(Entry.Key, TEXT("breach"));
	}
	for (const TPair<FVeyraContentId, FVeyraMomentumTuning>& Entry : Tuning.Momentum)
	{
		RegisterPassive(Entry.Key, TEXT("momentum"));
		const FString Pointer = TEXT("/momentum/") + Entry.Key.ToString();
		const FVeyraMomentumTuning& Momentum = Entry.Value;
		const FVeyraStatusTuning* Meter = Abilities.Statuses.Find(Momentum.Meter);
		if (!Meter || Meter->Kind != EVeyraStatusKind::Counter || Meter->Stacking != EVeyraStackingPolicy::Stacking || Meter->MaxStacks < 2)
		{
			Problem(Pointer + TEXT("/meter"), TEXT("names a Stacking Counter status of at least two stacks, which Abilities.json defines"));
		}
		TArray<FVeyraContentId> Named = Momentum.HoldFullStatuses;
		Named.Add(Momentum.Roadhouse.ReachStatus);
		for (const FVeyraContentId& Status : Named)
		{
			if (!Abilities.Statuses.Contains(Status))
			{
				Problem(Pointer, FString::Printf(TEXT("names status \"%s\", which Abilities.json does not define"), *Status.ToString()));
			}
		}
		TArray<EVeyraAbilitySlot> Slots;
		for (const FVeyraRedlinedTuning& Form : Momentum.Redlined)
		{
			const bool bBasic = Form.Slot == EVeyraAbilitySlot::Q || Form.Slot == EVeyraAbilitySlot::W || Form.Slot == EVeyraAbilitySlot::E;
			if (!bBasic || Slots.Contains(Form.Slot) || !VeyraAbilityRules::Defines(Abilities, Form.Ability))
			{
				Problem(Pointer + TEXT("/redlined"), TEXT("each form is a defined ability in a basic ability's slot, Q, W or E, once"));
			}
			Slots.Add(Form.Slot);
		}
		const FVeyraRoadhouseTuning& Roadhouse = Momentum.Roadhouse;
		if (!(Momentum.UnitsPerPoint > 0.0) || !(Momentum.SampleSeconds > 0.0) || Momentum.PointsPerAttack < 0 || Momentum.PointsPerCast < 0
			|| Roadhouse.Damage.AmountByRank.Num() != 1 || Roadhouse.MissingHealthRatio < 0.0 || Roadhouse.BonusHealthRatio < 0.0 || !(Roadhouse.LungeSpeed > 0.0))
		{
			Problem(Pointer, TEXT("unitsPerPoint, sampleSeconds and the lunge's speed are above 0; points and ratios at least 0; Roadhouse's damage one amount"));
		}
	}

	for (const TPair<FVeyraContentId, FVeyraCampRewardTuning>& Entry : Tuning.CampReward)
	{
		RegisterPassive(Entry.Key, TEXT("campReward"));
		const FString Pointer = TEXT("/campReward/") + Entry.Key.ToString();
		const FVeyraCampRewardTuning& Reward = Entry.Value;
		for (const FVeyraContentId& Status : Reward.Statuses)
		{
			if (!Abilities.Statuses.Contains(Status))
			{
				Problem(Pointer + TEXT("/statuses"), FString::Printf(TEXT("names status \"%s\", which Abilities.json does not define"), *Status.ToString()));
			}
		}
		if (Reward.HealthRatio < 0.0 || Reward.HealthAmount < 0.0 || Reward.TakedownCooldownSeconds < 0.0)
		{
			Problem(Pointer, TEXT("healthRatio, healthAmount and takedownCooldownSeconds are at least 0"));
		}
	}

	for (const TPair<FVeyraContentId, FVeyraHauntTuning>& Entry : Tuning.Haunt)
	{
		RegisterPassive(Entry.Key, TEXT("haunt"));
		const FString Pointer = TEXT("/haunt/") + Entry.Key.ToString();
		const FVeyraHauntTuning& Haunt = Entry.Value;
		TArray<FVeyraContentId> Named = Haunt.Statuses;
		Named.Add(Haunt.HauntStatus);
		for (const FVeyraContentId& Status : Named)
		{
			if (!Abilities.Statuses.Contains(Status))
			{
				Problem(Pointer, FString::Printf(TEXT("names status \"%s\", which Abilities.json does not define"), *Status.ToString()));
			}
		}
		if (Haunt.Damage.AmountByRank.Num() != 1)
		{
			Problem(Pointer + TEXT("/damage"), TEXT("a passive has no ranks: its damage has one amount"));
		}
		if (!(Haunt.AllyRadius > 0.0) || Haunt.PerEnemyCooldownSeconds < 0.0)
		{
			Problem(Pointer, TEXT("allyRadius is above 0 and perEnemyCooldownSeconds at least 0"));
		}
	}

	for (const TPair<FVeyraContentId, FVeyraMarkProcTuning>& Entry : Tuning.MarkProc)
	{
		RegisterPassive(Entry.Key, TEXT("markProc"));
		const FString Pointer = TEXT("/markProc/") + Entry.Key.ToString();
		const FVeyraMarkProcTuning& MarkProc = Entry.Value;
		const FVeyraStatusTuning* Mark = Abilities.Statuses.Find(MarkProc.Mark);
		if (!Mark)
		{
			Problem(Pointer + TEXT("/mark"), FString::Printf(TEXT("names status \"%s\", which Abilities.json does not define"), *MarkProc.Mark.ToString()));
		}
		else if (Mark->Stacking != EVeyraStackingPolicy::Stacking || Mark->MaxStacks < 2)
		{
			Problem(Pointer + TEXT("/mark"), TEXT("must be a Stacking status of at least two stacks, whose most prime the proc"));
		}
		if (MarkProc.ProcDamage.AmountByRank.Num() != 1 || MarkProc.ProcDamagePerLevel < 0.0 || MarkProc.Emergence.Num() > 1 || MarkProc.ProcBolts.Num() > 1)
		{
			Problem(Pointer, TEXT("procDamage has one amount and a per-Level amount of at least 0; emergence and procBolts hold at most one each"));
		}
		for (const FVeyraEmergenceTuning& Emergence : MarkProc.Emergence)
		{
			if (!Abilities.Statuses.Contains(Emergence.Status) || !(Emergence.WindowSeconds >= 0.0) || Emergence.BonusDamage.AmountByRank.Num() != 1)
			{
				Problem(Pointer + TEXT("/emergence/0"), TEXT("names a status Abilities.json defines, with a window of at least 0 and one amount"));
			}
		}
		for (const FVeyraProcBoltTuning& Bolt : MarkProc.ProcBolts)
		{
			if (!(Bolt.WindowSeconds > 0.0) || !(Bolt.Radius > 0.0) || Bolt.Damage.AmountByRank.Num() != 1 || !(Bolt.Projectile.Speed > 0.0))
			{
				Problem(Pointer + TEXT("/procBolts/0"), TEXT("its window and radius are above 0, its damage one amount, and its speed above 0"));
			}
		}
	}
	for (const TPair<FVeyraContentId, FVeyraCadenceTuning>& Entry : Tuning.Cadence)
	{
		RegisterPassive(Entry.Key, TEXT("cadence"));
		const FString Pointer = TEXT("/cadence/") + Entry.Key.ToString();
		const FVeyraCadenceTuning& Cadence = Entry.Value;
		for (const TPair<const TCHAR*, const FVeyraContentId*> Named : { TPair<const TCHAR*, const FVeyraContentId*>{ TEXT("status"), &Cadence.Status },
				 { TEXT("extraStackOn"), &Cadence.ExtraStackOn }, { TEXT("steadyStatus"), &Cadence.SteadyStatus }, { TEXT("fullStatus"), &Cadence.FullStatus } })
		{
			if (!Abilities.Statuses.Contains(*Named.Value))
			{
				Problem(Pointer + TEXT("/") + Named.Key, FString::Printf(TEXT("names status \"%s\", which Abilities.json does not define"), *Named.Value->ToString()));
			}
		}
		const FVeyraStatusTuning* Stacks = Abilities.Statuses.Find(Cadence.Status);
		if (Stacks && (Stacks->Kind != EVeyraStatusKind::AttackSpeed || Stacks->Stacking != EVeyraStackingPolicy::Stacking))
		{
			Problem(Pointer + TEXT("/status"), TEXT("must be a Stacking Attack Speed status"));
		}
		if (!(Cadence.SteadyDecayMultiplier >= 1.0) || !(Cadence.FiringLine.DelaySeconds >= 0.0) || !(Cadence.FiringLine.AttackDamageFraction >= 0.0)
			|| Cadence.FiringLine.Damage.AmountByRank.Num() != 1 || !(Cadence.FiringLine.Projectile.Speed > 0.0))
		{
			Problem(Pointer, TEXT("steadyDecayMultiplier is at least 1; the echo's delay and fraction at least 0, its damage one amount, and its speed above 0"));
		}
		if (Cadence.SpectralRank.EveryAttacks < 1)
		{
			Problem(Pointer + TEXT("/spectralRank/everyAttacks"), TEXT("must be at least 1"));
		}
		for (const FString& ShapeProblem : VeyraShapes::Validate(Cadence.SpectralRank.Shape))
		{
			Problem(Pointer + TEXT("/spectralRank/shape"), ShapeProblem);
		}
		for (const FVeyraDamageTuning& Damage : Cadence.SpectralRank.Damage)
		{
			if (Damage.AmountByRank.Num() != 1)
			{
				Problem(Pointer + TEXT("/spectralRank/damage"), TEXT("a passive has no ranks: one amount each"));
			}
		}
	}
	for (const TPair<FVeyraContentId, FVeyraMovingTargetTuning>& Entry : Tuning.MovingTarget)
	{
		RegisterPassive(Entry.Key, TEXT("movingTarget"));
		const FString Pointer = TEXT("/movingTarget/") + Entry.Key.ToString();
		const FVeyraMovingTargetTuning& Moving = Entry.Value;
		if (!Abilities.Statuses.Contains(Moving.TrackedStatus))
		{
			Problem(Pointer + TEXT("/trackedStatus"), FString::Printf(TEXT("names status \"%s\", which Abilities.json does not define"), *Moving.TrackedStatus.ToString()));
		}
		if (Moving.TrackedDamage.AmountByRank.Num() != 1 || Moving.DeadReckoning.Damage.AmountByRank.Num() != 1)
		{
			Problem(Pointer, TEXT("a passive has no ranks: trackedDamage and deadReckoning's damage each have one amount"));
		}
		const FVeyraDeadReckoningTuning& Reckoning = Moving.DeadReckoning;
		if (!(Reckoning.ThresholdUnits > 0.0) || Reckoning.CapUnits < Reckoning.ThresholdUnits || Reckoning.PhysicalPowerRatioPerStep < 0.0
			|| !(Reckoning.StepUnits > 0.0))
		{
			Problem(Pointer + TEXT("/deadReckoning"), TEXT("thresholdUnits is above 0, capUnits at least thresholdUnits, the ratio at least 0, and stepUnits above 0"));
		}
	}

	for (const TPair<FVeyraContentId, FVeyraVanguardDefinition>& Entry : Tuning.Vanguards)
	{
		const FString Pointer = TEXT("/vanguards/") + Entry.Key.ToString();
		const FVeyraVanguardDefinition& Vanguard = Entry.Value;
		// A capsule's half height includes its rounded ends, so it can never be shorter than its radius.
		if (Vanguard.Body.CapsuleHalfHeight < Vanguard.Body.CapsuleRadius)
		{
			Problem(Pointer + TEXT("/body/capsuleHalfHeight"), TEXT("must be at least capsuleRadius"));
		}
		for (const FString& AttackProblem : VeyraBasicAttacks::Validate(Vanguard.BasicAttack))
		{
			Problem(Pointer + TEXT("/basicAttack"), AttackProblem);
		}

		struct FSlot
		{
			const TCHAR* Name;
			const TArray<FVeyraContentId>* Ids;
			int32 RankCount;
		};
		const FSlot Slots[] = { { TEXT("q"), &Vanguard.Abilities.Q, BasicAbilityMaxRank }, { TEXT("w"), &Vanguard.Abilities.W, BasicAbilityMaxRank },
			{ TEXT("e"), &Vanguard.Abilities.E, BasicAbilityMaxRank }, { TEXT("r"), &Vanguard.Abilities.R, UltimateMaxRank } };
		TArray<FVeyraContentId, TInlineAllocator<4>> Kit;
		for (const FSlot& Slot : Slots)
		{
			for (int32 Index = 0; Index < Slot.Ids->Num(); ++Index)
			{
				const FVeyraContentId& Ability = (*Slot.Ids)[Index];
				const FString SlotPointer = FString::Printf(TEXT("%s/abilities/%s/%d"), *Pointer, Slot.Name, Index);
				if (!VeyraAbilityRules::Defines(Abilities, Ability))
				{
					Problem(SlotPointer, FString::Printf(TEXT("names ability \"%s\", which Abilities.json does not define"), *Ability.ToString()));
					continue;
				}
				for (const FString& RankProblem : VeyraAbilityRules::ValidateRanks(Abilities, Ability, Slot.RankCount))
				{
					Problem(SlotPointer, TEXT("in this slot, ") + RankProblem);
				}
				// A cast finds its rank by its ability, so one ability fills one slot.
				if (Kit.Contains(Ability))
				{
					Problem(SlotPointer, TEXT("is already in another slot; each slot has its own ability"));
				}
				Kit.Add(Ability);
			}
		}

		for (int32 Index = 0; Index < Vanguard.Passive.Num(); ++Index)
		{
			if (!PassiveMaps.Contains(Vanguard.Passive[Index]))
			{
				Problem(FString::Printf(TEXT("%s/passive/%d"), *Pointer, Index),
					FString::Printf(TEXT("names passive \"%s\", which no passive map defines"), *Vanguard.Passive[Index].ToString()));
			}
		}
	}

	for (const TPair<FVeyraContentId, FVeyraDeepFoundationTuning>& Entry : Tuning.DeepFoundation)
	{
		// Passives have no ranks.
		if (Entry.Value.Shield.AmountByRank.Num() != 1)
		{
			Problem(FString::Printf(TEXT("/deepFoundation/%s/shield/amountByRank"), *Entry.Key.ToString()), TEXT("holds exactly one value: a passive has no ranks"));
		}
	}

	for (const TPair<FVeyraContentId, FVeyraHitChainTuning>& Entry : Tuning.HitChain)
	{
		const FString Pointer = TEXT("/hitChain/") + Entry.Key.ToString();
		if (!Abilities.Statuses.Contains(Entry.Value.Status))
		{
			Problem(Pointer + TEXT("/status"), FString::Printf(TEXT("names status \"%s\", which Abilities.json does not define"), *Entry.Value.Status.ToString()));
		}
	}

	for (const TPair<FVeyraContentId, FVeyraGatheringLightTuning>& Entry : Tuning.GatheringLight)
	{
		if (Entry.Value.FragmentDamage.AmountByRank.Num() != 1)
		{
			Problem(FString::Printf(TEXT("/gatheringLight/%s/fragmentDamage/amountByRank"), *Entry.Key.ToString()), TEXT("holds exactly one value: a passive has no ranks"));
		}
	}

	for (const TPair<FVeyraContentId, FVeyraBreachTuning>& Entry : Tuning.Breach)
	{
		const FString Pointer = TEXT("/breach/") + Entry.Key.ToString();
		const FVeyraBreachTuning& Breach = Entry.Value;
		if (Breach.BonusDamage.AmountByRank.Num() != 1)
		{
			Problem(Pointer + TEXT("/bonusDamage/amountByRank"), TEXT("holds exactly one value: a passive has no ranks"));
		}
		for (int32 Index = 0; Index < Breach.Impact.Damage.Num(); ++Index)
		{
			if (Breach.Impact.Damage[Index].AmountByRank.Num() != 1)
			{
				Problem(FString::Printf(TEXT("%s/impact/damage/%d/amountByRank"), *Pointer, Index), TEXT("holds exactly one value: a passive has no ranks"));
			}
		}
		for (const FString& ShapeProblem : VeyraShapes::Validate(Breach.Impact.Shape))
		{
			Problem(Pointer + TEXT("/impact/shape"), ShapeProblem);
		}
		for (int32 Index = 0; Index < Breach.Impact.Statuses.Num(); ++Index)
		{
			if (!Abilities.Statuses.Contains(Breach.Impact.Statuses[Index]))
			{
				Problem(FString::Printf(TEXT("%s/impact/statuses/%d"), *Pointer, Index),
					FString::Printf(TEXT("names status \"%s\", which Abilities.json does not define"), *Breach.Impact.Statuses[Index].ToString()));
			}
		}
	}
	return Problems;
}
}
