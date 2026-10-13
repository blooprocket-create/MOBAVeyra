// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER && WITH_VEYRA_UI

#include "Stats/VeyraHaste.h"
#include "Statuses/VeyraStatusTypes.h"
#include "Text/VeyraAbilityNumbers.h"
#include "Text/VeyraContentText.h"
#include "Tuning/VeyraAbilitiesTuning.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"

namespace VeyraAbilityNumbersTests
{
	// Veyra.UI.AbilityNumbers.*: an ability's numbers beside its text, from the committed tuning and the cast's own
	// formula (ADR-065 §7).
	TEST_CLASS(AbilityNumbers, "Veyra.UI")
	{
		// Fixture values: a rank and a Magic Power to work Bear Hug's numbers out at.
		static constexpr int32 Rank = 3;
		static constexpr double MagicPower = 100.0;

		static FVeyraContentId Id(const TCHAR* Text)
		{
			return FVeyraContentId::FromText(Text).GetValue();
		}

		static TArray<FString> RolesOf(const TCHAR* Ability)
		{
			TArray<FString> Roles;
			for (const VeyraAbilityRules::FVeyraAbilityDamagePart& Part : VeyraAbilityRules::DamageParts(UVeyraAbilitiesTuningSubsystem::Get(), Id(Ability)))
			{
				Roles.Add(Part.Role);
			}
			return Roles;
		}

		TEST_METHOD(AnAbilitysDamagePartsCarryTheirRoles)
		{
			ASSERT_THAT(IsTrue(RolesOf(TEXT("patch_bear_hug")) == TArray<FString>{ TEXT("hostEffects") }));
			ASSERT_THAT(IsTrue(RolesOf(TEXT("patch_dont_leave_me")) == TArray<FString>{ TEXT("siphon") }));
			ASSERT_THAT(IsTrue(RolesOf(TEXT("patch_the_thing_inside")) == TArray<FString>{ TEXT("enemyDamagePerSecond") }));
			ASSERT_THAT(IsTrue(RolesOf(TEXT("patch_play_dead")).IsEmpty(), TEXT("an ability that deals no damage has no parts")));
		}

		TEST_METHOD(AtARankWithPowerTheNumbersAreTheCastsOwn)
		{
			const FVeyraAbilitiesTuning& Tuning = UVeyraAbilitiesTuningSubsystem::Get();
			const FVeyraContentId BearHug = Id(TEXT("patch_bear_hug"));
			const FVeyraDamageTuning& Damage = VeyraAbilityRules::DamageParts(Tuning, BearHug)[0].Damage[0];
			const double Amount = VeyraAbilityRules::DamageAmount(Damage, Rank, 0.0, MagicPower);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Amount, VeyraAbilityRules::ValueAtRank(Damage.AmountByRank, Rank) + MagicPower * Damage.MagicPowerRatio)));

			const TArray<FString> Lines = VeyraAbilityNumbers::AtRank(BearHug, Rank, 0.0, MagicPower, TEXT("Mana"));
			ASSERT_THAT(IsTrue(Lines.Num() == 2, *FString::Join(Lines, TEXT(" | "))));
			ASSERT_THAT(IsTrue(Lines[0].StartsWith(FString::Printf(TEXT("Cooldown %.0f s"), VeyraAbilityRules::CooldownSeconds(Tuning, BearHug, Rank))) && Lines[0].EndsWith(TEXT("Mana")),
				*Lines[0]));
			ASSERT_THAT(IsTrue(Lines[1].Contains(FString::Printf(TEXT("%.0f magic damage"), Amount)) && Lines[1].Contains(TEXT("% Magic Power")), *Lines[1]));

			const TArray<FString> PerRank = VeyraAbilityNumbers::ByRank(BearHug);
			ASSERT_THAT(IsTrue(PerRank.Num() == 1 && PerRank[0].Contains(TEXT(" / ")) && PerRank[0].EndsWith(TEXT("magic damage")), *FString::Join(PerRank, TEXT(" | "))));
		}

		TEST_METHOD(ATooltipSummaryShowsTheWaitTheCostEachFactAndTheNextRank)
		{
			// ADR-080 §2, on an ability that damages, stuns the close and slows those further out.
			const FVeyraAbilitiesTuning& Tuning = UVeyraAbilitiesTuningSubsystem::Get();
			const FVeyraContentId Hold = Id(TEXT("cairn_crushing_hold"));
			constexpr int32 HoldRank = 2;
			constexpr int32 MaxRank = 5;
			constexpr double PhysicalPower = 58.0;
			constexpr double Haste = 10.0;
			const FVeyraAbilitySummary Summary = VeyraAbilityNumbers::Summarise(Hold, HoldRank, MaxRank, PhysicalPower, 0.0, 0.0, Haste, TEXT("Mana"));
			// The wait the caster would have after Ability Haste, by the cooldown's own formula.
			const double Expected = VeyraAbilityRules::CooldownSeconds(Tuning, Hold, HoldRank) * VeyraHaste::CooldownMultiplier(Haste);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Summary.CooldownSeconds, Expected) && Summary.Cooldown.EndsWith(TEXT(" s")), *Summary.Cooldown));
			ASSERT_THAT(IsTrue(Summary.Cost.EndsWith(TEXT(" Mana")), *Summary.Cost));
			// Its damage now, physical, and how it is made from the caster's power.
			const FVeyraDamageTuning& Damage = VeyraAbilityRules::DamageParts(Tuning, Hold)[0].Damage[0];
			const FString Amount = FString::Printf(TEXT("%.0f physical"), FMath::RoundToDouble(VeyraAbilityRules::DamageAmount(Damage, HoldRank, PhysicalPower, 0.0)));
			const FVeyraAbilityFact* Hit = Summary.Facts.FindByPredicate([](const FVeyraAbilityFact& Fact) { return Fact.Tone == EVeyraFactTone::Physical; });
			ASSERT_THAT(IsTrue(Hit && Hit->Value == Amount && Hit->Note.Contains(TEXT("of your 58 Physical Power")), Hit ? *Hit->Value : TEXT("no damage fact")));
			// The stun and the slow its areas put on enemies, as harm, the slow by its strength for how long.
			const FString Stunned = VeyraContentText::StatusKindName(EVeyraStatusKind::Stun).ToString();
			const FString Slowed = VeyraContentText::StatusKindName(EVeyraStatusKind::Slow).ToString();
			const FVeyraAbilityFact* Stun = Summary.Facts.FindByPredicate([&Stunned](const FVeyraAbilityFact& Fact) { return Fact.Value.StartsWith(Stunned); });
			const FVeyraAbilityFact* Slow = Summary.Facts.FindByPredicate([&Slowed](const FVeyraAbilityFact& Fact) { return Fact.Value.StartsWith(Slowed); });
			ASSERT_THAT(IsTrue(Stun && Stun->Tone == EVeyraFactTone::Harm && Stun->Value.EndsWith(TEXT(" s")), TEXT("stunned, for how long")));
			ASSERT_THAT(IsTrue(Slow && Slow->Tone == EVeyraFactTone::Harm && Slow->Value.EndsWith(TEXT("%")) && Slow->Note.StartsWith(TEXT("for ")), TEXT("slowed by a share, for how long")));
			// The whole amount at the next rank with the same power, and none at the last.
			ASSERT_THAT(AreEqual(Summary.NextRank, FString::Printf(TEXT("%.0f"), FMath::RoundToDouble(VeyraAbilityRules::DamageAmount(Damage, HoldRank + 1, PhysicalPower, 0.0)))));
			ASSERT_THAT(IsTrue(VeyraAbilityNumbers::Summarise(Hold, MaxRank, MaxRank, PhysicalPower, 0.0, 0.0, Haste, TEXT("Mana")).NextRank.IsEmpty()));
		}

		TEST_METHOD(AShieldsFactIsTheGrantsOwnAmountAndHowItIsMade)
		{
			// ADR-080 §2, on a self-buff whose shield grows with the caster's Max Health.
			const FVeyraAbilitiesTuning& Tuning = UVeyraAbilitiesTuningSubsystem::Get();
			const FVeyraContentId WeatherIt = Id(TEXT("qazharr_weather_it"));
			constexpr int32 ShieldRank = 2;
			constexpr int32 MaxRank = 5;
			constexpr double MaxHealth = 1000.0;
			const TArray<VeyraAbilityRules::FVeyraAbilityShieldPart> Parts = VeyraAbilityRules::ShieldParts(Tuning, WeatherIt);
			ASSERT_THAT(IsTrue(Parts.Num() == 1, TEXT("one shield")));
			const FVeyraShieldTuning& Shield = Parts[0].Shield;
			ASSERT_THAT(IsTrue(VeyraContentText::HasDamageRole(Parts[0].Role), *Parts[0].Role));
			// The grant's own formula: the rank's amount and the share of Max Health.
			const double Amount = VeyraAbilityRules::ShieldAmount(Shield, ShieldRank, MaxHealth, 0.0);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Amount, VeyraAbilityRules::ValueAtRank(Shield.AmountByRank, ShieldRank) + MaxHealth * Shield.MaxHealthRatio)));
			const FVeyraAbilitySummary Summary = VeyraAbilityNumbers::Summarise(WeatherIt, ShieldRank, MaxRank, 0.0, 0.0, MaxHealth, 0.0, TEXT("Mana"));
			const FVeyraAbilityFact* Fact = Summary.Facts.FindByPredicate([](const FVeyraAbilityFact& Each) { return Each.Value.EndsWith(TEXT(" shield")); });
			ASSERT_THAT(IsTrue(Fact && Fact->Tone == EVeyraFactTone::Help && Fact->Value == FString::Printf(TEXT("%.0f shield"), FMath::RoundToDouble(Amount)),
				Fact ? *Fact->Value : TEXT("no shield fact")));
			ASSERT_THAT(IsTrue(Fact->Note.Contains(TEXT("of your 1000 Max Health")) && Fact->Note.EndsWith(TEXT(" s")), *Fact->Note));
		}

		TEST_METHOD(EveryShieldRoleHasItsWords)
		{
			// Every shield any ability grants is labelled by a damage_role row, not the plain "Damage".
			const FVeyraAbilitiesTuning& Tuning = UVeyraAbilitiesTuningSubsystem::Get();
			TArray<FVeyraContentId> Abilities;
			Tuning.SelfBuff.GetKeys(Abilities);
			for (const TPair<FVeyraContentId, FVeyraAreaAbilityTuning>& Each : Tuning.Area)
			{
				Abilities.Add(Each.Key);
			}
			for (const FVeyraContentId& Ability : Abilities)
			{
				for (const VeyraAbilityRules::FVeyraAbilityShieldPart& Part : VeyraAbilityRules::ShieldParts(Tuning, Ability))
				{
					ASSERT_THAT(IsTrue(VeyraContentText::HasDamageRole(Part.Role), *FString::Printf(TEXT("%s: %s"), *Ability.ToString(), *Part.Role)));
				}
			}
		}

		TEST_METHOD(EveryStatusKindHasItsWords)
		{
			// Status chips and tooltips name a status by its kind's row in the text table (ADR-080 §2).
			const UEnum* Kinds = StaticEnum<EVeyraStatusKind>();
			for (int32 Index = 0; Index < Kinds->NumEnums() - 1; ++Index)
			{
				const EVeyraStatusKind Kind = static_cast<EVeyraStatusKind>(Kinds->GetValueByIndex(Index));
				ASSERT_THAT(IsTrue(VeyraContentText::StatusKindName(Kind).IsFromStringTable(), *Kinds->GetNameStringByIndex(Index)));
			}
		}
	};
}

#endif // WITH_AUTOMATION_WORKER && WITH_VEYRA_UI
