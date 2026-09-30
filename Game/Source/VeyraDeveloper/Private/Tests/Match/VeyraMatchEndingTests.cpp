// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Ending/VeyraMatchEnding.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraMatchEndingTests
{
	// Veyra.Match.Ending.*: how a match's end reads to each player (ADR-020 §1).
	TEST_CLASS(Ending, "Veyra.Match")
	{
		TEST_METHOD(AFallenWellIsTheOtherSidesVictoryAndItsOwnSidesDefeat)
		{
			ASSERT_THAT(IsTrue(VeyraMatchEnding::Headline(EVeyraTeam::B, EVeyraTeam::A) == EVeyraEndingHeadline::Victory));
			ASSERT_THAT(IsTrue(VeyraMatchEnding::Headline(EVeyraTeam::B, EVeyraTeam::B) == EVeyraEndingHeadline::Defeat));
			ASSERT_THAT(IsTrue(VeyraMatchEnding::Headline({}, EVeyraTeam::A) == EVeyraEndingHeadline::MatchOver, TEXT("no Well fell")));
			ASSERT_THAT(IsTrue(VeyraMatchEnding::Headline(EVeyraTeam::A, EVeyraTeam::None) == EVeyraEndingHeadline::MatchOver, TEXT("a viewer on no side")));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
