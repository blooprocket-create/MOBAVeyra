// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER && WITH_VEYRA_UI

#include "Blueprint/UserWidget.h"
#include "Components/ActorTestSpawner.h"
#include "GameFramework/GameStateBase.h"
#include "Progression/VeyraProgressionComponent.h"
#include "Scoreboard/VeyraScoreboard.h"
#include "Scoreboard/VeyraScoreboardModel.h"
#include "Shell/VeyraUIInputSettings.h"
#include "Shop/VeyraShopSubsystem.h"
#include "Statistics/VeyraScoreComponent.h"
#include "VeyraPlayerController.h"
#include "VeyraPlayerState.h"

namespace VeyraScoreboardTests
{
	// Veyra.UI.Scoreboard.*: the in-match scoreboard (ADR-017 §4) shows both teams, the viewer's first,
	// with each player's Vanguard, level, public score and items, from what every client receives.
	TEST_CLASS(Scoreboard, "Veyra.UI")
	{
		FActorTestSpawner Spawner;
		AVeyraPlayerState* Ally = nullptr;
		AVeyraPlayerState* Viewer = nullptr;
		AVeyraPlayerState* Enemy = nullptr;
		AVeyraPlayerState* Spectator = nullptr;

		BEFORE_EACH()
		{
			// Spawned first, so each PlayerState joins its player list.
			Spawner.SpawnActor<AGameStateBase>();
			// Seated out of order, to show the scoreboard keeps seat order.
			Viewer = &Seat(EVeyraTeam::A, 2, TEXT("cairn"), MakeScore(1, 0, 2, 30, 4));
			Enemy = &Seat(EVeyraTeam::B, 3, TEXT("oriel"), MakeScore(0, 1, 0, 25, 0));
			Ally = &Seat(EVeyraTeam::A, 1, TEXT("bryn"), MakeScore(2, 0, 1, 10, 0));
			Spectator = &Spawner.SpawnActor<AVeyraPlayerState>();
		}

		static FVeyraScore MakeScore(int32 Kills, int32 Deaths, int32 Assists, int32 MinionKills, int32 JungleKills)
		{
			FVeyraScore Score;
			Score.Kills = Kills;
			Score.Deaths = Deaths;
			Score.Assists = Assists;
			Score.MinionKills = MinionKills;
			Score.JungleKills = JungleKills;
			return Score;
		}

		AVeyraPlayerState& Seat(EVeyraTeam Side, int32 PlayerId, const TCHAR* Vanguard, const FVeyraScore& Score)
		{
			AVeyraPlayerState& Participant = Spawner.SpawnActor<AVeyraPlayerState>();
			Participant.SetVeyraTeam(Side);
			Participant.SetPlayerId(PlayerId);
			Participant.SetPlayerName(FString::Printf(TEXT("Player %d"), PlayerId));
			Participant.SetVanguardId(FVeyraContentId::FromText(Vanguard).GetValue());
			Participant.FindComponentByClass<UVeyraProgressionComponent>()->Initialize(FVeyraStatGrowth(), 0.0);
			Participant.FindComponentByClass<UVeyraScoreComponent>()->SetScore(Score);
			UVeyraShopSubsystem::InitializeInventory(Participant);
			return Participant;
		}

		static FVeyraScoreboardView Describe(std::initializer_list<const APlayerState*> Participants, const APlayerState* Local)
		{
			return VeyraScoreboardModel::Describe(TArray<const APlayerState*>(Participants), Local);
		}

		TEST_METHOD(TheViewersTeamComesFirstInSeatOrder)
		{
			const FVeyraScoreboardView View = Describe({ Enemy, Viewer, Spectator, Ally }, Viewer);
			ASSERT_THAT(AreEqual(2, View.Sides.Num()));
			const FVeyraScoreboardSide& Ours = View.Sides[0];
			ASSERT_THAT(IsTrue(Ours.Team == EVeyraTeam::A && Ours.bAllies && Ours.Kills == 3));
			ASSERT_THAT(IsTrue(Ours.Rows.Num() == 2 && Ours.Rows[0].Name == Ally->GetPlayerName() && Ours.Rows[1].Name == Viewer->GetPlayerName()));
			ASSERT_THAT(IsTrue(Ours.Rows[1].bLocal && !Ours.Rows[0].bLocal));
			const FVeyraScoreboardSide& Theirs = View.Sides[1];
			ASSERT_THAT(IsTrue(Theirs.Team == EVeyraTeam::B && !Theirs.bAllies && Theirs.Rows.Num() == 1, TEXT("a player on no side is left out")));

			// From the other side, its team leads.
			ASSERT_THAT(IsTrue(Describe({ Enemy, Viewer, Ally }, Enemy).Sides[0].Team == EVeyraTeam::B));
			// A viewer on no side sees Team A first, and neither as its own.
			const FVeyraScoreboardView Neutral = Describe({ Enemy, Viewer, Ally }, Spectator);
			ASSERT_THAT(IsTrue(Neutral.Sides[0].Team == EVeyraTeam::A && !Neutral.Sides[0].bAllies && !Neutral.Sides[1].bAllies));
		}

		TEST_METHOD(ARowIsTheVanguardLevelScoreAndItems)
		{
			const FVeyraScoreboardView View = Describe({ Viewer }, Viewer);
			const FVeyraScoreboardRow& Row = View.Sides[0].Rows[0];
			ASSERT_THAT(IsTrue(Row.Vanguard == FVeyraContentId::FromText(TEXT("cairn")).GetValue() && Row.Level == 1));
			ASSERT_THAT(IsTrue(Row.Kills == 1 && Row.Deaths == 0 && Row.Assists == 2));
			ASSERT_THAT(IsTrue(Row.CreepScore == 34, TEXT("minions and monsters together, as League counts CS")));
			ASSERT_THAT(IsTrue(Row.Items.Num() == 6 && !Row.Items[0].IsValid()));
			ASSERT_THAT(AreEqual(FString(TEXT("1 / 0 / 2")), VeyraScoreboardModel::KdaText(Row).ToString()));
		}

		TEST_METHOD(APlayerWhoLeftKeepsItsLineMarkedAway)
		{
			// A client's GameState drops a PlayerState that goes inactive; the screen still lists it.
			Enemy->SetIsInactive(true);
			Spawner.GetWorld().GetGameState()->RemovePlayerState(Enemy);
			AVeyraPlayerController& Controller = Spawner.SpawnActor<AVeyraPlayerController>();
			Controller.PlayerState = Viewer;
			UVeyraScoreboard* Screen = CreateWidget<UVeyraScoreboard>(&Spawner.GetWorld());
			ASSERT_THAT(IsNotNull(Screen));
			Screen->Show(Controller);
			const FVeyraScoreboardSide& Theirs = Screen->GetView().Sides[1];
			ASSERT_THAT(IsTrue(Theirs.Rows.Num() == 1 && Theirs.Rows[0].Name == Enemy->GetPlayerName() && Theirs.Rows[0].bAway));
			ASSERT_THAT(IsTrue(UVeyraScoreboard::RowLine(Theirs.Rows[0]).ToString().Contains(TEXT("disconnected"))));
			ASSERT_THAT(IsFalse(Screen->GetView().Sides[0].Rows[0].bAway));
		}

		TEST_METHOD(TheScreenShowsBothTeamsAndFollowsTheScore)
		{
			AVeyraPlayerController& Controller = Spawner.SpawnActor<AVeyraPlayerController>();
			Controller.PlayerState = Viewer;
			UVeyraScoreboard* Screen = CreateWidget<UVeyraScoreboard>(&Spawner.GetWorld());
			ASSERT_THAT(IsNotNull(Screen));
			Screen->Show(Controller);
			ASSERT_THAT(IsTrue(Screen->GetView() == Describe({ Ally, Viewer, Enemy, Spectator }, Viewer)));
			const TArray<FString> Lines = Screen->GetLines();
			const FVeyraScoreboardView& View = Screen->GetView();
			ASSERT_THAT(IsTrue(Lines.Contains(UVeyraScoreboard::SideHeading(View.Sides[0]).ToString())));
			ASSERT_THAT(IsTrue(Lines.Contains(UVeyraScoreboard::SideHeading(View.Sides[1]).ToString())));
			for (const FVeyraScoreboardSide& Side : View.Sides)
			{
				for (const FVeyraScoreboardRow& Row : Side.Rows)
				{
					ASSERT_THAT(IsTrue(Lines.Contains(UVeyraScoreboard::RowLine(Row).ToString()) && Lines.Contains(UVeyraScoreboard::ItemsLine(Row).ToString())));
				}
			}

			// A kill the server reports shows at once.
			Enemy->FindComponentByClass<UVeyraScoreComponent>()->SetScore(MakeScore(1, 1, 0, 25, 0));
			Screen->Show(Controller);
			ASSERT_THAT(IsTrue(Screen->GetView().Sides[1].Kills == 1));
			ASSERT_THAT(IsTrue(Screen->GetLines().Contains(UVeyraScoreboard::RowLine(Screen->GetView().Sides[1].Rows[0]).ToString())));
		}

		TEST_METHOD(ItHasItsOwnKeyTabByDefault)
		{
			ASSERT_THAT(IsTrue(GetDefault<UVeyraUIInputSettings>()->ScoreboardKey == EKeys::Tab, TEXT("League's key")));
			UVeyraUIInputSettings* Settings = NewObject<UVeyraUIInputSettings>();
			Settings->MatchMenuKey = EKeys::Escape;
			Settings->ShopKey = EKeys::P;
			Settings->ScoreboardKey = EKeys::Tab;
			ASSERT_THAT(IsTrue(Settings->Validate().IsEmpty()));
			Settings->ScoreboardKey = EKeys::P;
			ASSERT_THAT(IsTrue(FString::Join(Settings->Validate(), TEXT(" ")).Contains(TEXT("ScoreboardKey"))));
			Settings->ScoreboardKey = FKey();
			ASSERT_THAT(IsTrue(FString::Join(Settings->Validate(), TEXT(" ")).Contains(TEXT("ScoreboardKey"))));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER && WITH_VEYRA_UI
