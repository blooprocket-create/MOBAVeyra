// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Absence/VeyraAbsenceRules.h"
#include "CQTest.h"
#include "Tuning/VeyraMatchTuning.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraAbsenceTests
{
	// Veyra.Match.Absence.*: the pure rules of AFK, disconnection, personal loss, forgiveness and the
	// autopilot (Match Flow Bible §4–§6; ADR-019 §2–§3). Fixture values, not the committed tuning.
	TEST_CLASS(Absence, "Veyra.Match")
	{
		FVeyraAbsenceTuning Tuning;
		FVeyraActivityTuning Activity;
		FVeyraAutopilotTuning Autopilot;

		BEFORE_EACH()
		{
			Tuning.AfkAfterSeconds = 90.0;
			Tuning.AfkPenaltyAfterSeconds = 60.0;
			Tuning.DisconnectPenaltyAfterSeconds = 150.0;
			Tuning.MaxForgivenAbsentFraction = 0.1;
			Activity.MinimumMoveDistance = 300.0;
			Autopilot.FountainAfterSeconds = 60.0;
			Autopilot.BehindTowerDistance = 700.0;
		}

		TEST_METHOD(AnInactivePlayerIsAfkFromTheThresholdThenLosesAfterTheGrace)
		{
			FVeyraAbsenceRecord Record;
			ASSERT_THAT(IsFalse(VeyraAbsence::Update(Record, 89.0, Tuning).bBecameAfk));
			ASSERT_THAT(IsTrue(VeyraAbsence::Update(Record, 100.0, Tuning).bBecameAfk));
			ASSERT_THAT(IsTrue(Record.Absence == EVeyraAbsence::Afk));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Record.AwaySince.GetValue(), 90.0), TEXT("from the threshold, not from when it was noticed")));
			ASSERT_THAT(IsFalse(VeyraAbsence::Update(Record, 149.0, Tuning).bPenalized));
			ASSERT_THAT(IsTrue(VeyraAbsence::Update(Record, 150.0, Tuning).bPenalized));
			ASSERT_THAT(IsFalse(VeyraAbsence::Update(Record, 151.0, Tuning).bPenalized, TEXT("once")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraAbsence::AbsentSeconds(Record, 160.0), 70.0)));
		}

		TEST_METHOD(OnlyMeaningfulActivityKeepsAPlayerPresent)
		{
			FVeyraAbsenceRecord Record;
			ASSERT_THAT(IsTrue(VeyraAbsence::NoteActivity(Record, 10.0, FVector(1000.0, 0.0, 0.0), Activity)));
			ASSERT_THAT(IsFalse(VeyraAbsence::NoteActivity(Record, 80.0, FVector(1100.0, 0.0, 0.0), Activity), TEXT("a move to nearly the same place")));
			ASSERT_THAT(IsTrue(VeyraAbsence::Update(Record, 100.0, Tuning).bBecameAfk, TEXT("so it goes AFK 90 s after the last move that counted")));
			ASSERT_THAT(IsTrue(VeyraAbsence::NoteActivity(Record, 105.0, TOptional<FVector>(), Activity), TEXT("any other order counts")));
			ASSERT_THAT(IsTrue(Record.Absence == EVeyraAbsence::Present && Record.ReturnedAt.GetValue() == 105.0));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Record.ClosedAbsentSeconds, 5.0)));
		}

		TEST_METHOD(ADisconnectLosesAfterItsOwnThresholdAndAReturnResetsOnlyTheTrigger)
		{
			FVeyraAbsenceRecord Record;
			VeyraAbsence::NoteDisconnected(Record, 10.0);
			ASSERT_THAT(IsFalse(VeyraAbsence::Update(Record, 159.0, Tuning).bPenalized));
			VeyraAbsence::NoteConnected(Record, 159.0);
			ASSERT_THAT(IsTrue(Record.Absence == EVeyraAbsence::Present));
			VeyraAbsence::NoteDisconnected(Record, 160.0);
			ASSERT_THAT(IsFalse(VeyraAbsence::Update(Record, 300.0, Tuning).bPenalized, TEXT("short disconnects never add up to the trigger")));
			ASSERT_THAT(IsTrue(VeyraAbsence::Update(Record, 310.0, Tuning).bPenalized));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraAbsence::AbsentSeconds(Record, 310.0), 149.0 + 150.0), TEXT("but the total keeps every absence")));
		}

		TEST_METHOD(AnAfkPlayerWhoDisconnectsStartsTheDisconnectTrigger)
		{
			FVeyraAbsenceRecord Record;
			VeyraAbsence::Update(Record, 90.0, Tuning);
			VeyraAbsence::NoteDisconnected(Record, 100.0);
			ASSERT_THAT(IsTrue(Record.Absence == EVeyraAbsence::Disconnected));
			ASSERT_THAT(IsFalse(VeyraAbsence::Update(Record, 200.0, Tuning).bPenalized, TEXT("not the AFK grace")));
			ASSERT_THAT(IsTrue(VeyraAbsence::Update(Record, 250.0, Tuning).bPenalized));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Record.AwaySince.GetValue(), 90.0), TEXT("one absence throughout")));
		}

		TEST_METHOD(TheAutopilotGoesBehindTheNearestAlliedTowerThenHome)
		{
			FVeyraAbsenceRecord Record;
			ASSERT_THAT(IsTrue(VeyraAbsence::StageOf(Record, 0.0, Autopilot) == EVeyraAutopilotStage::None));
			VeyraAbsence::NoteDisconnected(Record, 100.0);
			ASSERT_THAT(IsTrue(VeyraAbsence::StageOf(Record, 159.0, Autopilot) == EVeyraAutopilotStage::BehindTower));
			ASSERT_THAT(IsTrue(VeyraAbsence::StageOf(Record, 160.0, Autopilot) == EVeyraAutopilotStage::Fountain));

			const FVector Home(-5000.0, -5000.0, 0.0);
			const TArray<FVector> Towers = { FVector(0.0, 0.0, 0.0), FVector(-2000.0, 3000.0, 0.0) };
			const FVector Behind = VeyraAbsence::Destination(EVeyraAutopilotStage::BehindTower, FVector(500.0, 500.0, 0.0), Towers, Home, Autopilot.BehindTowerDistance);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(FVector::Dist2D(Behind, Towers[0]), Autopilot.BehindTowerDistance), TEXT("the nearest tower's")));
			ASSERT_THAT(IsTrue(FVector::Dist2D(Behind, Home) < FVector::Dist2D(Towers[0], Home), TEXT("on the home side")));
			ASSERT_THAT(IsTrue(VeyraAbsence::Destination(EVeyraAutopilotStage::BehindTower, FVector::ZeroVector, {}, Home, Autopilot.BehindTowerDistance).Equals(Home),
				TEXT("no allied tower stands: home")));
			ASSERT_THAT(IsTrue(VeyraAbsence::Destination(EVeyraAutopilotStage::Fountain, FVector::ZeroVector, Towers, Home, Autopilot.BehindTowerDistance).Equals(Home)));
		}

		TEST_METHOD(AWinForgivesOnlyAShortAbsenceFollowedByAContribution)
		{
			FVeyraAbsenceRecord Record;
			VeyraAbsence::NoteDisconnected(Record, 100.0);
			VeyraAbsence::Update(Record, 250.0, Tuning);
			VeyraAbsence::NoteConnected(Record, 250.0);
			ASSERT_THAT(IsTrue(Record.bPersonalLoss));
			const double LongMatch = 2000.0;
			ASSERT_THAT(IsTrue(VeyraAbsence::IsForgiven(Record, true, LongMatch, true, Tuning)));
			ASSERT_THAT(IsFalse(VeyraAbsence::IsForgiven(Record, false, LongMatch, true, Tuning), TEXT("not on a loss")));
			ASSERT_THAT(IsFalse(VeyraAbsence::IsForgiven(Record, true, LongMatch, false, Tuning), TEXT("not without a contribution")));
			ASSERT_THAT(IsFalse(VeyraAbsence::IsForgiven(Record, true, 1000.0, true, Tuning), TEXT("not past a tenth of the match")));
			FVeyraAbsenceRecord Present;
			ASSERT_THAT(IsTrue(VeyraAbsence::IsForgiven(Present, false, LongMatch, false, Tuning), TEXT("nothing to forgive")));
		}

		TEST_METHOD(AnAbsenceAtTheEndCountsInTheTotalButDoesNotBarForgiveness)
		{
			FVeyraAbsenceRecord Record;
			VeyraAbsence::NoteDisconnected(Record, 100.0);
			VeyraAbsence::Update(Record, 250.0, Tuning);
			VeyraAbsence::NoteConnected(Record, 250.0);
			const double LongMatch = 2000.0;
			FVeyraAbsenceRecord Brief = Record;
			VeyraAbsence::NoteDisconnected(Brief, 1990.0);
			ASSERT_THAT(IsTrue(VeyraAbsence::IsForgiven(Brief, true, LongMatch, true, Tuning), TEXT("150 s and then 10 s: within a tenth of the match")));
			FVeyraAbsenceRecord Long = Record;
			VeyraAbsence::NoteDisconnected(Long, 1940.0);
			ASSERT_THAT(IsFalse(VeyraAbsence::IsForgiven(Long, true, LongMatch, true, Tuning), TEXT("150 s and then 60 s: past it")));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
