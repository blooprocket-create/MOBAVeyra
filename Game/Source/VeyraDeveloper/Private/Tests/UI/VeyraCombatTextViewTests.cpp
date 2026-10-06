// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER && WITH_VEYRA_UI

#include "Components/ActorTestSpawner.h"
#include "Greybox/VeyraGreyboxSettings.h"
#include "Hud/VeyraCombatTextModel.h"
#include "Settings/VeyraInterfacePreferences.h"
#include "VeyraSettingsStore.h"
#include "VeyraSettingsSubsystem.h"

namespace VeyraCombatTextViewTests
{
	// Veyra.UI.CombatTextView.*: how the client shows the combat text it receives (Settings Bible §3.4; ADR-052 §1).
	TEST_CLASS(CombatTextView, "Veyra.UI")
	{
		// Fixture timing, independent of the committed HUD settings.
		static constexpr double ShowSeconds = 1.2;
		static constexpr double MergeSeconds = 0.4;
		static constexpr double GoldMergeSeconds = 0.15;

		FActorTestSpawner Spawner;

		static FVeyraCombatTextOptions OptionsOf(bool bReduced)
		{
			FVeyraCombatTextOptions Options;
			Options.bReduced = bReduced;
			Options.MergeSeconds = MergeSeconds;
			Options.ShowSeconds = ShowSeconds;
			return Options;
		}

		static FVeyraCombatTextArrival ArrivalOf(AActor& Unit, AActor* Other, EVeyraCombatTextKind Kind, double Amount, double At,
			EVeyraDamageType Type = EVeyraDamageType::Physical, bool bCritical = false)
		{
			FVeyraCombatTextArrival Arrival;
			Arrival.Line.Unit = &Unit;
			Arrival.Line.Other = Other;
			Arrival.Line.Kind = Kind;
			Arrival.Line.DamageType = Type;
			Arrival.Line.bCritical = bCritical;
			Arrival.Line.Amount = static_cast<float>(Amount);
			Arrival.ReceivedAt = At;
			return Arrival;
		}

		TEST_METHOD(ReducedDensityMergesQuickNumbersBetweenTheSameUnitsAndStandardDoesNot)
		{
			AActor& Target = Spawner.SpawnActor<AActor>();
			AActor& Dealer = Spawner.SpawnActor<AActor>();
			AActor& Companion = Spawner.SpawnActor<AActor>();
			// In the order they arrive; another type, and another source, keep their own totals.
			const TArray<FVeyraCombatTextArrival> Arrivals = {
				ArrivalOf(Target, &Dealer, EVeyraCombatTextKind::DamageDealt, 10.0, 0.0),
				ArrivalOf(Target, &Dealer, EVeyraCombatTextKind::DamageDealt, 4.0, 0.1, EVeyraDamageType::Magic),
				ArrivalOf(Target, &Companion, EVeyraCombatTextKind::DamageDealt, 3.0, 0.2),
				ArrivalOf(Target, &Dealer, EVeyraCombatTextKind::DamageDealt, 12.0, 0.3),
				ArrivalOf(Target, &Dealer, EVeyraCombatTextKind::DamageDealt, 5.0, 0.6),
			};
			ASSERT_THAT(IsTrue(VeyraCombatTextView::Describe(Arrivals, 0.7, OptionsOf(false)).Num() == 5, TEXT("Standard shows every number")));

			const TArray<FVeyraCombatTextShown> Reduced = VeyraCombatTextView::Describe(Arrivals, 0.7, OptionsOf(true));
			ASSERT_THAT(IsTrue(Reduced.Num() == 3));
			ASSERT_THAT(IsTrue(Reduced[0].Amount == 27.0 && Reduced[0].Unit.Get() == &Target, TEXT("a running total")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Reduced[0].Progress, 0.1 / ShowSeconds), TEXT("which shows from its latest part")));
			ASSERT_THAT(IsTrue(Reduced[1].Amount == 4.0 && Reduced[1].DamageType == EVeyraDamageType::Magic && Reduced[2].Amount == 3.0));

			// Past the merge window, a new total begins.
			TArray<FVeyraCombatTextArrival> Later = Arrivals;
			Later.Add(ArrivalOf(Target, &Dealer, EVeyraCombatTextKind::DamageDealt, 7.0, 0.6 + MergeSeconds + 0.1));
			const TArray<FVeyraCombatTextShown> Again = VeyraCombatTextView::Describe(Later, 1.2, OptionsOf(true));
			ASSERT_THAT(IsTrue(Again.Num() == 4 && Again[0].Amount == 27.0 && Again[3].Amount == 7.0));
		}

