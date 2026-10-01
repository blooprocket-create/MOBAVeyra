// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Tuning/VeyraVanguardsTuning.h"

#include "Progression/VeyraProgressionRules.h"

namespace VeyraVanguardRules
{
double DeadReckoningRatio(const FVeyraDeadReckoningTuning& Reckoning, double Banked)
{
	return Reckoning.StepUnits > 0.0 ? Reckoning.PhysicalPowerRatioPerStep * (Banked / Reckoning.StepUnits) : 0.0;
}

FVeyraRankShape RankShapeOf(const FVeyraVanguardDefinition& Vanguard, const FVeyraProgressionTuning& Progression)
{
	const TOptional<FVeyraRankShape> Shape = Vanguard.RankShape.IsEmpty() ? TOptional<FVeyraRankShape>() : VeyraProgression::FindShape(Progression, Vanguard.RankShape[0]);
	return Shape.IsSet() ? Shape.GetValue() : VeyraProgression::StandardShape(Progression);
}

TArray<FString> Validate(const FVeyraVanguardsTuning& Tuning, const FVeyraAbilitiesTuning& Abilities, const FVeyraProgressionTuning& Progression)
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

	for (const TPair<FVeyraContentId, FVeyraWildDominionTuning>& Entry : Tuning.WildDominion)
	{
		RegisterPassive(Entry.Key, TEXT("wildDominion"));
		const FString Pointer = TEXT("/wildDominion/") + Entry.Key.ToString();
		const FVeyraWildDominionTuning& Dominion = Entry.Value;
		// Each status outlasts a check, so it holds while its owner stays in the jungle and lapses soon after it leaves.
		for (const FVeyraContentId& Status : Dominion.Statuses)
		{
			const FVeyraStatusTuning* Given = Abilities.Statuses.Find(Status);
			if (!Given || !(Given->DurationSeconds > Dominion.CheckSeconds))
			{
				Problem(Pointer + TEXT("/statuses"), FString::Printf(TEXT("names status \"%s\", which Abilities.json must define lasting longer than a check"), *Status.ToString()));
			}
		}
		if (!(Dominion.CheckSeconds > 0.0) || Dominion.WildlifeHealFraction < 0.0 || Dominion.WildlifeHealFraction > 1.0)
		{
			Problem(Pointer, TEXT("checkSeconds is above 0 and wildlifeHealFraction from 0 to 1"));
		}
	}

	for (const TPair<FVeyraContentId, FVeyraKitStatusesTuning>& Entry : Tuning.KitStatuses)
	{
		RegisterPassive(Entry.Key, TEXT("kitStatuses"));
		const FString Pointer = TEXT("/kitStatuses/") + Entry.Key.ToString();
		if (Entry.Value.Statuses.IsEmpty())
		{
			Problem(Pointer + TEXT("/statuses"), TEXT("names at least one status: the passive is its kit's statuses"));
		}
		for (const FVeyraContentId& Status : Entry.Value.Statuses)
		{
			if (!Abilities.Statuses.Contains(Status))
			{
				Problem(Pointer + TEXT("/statuses"), FString::Printf(TEXT("names status \"%s\", which Abilities.json does not define"), *Status.ToString()));
			}
		}
	}

	for (const TPair<FVeyraContentId, FVeyraAttackStrideTuning>& Entry : Tuning.AttackStride)
	{
		RegisterPassive(Entry.Key, TEXT("attackStride"));
		const FString Pointer = TEXT("/attackStride/") + Entry.Key.ToString();
		if (!(Entry.Value.WindupShare > 0.0) || Entry.Value.WindupShare > 1.0)
		{
			Problem(Pointer + TEXT("/windupShare"), TEXT("must be above 0 and at most 1: a share of her speed"));
		}
		for (const FVeyraContentId& Status : Entry.Value.HitStatuses)
		{
			if (!Abilities.Statuses.Contains(Status))
			{
				Problem(Pointer + TEXT("/hitStatuses"), FString::Printf(TEXT("names status \"%s\", which Abilities.json does not define"), *Status.ToString()));
			}
		}
	}

