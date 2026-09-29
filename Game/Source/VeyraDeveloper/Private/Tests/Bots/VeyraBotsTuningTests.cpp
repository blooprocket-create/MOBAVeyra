// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Tuning/VeyraBotsTuningSubsystem.h"
#include "Tuning/VeyraVanguardsTuningSubsystem.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraBotsTests
{
	inline FVeyraContentId BotsId(const TCHAR* Text)
	{
		return FVeyraContentId::FromText(Text).GetValue();
	}

	// Veyra.Bots.BotsTuning.*: the committed Bots.json loads, and the checks the schema cannot express
	// hold every Vanguard's entry to the catalog and its kit (ADR-013 §5).
	TEST_CLASS(BotsTuning, "Veyra.Bots")
	{
		static bool HasProblem(const TArray<FString>& Problems, const TCHAR* Prefix)
		{
			return Problems.ContainsByPredicate([Prefix](const FString& Problem) { return Problem.StartsWith(Prefix); });
		}

		TEST_METHOD(TheCommittedFileLoadsWithAnEntryForEveryReleasedVanguard)
		{
			const FVeyraBotsTuning& Tuning = UVeyraBotsTuningSubsystem::Get();
			ASSERT_THAT(IsTrue(VeyraBots::Validate(Tuning).IsEmpty()));
			ASSERT_THAT(IsFalse(Tuning.Seats.IsEmpty()));
			// League's five roles: a seat of five plays the jungle (ADR-014 §7), and takes the spell that finishes camps (ADR-015 §8).
			const FVeyraBotSeatTuning* Jungler = Tuning.Seats.FindByPredicate([](const FVeyraBotSeatTuning& Seat) { return Seat.Role == EVeyraBotRole::Jungle; });
			ASSERT_THAT(IsNotNull(Jungler));
			ASSERT_THAT(IsTrue(Jungler->FluxSpells.ContainsByPredicate([&Tuning](const FVeyraContentId& Spell) {
				const EVeyraBotAbilityUse* Use = Tuning.FluxSpells.Find(Spell);
				return Use && *Use == EVeyraBotAbilityUse::Secure;
			})));
			// A seat's spells come off the roster, and each has a use.
			FVeyraBotsTuning Broken = Tuning;
			Broken.Seats[0].FluxSpells = { FVeyraContentId::FromText(TEXT("ignite")).GetValue() };
			ASSERT_THAT(IsTrue(HasProblem(VeyraBots::Validate(Broken), TEXT("/seats/0/fluxSpells"))));
			Broken = Tuning;
			Broken.FluxSpells.Remove(Tuning.Seats[0].FluxSpells[0]);
			ASSERT_THAT(IsTrue(HasProblem(VeyraBots::Validate(Broken), TEXT("/fluxSpells"))));
			for (const TPair<FVeyraContentId, FVeyraVanguardDefinition>& Pair : UVeyraVanguardsTuningSubsystem::Get().Vanguards)
			{
				if (Pair.Value.Availability == EVeyraVanguardAvailability::Playable)
				{
					ASSERT_THAT(IsNotNull(UVeyraBotsTuningSubsystem::FindVanguard(Pair.Key), *Pair.Key.ToString()));
				}
			}
			// League's jungler and support ward: the warding seats are seats, each named once (ADR-016 §7).
			ASSERT_THAT(IsTrue(Tuning.Warding.Seats.ContainsByPredicate([&Tuning](int32 Seat) { return Tuning.Seats[Seat].Role == EVeyraBotRole::Jungle; })));
			Broken = Tuning;
			Broken.Warding.Seats.Add(Tuning.Seats.Num());
			ASSERT_THAT(IsTrue(HasProblem(VeyraBots::Validate(Broken), TEXT("/warding/seats"))));
			Broken = Tuning;
			Broken.Warding.Seats.Add(Tuning.Warding.Seats[0]);
			ASSERT_THAT(IsTrue(HasProblem(VeyraBots::Validate(Broken), TEXT("/warding/seats"))));
			// Intermediate plays sharper than Beginner.
			const FVeyraBotDifficultyTuning& Beginner = UVeyraBotsTuningSubsystem::GetDifficulty(EVeyraBotDifficulty::Beginner);
			const FVeyraBotDifficultyTuning& Intermediate = UVeyraBotsTuningSubsystem::GetDifficulty(EVeyraBotDifficulty::Intermediate);
			ASSERT_THAT(IsTrue(Intermediate.ReactionSeconds < Beginner.ReactionSeconds && Intermediate.LastHitChance > Beginner.LastHitChance));
		}

		TEST_METHOD(BuildsNameItemsTheCatalogSellsAndNoneTakesAnotherApart)
		{
			FVeyraBotsTuning Broken = UVeyraBotsTuningSubsystem::Get();
			FVeyraBotVanguardTuning& Cairn = Broken.Vanguards[BotsId(TEXT("cairn"))];
			Cairn.Build.Add(BotsId(TEXT("no_such_item")));
			// Vital Plate goes into Colossus Temper, which comes earlier in the build.
			Cairn.Build.Add(BotsId(TEXT("vital_plate")));
			const TArray<FString> Problems = VeyraBots::Validate(Broken);
			const int32 Unknown = Cairn.Build.Num() - 2;
			ASSERT_THAT(IsTrue(HasProblem(Problems, *FString::Printf(TEXT("/vanguards/cairn/build/%d"), Unknown)), FString::Join(Problems, TEXT(" | "))));
			ASSERT_THAT(IsTrue(HasProblem(Problems, *FString::Printf(TEXT("/vanguards/cairn/build/%d"), Unknown + 1))));
		}

		TEST_METHOD(SkillPriorityNamesEachKitSlotOnce)
		{
			FVeyraBotsTuning Broken = UVeyraBotsTuningSubsystem::Get();
			Broken.Vanguards[BotsId(TEXT("oriel"))].SkillPriority = { EVeyraBotSkill::Q, EVeyraBotSkill::Q, EVeyraBotSkill::E };
			ASSERT_THAT(IsTrue(HasProblem(VeyraBots::Validate(Broken), TEXT("/vanguards/oriel/skillPriority"))));
		}

		TEST_METHOD(EveryReleasedVanguardAndItsWholeKitHasAnEntry)
		{
			FVeyraBotsTuning Broken = UVeyraBotsTuningSubsystem::Get();
			Broken.Vanguards.Remove(BotsId(TEXT("bryn")));
			Broken.Vanguards[BotsId(TEXT("qazharr"))].Abilities.Remove(BotsId(TEXT("qazharr_no_quarter")));
			const TArray<FString> Problems = VeyraBots::Validate(Broken);
			ASSERT_THAT(IsTrue(Problems.ContainsByPredicate([](const FString& Problem) { return Problem.Contains(TEXT("no entry for bryn")); }),
				FString::Join(Problems, TEXT(" | "))));
			ASSERT_THAT(IsTrue(HasProblem(Problems, TEXT("/vanguards/qazharr/abilities"))));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