		TEST_METHOD(GoldMergesAtAnyDensityWhereItWasEarnedAndItsSettingHidesIt)
		{
			AActor& Own = Spawner.SpawnActor<AActor>();
			const FVector Fell(100.0, 200.0, 0.0);
			FVeyraCombatTextArrival LastHit;
			LastHit.Line.Kind = EVeyraCombatTextKind::Gold;
			LastHit.Line.bFixed = true;
			LastHit.Line.Where = Fell;
			LastHit.Line.Amount = 21.0f;
			// A kill over the player's own Vanguard, its bounty at the same moment, and a last hit where a unit fell.
			const TArray<FVeyraCombatTextArrival> Arrivals = {
				ArrivalOf(Own, nullptr, EVeyraCombatTextKind::Gold, 300.0, 0.0),
				ArrivalOf(Own, nullptr, EVeyraCombatTextKind::Gold, 150.0, GoldMergeSeconds / 2.0),
				LastHit,
			};
			FVeyraCombatTextOptions Options = OptionsOf(/*bReduced*/ false);
			Options.GoldMergeSeconds = GoldMergeSeconds;
			const TArray<FVeyraCombatTextShown> Shown = VeyraCombatTextView::Describe(Arrivals, GoldMergeSeconds, Options);
			ASSERT_THAT(IsTrue(Shown.Num() == 2 && Shown[0].Amount == 450.0 && !Shown[0].bFixed, TEXT("a kill and its bounty read as one")));
			ASSERT_THAT(IsTrue(Shown[1].bFixed && Shown[1].Where.Equals(Fell) && Shown[1].Amount == 21.0, TEXT("a last hit stays where its unit fell")));
			Options.bGold = false;
			ASSERT_THAT(IsTrue(VeyraCombatTextView::Describe(Arrivals, GoldMergeSeconds, Options).IsEmpty()));
		}

		TEST_METHOD(TurnedOffKindsShowNothingAndCritEmphasisIsTheirs)
		{
			AActor& Unit = Spawner.SpawnActor<AActor>();
			const TArray<FVeyraCombatTextArrival> Arrivals = {
				ArrivalOf(Unit, nullptr, EVeyraCombatTextKind::DamageDealt, 30.0, 0.0, EVeyraDamageType::Physical, /*bCritical*/ true),
				ArrivalOf(Unit, nullptr, EVeyraCombatTextKind::DamageReceived, 20.0, 0.0),
				ArrivalOf(Unit, nullptr, EVeyraCombatTextKind::Healing, 15.0, 0.0),
				ArrivalOf(Unit, nullptr, EVeyraCombatTextKind::Shielding, 25.0, 0.0),
			};
			FVeyraCombatTextOptions Options = OptionsOf(false);
			ASSERT_THAT(IsTrue(VeyraCombatTextView::Describe(Arrivals, 0.1, Options)[0].bCritical, TEXT("a crit stands out")));
			Options.bCritEmphasis = false;
			ASSERT_THAT(IsFalse(VeyraCombatTextView::Describe(Arrivals, 0.1, Options)[0].bCritical, TEXT("unless the player turned that off")));
			Options.bDamageDealt = false;
			Options.bHealing = false;
			const TArray<FVeyraCombatTextShown> Shown = VeyraCombatTextView::Describe(Arrivals, 0.1, Options);
			ASSERT_THAT(IsTrue(Shown.Num() == 2 && Shown[0].Kind == EVeyraCombatTextKind::DamageReceived && Shown[1].Kind == EVeyraCombatTextKind::Shielding));
			Options.bDamageReceived = false;
			Options.bShielding = false;
			ASSERT_THAT(IsTrue(VeyraCombatTextView::Describe(Arrivals, 0.1, Options).IsEmpty()));
		}

		TEST_METHOD(ANumberShowsForItsTimeAndTheTinyNeverShow)
		{
			AActor& Unit = Spawner.SpawnActor<AActor>();
			TArray<FVeyraCombatTextArrival> Arrivals = {
				ArrivalOf(Unit, nullptr, EVeyraCombatTextKind::DamageDealt, 40.0, 0.0),
				ArrivalOf(Unit, nullptr, EVeyraCombatTextKind::Healing, 0.3, 0.0),
			};
			const TArray<FVeyraCombatTextShown> Shown = VeyraCombatTextView::Describe(Arrivals, 0.6, OptionsOf(false));
			ASSERT_THAT(IsTrue(Shown.Num() == 1 && FMath::IsNearlyEqual(Shown[0].Progress, 0.5), TEXT("halfway through, and nothing that rounds to 0")));
			ASSERT_THAT(IsTrue(VeyraCombatTextView::Describe(Arrivals, ShowSeconds, OptionsOf(false)).IsEmpty(), TEXT("gone once shown")));
			VeyraCombatTextView::Forget(Arrivals, ShowSeconds - 0.1, OptionsOf(false));
			ASSERT_THAT(IsTrue(Arrivals.Num() == 2));
			VeyraCombatTextView::Forget(Arrivals, ShowSeconds, OptionsOf(false));
			ASSERT_THAT(IsTrue(Arrivals.IsEmpty()));
		}