	for (const TPair<FVeyraContentId, FVeyraSlipstreamTuning>& Entry : Tuning.Slipstream)
	{
		RegisterPassive(Entry.Key, TEXT("slipstream"));
		const FString Pointer = TEXT("/slipstream/") + Entry.Key.ToString();
		const FVeyraSlipstreamTuning& Slipstream = Entry.Value;
		if (!(Slipstream.Width > 0.0) || !(Slipstream.MaxLength > 0.0) || !(Slipstream.DurationSeconds > 0.0) || !(Slipstream.PulseSeconds > 0.0)
			|| Slipstream.PulseSeconds > Slipstream.DurationSeconds)
		{
			Problem(Pointer, TEXT("width, maxLength, durationSeconds and pulseSeconds are above 0, and a pulse is no longer than the current lasts"));
		}
		// Each status outlasts a pulse, so it holds while one stays inside.
		for (const FVeyraContentId& Status : Slipstream.Statuses)
		{
			const FVeyraStatusTuning* Given = Abilities.Statuses.Find(Status);
			if (!Given || !(Given->DurationSeconds > Slipstream.PulseSeconds))
			{
				Problem(Pointer + TEXT("/statuses"), FString::Printf(TEXT("names status \"%s\", which Abilities.json must define lasting longer than a pulse"), *Status.ToString()));
			}
		}
	}

	for (const TPair<FVeyraContentId, FVeyraReclaimTuning>& Entry : Tuning.Reclaim)
	{
		RegisterPassive(Entry.Key, TEXT("reclaim"));
		const FString Pointer = TEXT("/reclaim/") + Entry.Key.ToString();
		const FVeyraReclaimTuning& Reclaim = Entry.Value;
		if (!Abilities.Statuses.Contains(Reclaim.Mark))
		{
			Problem(Pointer + TEXT("/mark"), FString::Printf(TEXT("names status \"%s\", which Abilities.json does not define"), *Reclaim.Mark.ToString()));
		}
		if (Reclaim.HealAmount < 0.0 || Reclaim.HealPerLevel < 0.0 || Reclaim.MagicPowerRatio < 0.0 || !(Reclaim.LockoutSeconds > 0.0))
		{
			Problem(Pointer, TEXT("healAmount, healPerLevel and magicPowerRatio are at least 0, and lockoutSeconds above 0"));
		}
	}

	for (const TPair<FVeyraContentId, FVeyraQuarryTuning>& Entry : Tuning.Quarry)
	{
		RegisterPassive(Entry.Key, TEXT("quarry"));
		const FString Pointer = TEXT("/quarry/") + Entry.Key.ToString();
		const FVeyraQuarryTuning& Quarry = Entry.Value;
		for (const TPair<const TCHAR*, const FVeyraContentId*> Named : { TPair<const TCHAR*, const FVeyraContentId*>(TEXT("/mark"), &Quarry.Mark),
				 TPair<const TCHAR*, const FVeyraContentId*>(TEXT("/chaseStatus"), &Quarry.ChaseStatus) })
		{
			if (!Abilities.Statuses.Contains(*Named.Value))
			{
				Problem(Pointer + Named.Key, FString::Printf(TEXT("names status \"%s\", which Abilities.json does not define"), *Named.Value->ToString()));
			}
		}
		const FVeyraStatusTuning* Chase = Abilities.Statuses.Find(Quarry.ChaseStatus);
		if (Chase && Chase->Kind != EVeyraStatusKind::MoveSpeed)
		{
			Problem(Pointer + TEXT("/chaseStatus"), TEXT("is a MoveSpeed status"));
		}
		if (!(Quarry.ChaseRange > 0.0) || !(Quarry.ChaseAngleDegrees > 0.0 && Quarry.ChaseAngleDegrees <= 180.0) || !(Quarry.SampleSeconds > 0.0)
			|| Quarry.DamageAmount < 0.0 || Quarry.DamagePerLevel < 0.0 || Quarry.MagicPowerRatio < 0.0 || !(Quarry.CooldownRefund >= 0.0 && Quarry.CooldownRefund <= 1.0)
			|| !(Quarry.JumpRadius > 0.0))
		{
			Problem(Pointer, TEXT("chaseRange, sampleSeconds and jumpRadius are above 0, chaseAngleDegrees above 0 and at most 180, the damage values at least 0, and cooldownRefund from 0 to 1"));
		}
	}

