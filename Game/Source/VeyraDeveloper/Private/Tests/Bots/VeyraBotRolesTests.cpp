// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Brain/VeyraBotRoles.h"
#include "CQTest.h"
#include "Tuning/VeyraBotsTuningSubsystem.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraBotRolesTests
{
	using EVeyraBotRole::Bottom;
	using EVeyraBotRole::Jungle;
	using EVeyraBotRole::Mid;
	using EVeyraBotRole::Top;

	// Veyra.Bots.BotRoles.*: a team's bots share its places by the roles their Vanguards play (ADR-039 §5).
	TEST_CLASS(BotRoles, "Veyra.Bots")
	{
		TEST_METHOD(TheJungleGoesToTheBotThatPlaysItBest)
		{
			// The places of four seats, Mid, Top, Jungle and Bottom; a support in the jungle seat.
			const TArray<EVeyraBotRole> Places = { Mid, Top, Jungle, Bottom };
			const TArray<TArray<EVeyraBotRole>> Preferences = { { Bottom, Mid }, { Top, Jungle, Mid }, { Bottom, Mid, Top }, { Mid, Bottom } };
			const TArray<int32> PlaceOf = VeyraBotRoles::Deal(Places, Preferences);
			ASSERT_THAT(AreEqual(4, PlaceOf.Num()));
			ASSERT_THAT(AreEqual(2, PlaceOf[1], TEXT("the one that plays Jungle second takes it, before the support who plays it not")));
			ASSERT_THAT(AreEqual(0, PlaceOf[3], TEXT("then Mid, Top and Bottom in order: Mid to the one that plays it first")));
			ASSERT_THAT(AreEqual(1, PlaceOf[2], TEXT("Top to the support that plays it, third")));
			ASSERT_THAT(AreEqual(3, PlaceOf[0], TEXT("Bottom to the other support")));
		}

		TEST_METHOD(EveryBotTakesOnePlaceAndTiesGoToTheEarlier)
		{
			const TArray<EVeyraBotRole> Places = { Mid, Top, Jungle, Bottom, Bottom };
			const TArray<TArray<EVeyraBotRole>> Same = { { Mid }, { Mid }, { Mid }, { Mid }, { Mid } };
			const TArray<int32> PlaceOf = VeyraBotRoles::Deal(Places, Same);
			TSet<int32> Taken(PlaceOf);
			ASSERT_THAT(AreEqual(5, Taken.Num(), TEXT("each place once")));
			ASSERT_THAT(AreEqual(2, PlaceOf[0], TEXT("nobody plays Jungle: the earliest takes it")));
			ASSERT_THAT(AreEqual(0, PlaceOf[1], TEXT("Mid to the earliest left")));
			const TArray<int32> Empty = VeyraBotRoles::Deal({}, {});
			ASSERT_THAT(IsTrue(Empty.IsEmpty()));
		}

		TEST_METHOD(EveryVanguardsBotNamesItsRoles)
		{
			for (const TPair<FVeyraContentId, FVeyraBotVanguardTuning>& Each : UVeyraBotsTuningSubsystem::Get().Vanguards)
			{
				ASSERT_THAT(IsFalse(Each.Value.Roles.IsEmpty(), Each.Key.ToString()));
			}
			FVeyraBotsTuning Broken = UVeyraBotsTuningSubsystem::Get();
			Broken.Vanguards.begin()->Value.Roles = { Mid, Mid };
			ASSERT_THAT(IsTrue(VeyraBots::Validate(Broken).ContainsByPredicate([](const FString& Problem) { return Problem.Contains(TEXT("/roles")); })));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
