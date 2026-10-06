// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER && WITH_VEYRA_UI

#include "Hud/VeyraKillFeedModel.h"

namespace VeyraKillFeedViewTests
{
	// Veyra.UI.KillFeedView.*: which falls the kill feed shows, and which it announces to the player (ADR-065 §10).
	TEST_CLASS(KillFeedView, "Veyra.UI")
	{
		// Fixture values: players' IDs, and the feed's timing.
		static constexpr int32 Own = 1;
		static constexpr int32 Ally = 2;
		static constexpr int32 Enemy = 3;
		static constexpr double ShowSeconds = 8.0;
		static constexpr int32 MaxRows = 2;

		static FVeyraKillFeedArrival Takedown(int32 Killer, EVeyraTeam KillerSide, int32 Victim, double At, bool bFirstBlood = false)
		{
			FVeyraKillFeedArrival Arrival;
			Arrival.Line.Kind = EVeyraKillFeedKind::Takedown;
			Arrival.Line.KillerPlayerId = Killer;
			Arrival.Line.KillerName = FString::Printf(TEXT("Player %d"), Killer);
			Arrival.Line.KillerSide = KillerSide;
			Arrival.Line.VictimPlayerId = Victim;
			Arrival.Line.VictimName = FString::Printf(TEXT("Player %d"), Victim);
			Arrival.Line.VictimSide = VeyraTeams::Opposing(KillerSide);
			Arrival.Line.bFirstBlood = bFirstBlood;
			Arrival.ReceivedAt = At;
			return Arrival;
		}

		TEST_METHOD(TheFeedShowsTheLatestRowsForTheirTime)
		{
			const TArray<FVeyraKillFeedArrival> Arrivals = { Takedown(Ally, EVeyraTeam::A, Enemy, 0.0), Takedown(Enemy, EVeyraTeam::B, Ally, 1.0),
				Takedown(Own, EVeyraTeam::A, Enemy, 2.0) };
			const TArray<FVeyraKillFeedRow> Rows = VeyraKillFeedView::Rows(Arrivals, 3.0, ShowSeconds, MaxRows);
			ASSERT_THAT(IsTrue(Rows.Num() == MaxRows && Rows[0].Line.KillerPlayerId == Enemy && Rows[1].Line.KillerPlayerId == Own, TEXT("the latest, oldest first")));
			ASSERT_THAT(IsTrue(VeyraKillFeedView::Rows(Arrivals, 2.0 + ShowSeconds, ShowSeconds, MaxRows).IsEmpty(), TEXT("each for its time")));
		}

		TEST_METHOD(ThePlayersOwnTakedownsDeathsAndFirstBloodAreAnnounced)
		{
			ASSERT_THAT(IsTrue(VeyraKillFeedView::AnnouncementOf(Takedown(Own, EVeyraTeam::A, Enemy, 0.0).Line, Own, EVeyraTeam::A).StartsWith(TEXT("You slew"))));
			ASSERT_THAT(IsTrue(VeyraKillFeedView::AnnouncementOf(Takedown(Enemy, EVeyraTeam::B, Own, 0.0).Line, Own, EVeyraTeam::A).EndsWith(TEXT("slew you"))));
			ASSERT_THAT(IsTrue(VeyraKillFeedView::AnnouncementOf(Takedown(Ally, EVeyraTeam::A, Enemy, 0.0, true).Line, Own, EVeyraTeam::A).StartsWith(TEXT("First Blood"))));
			ASSERT_THAT(IsTrue(VeyraKillFeedView::AnnouncementOf(Takedown(Ally, EVeyraTeam::A, Enemy, 0.0).Line, Own, EVeyraTeam::A).IsEmpty(), TEXT("others' takedowns stay in the feed")));

			FVeyraKillFeedArrival Spire;
			Spire.Line.Kind = EVeyraKillFeedKind::Structure;
			Spire.Line.VictimSide = EVeyraTeam::B;
			Spire.Line.bHasLane = true;
			Spire.Line.Lane = EVeyraLane::Top;
			const TOptional<FVeyraAnnouncement> Fallen = VeyraKillFeedView::Announcement({ Spire }, 0.5, ShowSeconds, Own, EVeyraTeam::A);
			ASSERT_THAT(IsTrue(Fallen.IsSet() && Fallen->bGood && Fallen->Text == TEXT("Enemy top outer Spire destroyed"), Fallen.IsSet() ? *Fallen->Text : TEXT("none")));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER && WITH_VEYRA_UI