	for (const TPair<FVeyraContentId, FVeyraDisciplinesTuning>& Entry : Tuning.Disciplines)
	{
		RegisterPassive(Entry.Key, TEXT("disciplines"));
		const FString Pointer = TEXT("/disciplines/") + Entry.Key.ToString();
		const FVeyraDisciplinesTuning& Disciplines = Entry.Value;
		const auto IsFraction = [](double Value) { return Value >= 0.0 && Value <= 1.0; };
		if (Disciplines.Marks.IsEmpty() || Disciplines.RefundSeconds < 0.0)
		{
			Problem(Pointer, TEXT("names at least one mark, and refundSeconds is at least 0"));
		}
		for (int32 Index = 0; Index < Disciplines.Marks.Num(); ++Index)
		{
			const FVeyraDisciplineMarkTuning& Mark = Disciplines.Marks[Index];
			const FString MarkPointer = FString::Printf(TEXT("%s/marks/%d"), *Pointer, Index);
			if (!Abilities.Statuses.Contains(Mark.Status))
			{
				Problem(MarkPointer + TEXT("/status"), FString::Printf(TEXT("names status \"%s\", which Abilities.json does not define"), *Mark.Status.ToString()));
			}
			for (const FVeyraContentId& Status : Mark.CasterStatuses)
			{
				if (!Abilities.Statuses.Contains(Status))
				{
					Problem(MarkPointer + TEXT("/casterStatuses"), FString::Printf(TEXT("names status \"%s\", which Abilities.json does not define"), *Status.ToString()));
				}
			}
			if (Mark.ConsumedBy.IsEmpty())
			{
				Problem(MarkPointer + TEXT("/consumedBy"), TEXT("names at least one ability"));
			}
			for (const FVeyraContentId& Ability : Mark.ConsumedBy)
			{
				if (!VeyraAbilityRules::Defines(Abilities, Ability))
				{
					Problem(MarkPointer + TEXT("/consumedBy"), FString::Printf(TEXT("names ability \"%s\", which Abilities.json does not define"), *Ability.ToString()));
				}
			}
			if (Mark.DamageAmount < 0.0 || Mark.DamagePerLevel < 0.0 || Mark.PhysicalPowerRatio < 0.0 || !IsFraction(Mark.Penetration) || !IsFraction(Mark.ResourceRefund)
				|| Mark.DamageType == EVeyraDamageType::TrueDamage)
			{
				Problem(MarkPointer, TEXT("its damage values are at least 0 and its type Physical or Magic; penetration and resourceRefund are from 0 to 1"));
			}
		}
		for (int32 Index = 0; Index < Disciplines.Bonuses.Num(); ++Index)
		{
			const FVeyraDisciplineBonusTuning& Bonus = Disciplines.Bonuses[Index];
			const FString BonusPointer = FString::Printf(TEXT("%s/bonuses/%d"), *Pointer, Index);
			const bool bSpends = Disciplines.Marks.ContainsByPredicate([&Bonus](const FVeyraDisciplineMarkTuning& Mark) { return Mark.ConsumedBy.Contains(Bonus.Ability); });
			if (!bSpends)
			{
				Problem(BonusPointer + TEXT("/ability"), FString::Printf(TEXT("names %s, which spends no mark"), *Bonus.Ability.ToString()));
			}
			if (Bonus.DamageMultiplier < 0.0 || !IsFraction(Bonus.CooldownRefund))
			{
				Problem(BonusPointer, TEXT("damageMultiplier is at least 0, and cooldownRefund from 0 to 1"));
			}
		}
	}

