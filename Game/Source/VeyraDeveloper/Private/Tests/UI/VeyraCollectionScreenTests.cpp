// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER && WITH_VEYRA_UI

#include "Blueprint/UserWidget.h"
#include "Components/ActorTestSpawner.h"
#include "Shell/VeyraProgressionModels.h"
#include "Shell/VeyraShellButton.h"
#include "Shell/VeyraShellModels.h"
#include "Shell/VeyraShellScreen.h"
#include "Tests/Services/VeyraClientFlowTestRig.h"

namespace VeyraCollectionScreenTests
{
	using namespace VeyraClientFlowTests;
	using VeyraBackendProtocol::ECurrency;

	// Fixture answers, independent of the committed backend configuration.
	FString ProgressionOf(int64 Flux, int64 RefinedFlux)
	{
		return FString::Printf(TEXT("{\"progression\":{\"level\":7,\"levelXp\":40,\"levelNeed\":170,\"lifetimeXp\":1210,\"flux\":%lld,\"refinedFlux\":%lld}}"), Flux, RefinedFlux);
	}

	FString Entry(const TCHAR* Vanguard, bool bOwned, const TCHAR* Source, bool bRotation)
	{
		return FString::Printf(TEXT("{\"vanguardId\":\"%s\",\"owned\":%s,\"source\":%s,\"rotation\":%s,\"price\":{\"flux\":3000,\"refinedFlux\":550},")
								   TEXT("\"purchasable\":%s,\"mastery\":{\"level\":2,\"levelPoints\":120,\"levelNeed\":1500,\"lifetimePoints\":1120,\"emoteTier\":1}}"),
			Vanguard, bOwned ? TEXT("true") : TEXT("false"), Source ? *FString::Printf(TEXT("\"%s\""), Source) : TEXT("null"), bRotation ? TEXT("true") : TEXT("false"),
			bOwned ? TEXT("false") : TEXT("true"));
	}

	FString Collection()
	{
		return FString::Printf(TEXT("{\"vanguards\":[%s,%s,%s]}"), *Entry(TEXT("cairn"), true, TEXT("starter"), false), *Entry(TEXT("oriel"), false, nullptr, true),
			*Entry(TEXT("bryn"), false, nullptr, false));
	}

