// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER && WITH_VEYRA_UI

#include "Text/VeyraAbilityNumbers.h"
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
	};
}

#endif // WITH_AUTOMATION_WORKER && WITH_VEYRA_UI