		TEST_METHOD(ReducedDensityKeepsEveryPartOfATotalWhileItShows)
		{
			AActor& Target = Spawner.SpawnActor<AActor>();
			AActor& Dealer = Spawner.SpawnActor<AActor>();
			// Two quick hits merge, and their total shows from the second.
			constexpr double Second = MergeSeconds - 0.1;
			const TArray<FVeyraCombatTextArrival> Hits = {
				ArrivalOf(Target, &Dealer, EVeyraCombatTextKind::DamageDealt, 10.0, 0.0),
				ArrivalOf(Target, &Dealer, EVeyraCombatTextKind::DamageDealt, 12.0, Second),
			};
			TArray<FVeyraCombatTextArrival> Standard = Hits;
			VeyraCombatTextView::Forget(Standard, ShowSeconds, OptionsOf(false));
			ASSERT_THAT(IsTrue(Standard.Num() == 1, TEXT("Standard drops each number in its own time")));

			TArray<FVeyraCombatTextArrival> Reduced = Hits;
			VeyraCombatTextView::Forget(Reduced, ShowSeconds, OptionsOf(true));
			ASSERT_THAT(IsTrue(Reduced.Num() == 2, TEXT("past the first part's own time, the total still shows")));
			const TArray<FVeyraCombatTextShown> Shown = VeyraCombatTextView::Describe(Reduced, ShowSeconds, OptionsOf(true));
			ASSERT_THAT(IsTrue(Shown.Num() == 1 && Shown[0].Amount == 22.0, TEXT("whole")));
			VeyraCombatTextView::Forget(Reduced, Second + ShowSeconds, OptionsOf(true));
			ASSERT_THAT(IsTrue(Reduced.IsEmpty(), TEXT("its parts go with it")));
		}

		TEST_METHOD(ThePlayersCombatTextSettingsTakeTheirPlace)
		{
			using namespace VeyraInterfacePreferences;
			FVeyraSettingsRegistry Registry;
			UVeyraSettingsSubsystem::LoadRegistry(Registry);
			const UVeyraGreyboxSettings& Hud = *GetDefault<UVeyraGreyboxSettings>();
			const FVeyraSettingsStore Defaults(Registry);
			const FVeyraInterfacePreferences Untouched = Resolve(Hud, &Defaults);
			const FVeyraCombatTextOptions& On = Untouched.CombatText;
			ASSERT_THAT(IsTrue(On.bDamageDealt && On.bDamageReceived && On.bHealing && On.bShielding && On.bCritEmphasis && !On.bReduced && !Untouched.bUniformDamageColors,
				TEXT("every number shows, Standard and Color-Coded, by default")));
			ASSERT_THAT(IsTrue(On.ShowSeconds == Hud.CombatTextShowSeconds && On.MergeSeconds == Hud.CombatTextMergeSeconds));

			FVeyraSettingsStore Store(Registry);
			Store.Set(CombatTextDamageReceived(), VeyraSettings::Off());
			Store.Set(CombatTextCrits(), VeyraSettings::Off());
			Store.Set(CombatTextDensity(), TEXT("Reduced"));
			Store.Set(DamageNumberColors(), TEXT("Uniform"));
			const FVeyraInterfacePreferences Changed = Resolve(Hud, &Store);
			ASSERT_THAT(IsTrue(!Changed.CombatText.bDamageReceived && !Changed.CombatText.bCritEmphasis && Changed.CombatText.bReduced && Changed.bUniformDamageColors));
		}

		TEST_METHOD(EachDamageTypeHasItsOwnColour)
		{
			// Proposal 52: Color-Coded tells Physical, Magic and True damage apart.
			const UVeyraGreyboxSettings& Hud = *GetDefault<UVeyraGreyboxSettings>();
			ASSERT_THAT(IsTrue(Hud.CombatTextPhysicalColor != Hud.CombatTextMagicColor && Hud.CombatTextMagicColor != Hud.CombatTextTrueColor
				&& Hud.CombatTextPhysicalColor != Hud.CombatTextTrueColor));
			ASSERT_THAT(IsTrue(Hud.CombatTextHealingColor != Hud.CombatTextShieldingColor));
			ASSERT_THAT(IsTrue(Hud.Validate().IsEmpty(), FString::Join(Hud.Validate(), TEXT(" "))));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER && WITH_VEYRA_UI