	// Veyra.UI.CollectionScreen.*: the top bar's account readout, the Collection page and its confirmed Buy, and
	// a result's rewards (ADR-045 §8), shown from the coordinator and clicked as the player would click them.
	TEST_CLASS(CollectionScreen, "Veyra.UI")
	{
		FActorTestSpawner Spawner;
		FClientFlowTestRig Rig;
		UVeyraShellScreen* Screen = nullptr;

		AFTER_EACH()
		{
			if (Screen)
			{
				Screen->Unbind();
			}
		}

		/** The shell's screen over the coordinator, the level and balances read as Progression. */
		bool ShowShell(const FString& Progression)
		{
			if (!Rig.ReachShell())
			{
				return false;
			}
			Screen = CreateWidget<UVeyraShellScreen>(&Spawner.GetWorld());
			Screen->Bind(*Rig.Flow);
			return Rig.Backend.Answer(TEXT("GET"), TEXT("/v1/me/progression"), 200, Progression);
		}

		bool Press(const FText& Label)
		{
			UVeyraShellButton* Found = Screen->FindButton(Label);
			if (!Found || !Found->GetIsEnabled())
			{
				return false;
			}
			Found->Press();
			return true;
		}

		/** The Collection page, opened from its tab and read. */
		bool OpenCollection()
		{
			return Press(FText::FromString(TEXT("Collection"))) && Rig.Backend.Answer(TEXT("GET"), TEXT("/v1/me/collection"), 200, Collection())
				&& Screen->GetPage() == EVeyraShellPage::Collection;
		}

		TEST_METHOD(TheTopBarShowsTheLevelItsXPAndTheBalances)
		{
			ASSERT_THAT(IsTrue(ShowShell(ProgressionOf(1200, 250))));
			const FVeyraProgressionBarModel Bar = VeyraProgressionModels::DescribeBar(Rig.Flow->GetSnapshot().Progression);
			const FString Text = Screen->DescribeText();
			ASSERT_THAT(IsTrue(Text.Contains(Bar.Level.ToString()) && Text.Contains(Bar.XP.ToString()) && Text.Contains(Bar.Currencies.ToString()), Text));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Bar.Fraction, 40.0f / 170.0f)));
			ASSERT_THAT(IsFalse(VeyraProgressionModels::DescribeBar({}).bShown, TEXT("nothing before the first read")));
		}

		TEST_METHOD(TheCollectionShowsEveryVanguardOwnedOrNot)
		{
			ASSERT_THAT(IsTrue(ShowShell(ProgressionOf(1200, 250))));
			ASSERT_THAT(IsTrue(OpenCollection()));
			const FString Text = Screen->DescribeText();
			// Owned, Free Rotation or Locked (UX-19).
			for (const TCHAR* Line : { TEXT("Cairn"), TEXT("Oriel"), TEXT("Bryn"), TEXT("Owned"), TEXT("Free Rotation"), TEXT("Locked"), TEXT("Mastery 2") })
			{
				ASSERT_THAT(IsTrue(Text.Contains(Line), Line));
			}
			for (const TCHAR* Vanguard : { TEXT("cairn"), TEXT("oriel"), TEXT("bryn") })
			{
				ASSERT_THAT(IsNotNull(Screen->FindButton(VeyraProgressionModels::CollectionCardLabel(Vanguard)), Vanguard));
			}
			// An owned Vanguard's card shows its Mastery and offers no Buy.
			ASSERT_THAT(IsTrue(Press(VeyraProgressionModels::CollectionCardLabel(TEXT("cairn")))));
			ASSERT_THAT(IsTrue(Screen->DescribeText().Contains(TEXT("Mastery Level 2"))));
			ASSERT_THAT(IsNull(Screen->FindButton(VeyraProgressionModels::BuyLabel(TEXT("cairn"), ECurrency::Flux, 3000))));
		}

		TEST_METHOD(TheRosterFilterNarrowsByNameAndTabOnly)
		{
			// Owned wins over the rotation; Favorites reads the favorite alone (ADR-058 §2).
			const FVeyraRosterEntry Owned{ FText::FromString(TEXT("Cairn")), true, true, false };
			const FVeyraRosterEntry Lent{ FText::FromString(TEXT("Oriel")), false, true, true };
			const FVeyraRosterEntry Locked{ FText::FromString(TEXT("Bryn")), false, false, false };
			ASSERT_THAT(IsTrue(VeyraRosterFilter::Shows(Owned, EVeyraRosterTab::All, FString()) && VeyraRosterFilter::Shows(Locked, EVeyraRosterTab::All, FString())));
			ASSERT_THAT(IsTrue(VeyraRosterFilter::Shows(Owned, EVeyraRosterTab::Owned, FString()) && !VeyraRosterFilter::Shows(Lent, EVeyraRosterTab::Owned, FString())));
			ASSERT_THAT(IsTrue(VeyraRosterFilter::Shows(Lent, EVeyraRosterTab::FreeRotation, FString()) && !VeyraRosterFilter::Shows(Owned, EVeyraRosterTab::FreeRotation, FString())));
			ASSERT_THAT(IsTrue(VeyraRosterFilter::Shows(Lent, EVeyraRosterTab::Favorites, FString()) && !VeyraRosterFilter::Shows(Owned, EVeyraRosterTab::Favorites, FString())));
			ASSERT_THAT(IsTrue(VeyraRosterFilter::Shows(Lent, EVeyraRosterTab::All, TEXT(" ORI ")) && !VeyraRosterFilter::Shows(Owned, EVeyraRosterTab::All, TEXT("ori"))));
		}

		TEST_METHOD(TheRosterSearchesByNameAndNarrowsToOwnedOrFreeRotation)
		{
			ASSERT_THAT(IsTrue(ShowShell(ProgressionOf(1200, 250))));
			ASSERT_THAT(IsTrue(OpenCollection()));
			const auto Shown = [this](const TCHAR* Id) { return Screen->IsRosterCardShown(Id); };
			ASSERT_THAT(IsTrue(Shown(TEXT("cairn")) && Shown(TEXT("oriel")) && Shown(TEXT("bryn")), TEXT("every Vanguard under All")));
			// By name, ignoring case (UX-19; ADR-058 §3).
			Screen->SetRosterSearch(TEXT(" ORI"));
			ASSERT_THAT(IsTrue(Shown(TEXT("oriel")) && !Shown(TEXT("cairn")) && !Shown(TEXT("bryn"))));
			Screen->SetRosterSearch(TEXT("nobody"));
			ASSERT_THAT(IsTrue(!Shown(TEXT("oriel")) && !Shown(TEXT("cairn")) && !Shown(TEXT("bryn"))));
			Screen->SetRosterSearch(FString());
			// Owned takes the starter; Free Rotation the Vanguard lent this week; filters never grant: the locked one stays under All.
			ASSERT_THAT(IsTrue(Press(UVeyraShellScreen::RosterTabLabel(EVeyraRosterTab::Owned))));
			ASSERT_THAT(IsTrue(Shown(TEXT("cairn")) && !Shown(TEXT("oriel")) && !Shown(TEXT("bryn"))));
			ASSERT_THAT(IsTrue(Press(UVeyraShellScreen::RosterTabLabel(EVeyraRosterTab::FreeRotation))));
			ASSERT_THAT(IsTrue(Shown(TEXT("oriel")) && !Shown(TEXT("cairn")) && !Shown(TEXT("bryn"))));
			ASSERT_THAT(IsTrue(Press(UVeyraShellScreen::RosterTabLabel(EVeyraRosterTab::All)) && Shown(TEXT("bryn"))));
			ASSERT_THAT(IsNull(Screen->FindButton(UVeyraShellScreen::RosterTabLabel(EVeyraRosterTab::Favorites)), TEXT("Favorites belongs to champion select")));
		}

		TEST_METHOD(AnOpenedCardMarksAndUnmarksAFavoriteOwnedOrNot)
		{
			ASSERT_THAT(IsTrue(ShowShell(ProgressionOf(1200, 250))));
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("GET"), TEXT("/v1/me/favorites"), 200, TEXT("{\"favorites\":[]}"))));
			ASSERT_THAT(IsTrue(OpenCollection()));
			// A locked Vanguard may be a favorite too (UX-30; ADR-058 §3).
			ASSERT_THAT(IsTrue(Press(VeyraProgressionModels::CollectionCardLabel(TEXT("bryn")))));
			ASSERT_THAT(IsTrue(Press(VeyraProgressionModels::FavoriteLabel(TEXT("bryn"), false))));
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("PUT"), TEXT("/v1/me/favorites/bryn"), 200, TEXT("{\"favorites\":[\"bryn\"]}"))));
			ASSERT_THAT(IsTrue(Screen->DescribeText().Contains(TEXT("Locked · Favorite")), Screen->DescribeText()));
			// The card stays open, its toggle now taking the favorite back; a refusal shows in the Collection.
			ASSERT_THAT(IsTrue(Press(VeyraProgressionModels::FavoriteLabel(TEXT("bryn"), true))));
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("DELETE"), TEXT("/v1/me/favorites/bryn"), 409, ErrorBody(TEXT("playing")))));
			ASSERT_THAT(IsTrue(Screen->DescribeText().Contains(TEXT("Favorites change outside champion select and matches."))));
			ASSERT_THAT(IsTrue(Press(VeyraProgressionModels::FavoriteLabel(TEXT("bryn"), true))));
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("DELETE"), TEXT("/v1/me/favorites/bryn"), 200, TEXT("{\"favorites\":[]}"))));
			ASSERT_THAT(IsFalse(Screen->DescribeText().Contains(TEXT("Favorite ·")) || Screen->DescribeText().Contains(TEXT("· Favorite"))));
			ASSERT_THAT(IsNotNull(Screen->FindButton(VeyraProgressionModels::FavoriteLabel(TEXT("bryn"), false))));
		}

		TEST_METHOD(ACardWithNoMasteryPointsSaysSo)
		{
			FVeyraClientSnapshot Snapshot;
			Snapshot.Collection.bLoaded = true;
			VeyraBackendProtocol::FCollectionEntry& Fresh = Snapshot.Collection.Vanguards.AddDefaulted_GetRef();
			Fresh.VanguardId = TEXT("bryn");
			const FVeyraCollectionModel Model = VeyraProgressionModels::DescribeCollection(Snapshot, false);
			ASSERT_THAT(AreEqual(Model.Cards[0].MasteryShort.ToString(), FString(TEXT("No Mastery Progress"))));
		}

		TEST_METHOD(BuyAsksFirstNamingThePriceThenBuys)
		{
			ASSERT_THAT(IsTrue(ShowShell(ProgressionOf(4000, 300))));
			ASSERT_THAT(IsTrue(OpenCollection()));
			ASSERT_THAT(IsTrue(Press(VeyraProgressionModels::CollectionCardLabel(TEXT("bryn")))));
			const FText BuyFlux = VeyraProgressionModels::BuyLabel(TEXT("bryn"), ECurrency::Flux, 3000);
			const FText BuyRefined = VeyraProgressionModels::BuyLabel(TEXT("bryn"), ECurrency::RefinedFlux, 550);
			ASSERT_THAT(IsTrue(Screen->FindButton(BuyFlux) && Screen->FindButton(BuyFlux)->GetIsEnabled()));
			ASSERT_THAT(IsTrue(Screen->FindButton(BuyRefined) && !Screen->FindButton(BuyRefined)->GetIsEnabled(), TEXT("300 Refined Flux does not cover 550")));
			// Browsing never spends: the first click only asks.
			ASSERT_THAT(IsTrue(Press(BuyFlux)));
			ASSERT_THAT(IsNull(Rig.Backend.Find(TEXT("POST"), TEXT("/v1/me/purchases"))));
			const FString Prompt = VeyraProgressionModels::ConfirmBuyPrompt(TEXT("bryn"), ECurrency::Flux, 3000, Rig.Flow->GetSnapshot().Progression).ToString();
			ASSERT_THAT(IsTrue(Screen->DescribeText().Contains(Prompt), Prompt));
			// Cancelling takes the question back; asking again and confirming buys.
			ASSERT_THAT(IsTrue(Press(VeyraShellModels::CancelConfirmLabel())));
			ASSERT_THAT(IsFalse(Screen->DescribeText().Contains(Prompt)));
			ASSERT_THAT(IsTrue(Press(BuyFlux)));
			ASSERT_THAT(IsTrue(Press(VeyraProgressionModels::ConfirmBuyLabel(TEXT("bryn"), ECurrency::Flux, 3000))));
			const FFlowTestBackend::FRequest* Request = Rig.Backend.Find(TEXT("POST"), TEXT("/v1/me/purchases"));
			ASSERT_THAT(IsNotNull(Request));
			ASSERT_THAT(IsTrue(Request->Body.Contains(TEXT("\"vanguardId\":\"bryn\"")) && Request->Body.Contains(TEXT("\"currency\":\"flux\""))));
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("POST"), TEXT("/v1/me/purchases"), 409, ErrorBody(TEXT("insufficient_balance")))));
			ASSERT_THAT(IsTrue(Screen->DescribeText().Contains(TEXT("Not enough to buy Bryn."))));
		}

		TEST_METHOD(BuyWaitsUntilTheBalanceIsKnown)
		{
			// The shell's read of the balance has not answered yet.
			ASSERT_THAT(IsTrue(Rig.ReachShell()));
			Screen = CreateWidget<UVeyraShellScreen>(&Spawner.GetWorld());
			Screen->Bind(*Rig.Flow);
			ASSERT_THAT(IsTrue(OpenCollection()));
			ASSERT_THAT(IsTrue(Press(VeyraProgressionModels::CollectionCardLabel(TEXT("bryn")))));
			const FText BuyFlux = VeyraProgressionModels::BuyLabel(TEXT("bryn"), ECurrency::Flux, 3000);
			ASSERT_THAT(IsTrue(Screen->FindButton(BuyFlux) && !Screen->FindButton(BuyFlux)->GetIsEnabled(), TEXT("an unread balance covers nothing")));
			ASSERT_THAT(IsTrue(Screen->DescribeText().Contains(TEXT("Reading your balance"))));
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("GET"), TEXT("/v1/me/progression"), 200, ProgressionOf(4000, 300))));
			ASSERT_THAT(IsTrue(Screen->FindButton(BuyFlux) && Screen->FindButton(BuyFlux)->GetIsEnabled()));
		}

		TEST_METHOD(TheResultsSayWhatTheMatchGaveApartFromItsGold)
		{
			VeyraBackendProtocol::FMatchOutcome Outcome;
			Outcome.Rewards.Emplace();
			Outcome.Rewards->AccountXP = 181;
			Outcome.Rewards->LevelBefore = 6;
			Outcome.Rewards->LevelAfter = 7;
			Outcome.Rewards->Flux = 400;
			Outcome.Rewards->VanguardId = TEXT("cairn");
			Outcome.Rewards->MasteryPoints = 477;
			Outcome.Rewards->MasteryBefore = 2;
			Outcome.Rewards->MasteryAfter = 3;
			const FVeyraRewardsModel Earned = VeyraProgressionModels::DescribeRewards(Outcome);
			const FString Lines = FText::Join(FText::FromString(TEXT("\n")), Earned.Lines).ToString();
			for (const TCHAR* Line : { TEXT("+181 account XP"), TEXT("Level up: Level 6 to 7"), TEXT("+400 Flux"), TEXT("Cairn Mastery points"), TEXT("Cairn Mastery Level 2 to 3") })
			{
				ASSERT_THAT(IsTrue(Earned.bShown && Lines.Contains(Line), Line));
			}
			// Co-op past its level: the reason, and the Mastery it still gave.
			Outcome.Rewards->Reason = TEXT("coop_level");
			Outcome.Rewards->AccountXP = Outcome.Rewards->Flux = 0;
			Outcome.Rewards->LevelAfter = 6;
			const TArray<FText> Withheld = VeyraProgressionModels::DescribeRewards(Outcome).Lines;
			ASSERT_THAT(AreEqual(3, Withheld.Num(), TEXT("the reason, the Mastery points and the Mastery level")));
			ASSERT_THAT(IsTrue(Withheld[0].ToString().Contains(TEXT("no longer gives you account XP")) && Withheld[1].ToString().Contains(TEXT("Mastery points"))));
			ASSERT_THAT(IsFalse(VeyraProgressionModels::DescribeRewards(VeyraBackendProtocol::FMatchOutcome()).bShown, TEXT("before adjudication")));
		}

		TEST_METHOD(TheResultsScreenShowsTheRewardsPanel)
		{
			ASSERT_THAT(IsTrue(Rig.ReachResults(RewardedOutcomeBody())));
			Screen = CreateWidget<UVeyraShellScreen>(&Spawner.GetWorld());
			Screen->Bind(*Rig.Flow);
			const FString Text = Screen->DescribeText();
			ASSERT_THAT(IsTrue(Text.Contains(TEXT("Your rewards")) && Text.Contains(TEXT("+181 account XP")) && Text.Contains(TEXT("Level up: Level 6 to 7")), Text));
		}

		TEST_METHOD(TheResultsScreenSaysRewardsAreComingThenShowsThem)
		{
			Rig.ShellProgression = ProgressionOf(1200, 250);
			ASSERT_THAT(IsTrue(Rig.ReachResults(ScoredOutcomeBody())));
			Screen = CreateWidget<UVeyraShellScreen>(&Spawner.GetWorld());
			Screen->Bind(*Rig.Flow);
			FString Text = Screen->DescribeText();
			ASSERT_THAT(IsTrue(Text.Contains(TEXT("Your rewards")) && Text.Contains(TEXT("still being counted")), Text));
			Rig.Advance(FClientFlowTestRig::ResultPollSeconds);
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("GET"), MatchOutcomePath(), 200, RewardedOutcomeBody())));
			Text = Screen->DescribeText();
			ASSERT_THAT(IsTrue(Text.Contains(TEXT("+181 account XP")) && !Text.Contains(TEXT("still being counted")), Text));
		}
	};
}

#endif