	for (const TPair<FVeyraContentId, FVeyraStressTemperTuning>& Entry : Tuning.StressTemper)
	{
		RegisterPassive(Entry.Key, TEXT("stressTemper"));
		const FString Pointer = TEXT("/stressTemper/") + Entry.Key.ToString();
		const FVeyraStressTemperTuning& Temper = Entry.Value;
		TArray<TPair<FString, FVeyraContentId>> Named = { { TEXT("/coating"), Temper.Coating }, { TEXT("/lockout"), Temper.Lockout } };
		for (const FVeyraContentId& Status : Temper.StrikeStatuses)
		{
			Named.Add({ TEXT("/strikeStatuses"), Status });
		}
		for (const TPair<FString, FVeyraContentId>& Each : Named)
		{
			if (!Abilities.Statuses.Contains(Each.Value))
			{
				Problem(Pointer + Each.Key, FString::Printf(TEXT("names status \"%s\", which Abilities.json does not define"), *Each.Value.ToString()));
			}
		}
		// The lockout outlasts the strike, and is not the coating it spares.
		const FVeyraStatusTuning* Lockout = Abilities.Statuses.Find(Temper.Lockout);
		if (Temper.Lockout == Temper.Coating || (Lockout && !(Lockout->DurationSeconds > 0.0)))
		{
			Problem(Pointer + TEXT("/lockout"), TEXT("names a status other than the coating, lasting above 0 seconds"));
		}
		if (Temper.DamageAmount < 0.0 || Temper.DamagePerLevel < 0.0 || Temper.MagicPowerRatio < 0.0 || Temper.DamageType == EVeyraDamageType::TrueDamage)
		{
			Problem(Pointer, TEXT("its damage values are at least 0, and its type Physical or Magic"));
		}
	}

	for (const TPair<FVeyraContentId, FVeyraChargerTuning>& Entry : Tuning.Charger)
	{
		RegisterPassive(Entry.Key, TEXT("charger"));
		const FString Pointer = TEXT("/charger/") + Entry.Key.ToString();
		const FVeyraChargerTuning& Charger = Entry.Value;
		if (!Abilities.Statuses.Contains(Charger.BoostStatus))
		{
			Problem(Pointer + TEXT("/boostStatus"), FString::Printf(TEXT("names status \"%s\", which Abilities.json does not define"), *Charger.BoostStatus.ToString()));
		}
		if (!(Charger.Radius > 0.0) || !(Charger.ChargePerDeath > 0.0) || Charger.BoostMultiplier < 1.0)
		{
			Problem(Pointer, TEXT("radius and chargePerDeath are above 0, and boostMultiplier at least 1"));
		}
	}

	for (const TPair<FVeyraContentId, FVeyraAccordTuning>& Entry : Tuning.Accord)
	{
		RegisterPassive(Entry.Key, TEXT("accord"));
		const FString Pointer = TEXT("/accord/") + Entry.Key.ToString();
		const FVeyraAccordTuning& Accord = Entry.Value;
		if (!Abilities.Companions.Contains(Accord.Companion))
		{
			Problem(Pointer + TEXT("/companion"), FString::Printf(TEXT("names companion \"%s\", which Abilities.json does not define"), *Accord.Companion.ToString()));
		}
		for (const TPair<FString, FVeyraContentId>& Each : { TPair<FString, FVeyraContentId>(TEXT("/mark"), Accord.Mark),
				 TPair<FString, FVeyraContentId>(TEXT("/boostStatus"), Accord.BoostStatus) })
		{
			if (!Abilities.Statuses.Contains(Each.Value))
			{
				Problem(Pointer + Each.Key, FString::Printf(TEXT("names status \"%s\", which Abilities.json does not define"), *Each.Value.ToString()));
			}
		}
		if (!(Accord.WindowSeconds > 0.0) || !(Accord.PerTargetSeconds > 0.0) || Accord.DamageAmount < 0.0 || Accord.DamagePerLevel < 0.0
			|| Accord.MagicPowerRatio < 0.0 || Accord.RefundSeconds < 0.0 || Accord.BoostMultiplier < 1.0)
		{
			Problem(Pointer, TEXT("windowSeconds and perTargetSeconds are above 0, its damage and refund at least 0, and boostMultiplier at least 1"));
		}
	}

	for (const TPair<FVeyraContentId, FVeyraUnreturnedTuning>& Entry : Tuning.Unreturned)
	{
		RegisterPassive(Entry.Key, TEXT("unreturned"));
		const FString Pointer = TEXT("/unreturned/") + Entry.Key.ToString();
		const FVeyraUnreturnedTuning& Unreturned = Entry.Value;
		if (!(Unreturned.CheckSeconds > 0.0) || !(Unreturned.RestoreFractionPerSecond > 0.0) || Unreturned.RestoreFractionPerSecond > 1.0)
		{
			Problem(Pointer, TEXT("checkSeconds is above 0, and restoreFractionPerSecond above 0 and at most 1"));
		}
		for (int32 Index = 0; Index < Unreturned.Thresholds.Num(); ++Index)
		{
			const FVeyraUnreturnedThresholdTuning& Threshold = Unreturned.Thresholds[Index];
			const FString ThresholdPointer = FString::Printf(TEXT("%s/thresholds/%d"), *Pointer, Index);
			if (!(Threshold.HealthFraction > 0.0) || Threshold.HealthFraction > 1.0)
			{
				Problem(ThresholdPointer + TEXT("/healthFraction"), TEXT("must be above 0 and at most 1"));
			}
			// Each status outlasts a check, so it holds while its owner stays below the threshold.
			for (const FVeyraContentId& Status : Threshold.Statuses)
			{
				const FVeyraStatusTuning* Given = Abilities.Statuses.Find(Status);
				if (!Given || !(Given->DurationSeconds > Unreturned.CheckSeconds))
				{
					Problem(ThresholdPointer + TEXT("/statuses"), FString::Printf(TEXT("names status \"%s\", which Abilities.json must define lasting longer than a check"), *Status.ToString()));
				}
			}
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
		// Charge never regenerates: only effects restore it (ADR-033 §1).
		if (Vanguard.Resource == EVeyraResourceFamily::Charge && (Vanguard.BaseStats.ResourceRegen != 0.0 || Vanguard.Growth.ResourceRegen != 0.0))
		{
			Problem(Pointer + TEXT("/resource"), TEXT("Charge never regenerates: its baseStats and growth resourceRegen are 0"));
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
		if (!Vanguard.RankShape.IsEmpty() && !Progression.RankShapes.Contains(Vanguard.RankShape[0]))
		{
			Problem(Pointer + TEXT("/rankShape/0"), FString::Printf(TEXT("names rank shape \"%s\", which Progression.json does not define"), *Vanguard.RankShape[0].ToString()));
		}
		// Each slot's abilities rank as the Vanguard's rank shape says (ADR-031 §2).
		const FVeyraRankShape Shape = RankShapeOf(Vanguard, Progression);
		const int32 BasicRanks = VeyraProgression::MaxRank(EVeyraAbilitySlot::Q, Progression, Shape);
		const int32 UltimateRanks = VeyraProgression::MaxRank(EVeyraAbilitySlot::R, Progression, Shape);
		const FSlot Slots[] = { { TEXT("q"), &Vanguard.Abilities.Q, BasicRanks }, { TEXT("w"), &Vanguard.Abilities.W, BasicRanks },
			{ TEXT("e"), &Vanguard.Abilities.E, BasicRanks }, { TEXT("r"), &Vanguard.Abilities.R, UltimateRanks } };
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
		// A stance's abilities rank with the slots they hold, and are the kit's own as much (ADR-031 §3).
		for (const TArray<FVeyraContentId>* Ids : { &Vanguard.Abilities.Q, &Vanguard.Abilities.W, &Vanguard.Abilities.E, &Vanguard.Abilities.R })
		{
			for (const FVeyraContentId& Ability : *Ids)
			{
				const FVeyraStanceAbilityTuning* Stance = Abilities.Stance.Find(Ability);
				for (int32 Index = 0; Stance && Index < Stance->Slots.Num(); ++Index)
				{
					const FVeyraStanceSlotTuning& Held = Stance->Slots[Index];
					const FString HeldPointer = FString::Printf(TEXT("%s/abilities (stance %s, slot %d)"), *Pointer, *Ability.ToString(), Index);
					for (const FString& RankProblem : VeyraAbilityRules::ValidateRanks(Abilities, Held.Ability, VeyraProgression::MaxRank(Held.Slot, Progression, Shape)))
					{
						Problem(HeldPointer, TEXT("in its slot, ") + RankProblem);
					}
					if (Kit.Contains(Held.Ability))
					{
						Problem(HeldPointer, FString::Printf(TEXT("names %s, already in the kit; each slot has its own ability"), *Held.Ability.ToString()));
					}
					Kit.Add(Held.Ability);
				}
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
