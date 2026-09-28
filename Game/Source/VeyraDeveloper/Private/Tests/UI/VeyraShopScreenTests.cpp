// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER && WITH_VEYRA_UI

#include "Blueprint/UserWidget.h"
#include "Components/ActorTestSpawner.h"
#include "Gold/VeyraGoldComponent.h"
#include "Hud/VeyraHudModel.h"
#include "Inventory/VeyraInventoryComponent.h"
#include "Shell/VeyraShellButton.h"
#include "Shop/VeyraShopModel.h"
#include "Shop/VeyraShopScreen.h"
#include "Shop/VeyraShopSubsystem.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "Tests/Items/VeyraItemsTestCatalog.h"
#include "Tuning/VeyraItemsTuningSubsystem.h"
#include "VeyraPlayerController.h"
#include "VeyraPlayerState.h"

namespace VeyraItemsTests
{
	// Veyra.UI.ShopScreen.*: the shop screen (ADR-012 §11) shows the participant's Gold, items and queue
	// and each item's price now, by the rules the server uses, and sends its requests through the
	// player's controller.
	TEST_CLASS(ShopScreen, "Veyra.UI")
	{
		// Fixture values: enough Gold for a component and then an assembly built on it, not a Masterwork.
		static constexpr double Purse = 1500.0;
		static constexpr double GripPower = 10.0;

		FActorTestSpawner Spawner;
		FVeyraItemsTuning Tuning = TestCatalog();
		UVeyraShopSubsystem* Subsystem = nullptr;
		AVeyraPlayerState* Participant = nullptr;

		BEFORE_EACH()
		{
			Tuning.Items[ItemId(TEXT("test_grip"))].Stats.PhysicalPower = GripPower;
			UVeyraItemsTuningSubsystem::SetTestOverride(&Tuning);
			Subsystem = Spawner.GetWorld().GetSubsystem<UVeyraShopSubsystem>();
			VeyraAbilitiesTests::FArchetypeTestWorld World{ Spawner };
			Participant = World.Spawn(EVeyraTeam::A, FVector::ZeroVector).GetPlayerState<AVeyraPlayerState>();
			ASSERT_THAT(IsTrue(Subsystem && Participant));
			UVeyraShopSubsystem::InitializeInventory(*Participant);
			ASSERT_THAT(IsTrue(Participant->FindComponentByClass<UVeyraGoldComponent>()->Grant(Purse, EVeyraGoldReason::Developer)));
		}

		AFTER_EACH()
		{
			UVeyraItemsTuningSubsystem::SetTestOverride(nullptr);
		}

		static const FVeyraShopOffer* OfferFor(const FVeyraShopView& View, const TCHAR* Item)
		{
			return View.Offers.FindByPredicate([Id = ItemId(Item)](const FVeyraShopOffer& Offer) { return Offer.Item == Id; });
		}

		TEST_METHOD(TheModelPricesEachItemFromWhatIsHeld)
		{
			Subsystem->SetAtFountain(*Participant, true);
			ASSERT_THAT(IsTrue(Subsystem->Buy(*Participant, ItemId(TEXT("test_grip"))) == EVeyraShopRefusal::None));

			const FVeyraShopView View = VeyraShopModel::Describe(*Participant);
			const double GripCost = Tuning.Items[ItemId(TEXT("test_grip"))].Cost;
			ASSERT_THAT(IsTrue(View.bAtShop && View.UndoSteps == 1 && View.Gold == Purse - GripCost));
			ASSERT_THAT(AreEqual(Tuning.Shop.InventorySlots, View.Slots.Num()));
			ASSERT_THAT(IsTrue(View.Slots[0].Item == ItemId(TEXT("test_grip")) && View.Slots[0].SaleValue == GripCost * Tuning.Shop.ResaleFraction));
			ASSERT_THAT(IsFalse(View.Slots[1].Item.IsValid()));

			// The harness consumes the grip it holds: it costs its recipe and the plate it buys.
			const FVeyraShopOffer* Harness = OfferFor(View, TEXT("test_harness"));
			const double PlateCost = Tuning.Items[ItemId(TEXT("test_plate"))].Cost;
			ASSERT_THAT(IsTrue(Harness && Harness->Price == Tuning.Items[ItemId(TEXT("test_harness"))].Cost + PlateCost));
			ASSERT_THAT(IsTrue(Harness->Refusal == EVeyraShopRefusal::None));
			const FVeyraShopOffer* Temper = OfferFor(View, TEXT("test_temper"));
			ASSERT_THAT(IsTrue(Temper && Temper->Refusal == EVeyraShopRefusal::NotEnoughGold, TEXT("a Masterwork costs more than is left")));

			// Components first, then assemblies, then Masterworks.
			for (int32 Index = 1; Index < View.Offers.Num(); ++Index)
			{
				ASSERT_THAT(IsTrue(View.Offers[Index - 1].Tier <= View.Offers[Index].Tier));
			}
		}

		TEST_METHOD(AwayFromTheFountainPurchasesWaitAndNothingSells)
		{
			ASSERT_THAT(IsTrue(Subsystem->Buy(*Participant, ItemId(TEXT("test_grip"))) == EVeyraShopRefusal::None));
			const FVeyraShopView View = VeyraShopModel::Describe(*Participant);
			ASSERT_THAT(IsFalse(View.bAtShop));
			ASSERT_THAT(IsTrue(View.Pending.Num() == 1 && View.Pending[0].Item == ItemId(TEXT("test_grip"))));
			ASSERT_THAT(IsFalse(View.Slots[0].Item.IsValid(), TEXT("it waits in the queue")));
			// A second grip is priced from the queue as it will deliver: a wheel now needs one more grip.
			const FVeyraShopOffer* Wheel = OfferFor(View, TEXT("test_wheel"));
			const double GripCost = Tuning.Items[ItemId(TEXT("test_grip"))].Cost;
			ASSERT_THAT(IsTrue(Wheel && Wheel->Price == Tuning.Items[ItemId(TEXT("test_wheel"))].Cost + GripCost));
		}

		TEST_METHOD(StatsReadAsTheShopListsThem)
		{
			FVeyraItemStatsTuning Stats;
			Stats.PhysicalPower = GripPower;
			ASSERT_THAT(AreEqual(FString(TEXT("+10 Physical Power")), VeyraShopModel::DescribeStats(Stats).ToString()));
			constexpr double Fraction = 0.25;
			Stats.AttackSpeed = Fraction;
			ASSERT_THAT(AreEqual(FString(TEXT("+10 Physical Power, +25% Attack Speed")), VeyraShopModel::DescribeStats(Stats).ToString()));
			ASSERT_THAT(IsTrue(VeyraShopModel::DescribeStats(FVeyraItemStatsTuning()).IsEmpty()));
		}

		TEST_METHOD(TheHudShowsTheItemBarAndWhatWaits)
		{
			Subsystem->SetAtFountain(*Participant, true);
			ASSERT_THAT(IsTrue(Subsystem->Buy(*Participant, ItemId(TEXT("test_grip"))) == EVeyraShopRefusal::None));
			Subsystem->SetAtFountain(*Participant, false);
			ASSERT_THAT(IsTrue(Subsystem->Buy(*Participant, ItemId(TEXT("test_tonic"))) == EVeyraShopRefusal::None));

			const FVeyraHudPlayer Player = VeyraHud::DescribePlayer(*Participant, Spawner.GetWorld().GetTimeSeconds());
			ASSERT_THAT(AreEqual(Tuning.Shop.InventorySlots, Player.Items.Num()));
			ASSERT_THAT(IsTrue(Player.Items[0].Slot == EVeyraAbilitySlot::Item1 && Player.Items[0].Item == ItemId(TEXT("test_grip")) && Player.Items[0].Count == 1));
			ASSERT_THAT(IsTrue(Player.Items[5].Slot == EVeyraAbilitySlot::Item6 && !Player.Items[5].Item.IsValid()));
			ASSERT_THAT(AreEqual(1, Player.PendingPurchases, TEXT("the tonic waits for the fountain")));
		}

		TEST_METHOD(TheScreenOffersWhatTheModelAllowsAndShowsRefusals)
		{
			Subsystem->SetAtFountain(*Participant, true);
			ASSERT_THAT(IsTrue(Subsystem->Buy(*Participant, ItemId(TEXT("test_grip"))) == EVeyraShopRefusal::None));
			AVeyraPlayerController& Controller = Spawner.SpawnActor<AVeyraPlayerController>();
			Controller.PlayerState = Participant;
			UVeyraShopScreen* Screen = CreateWidget<UVeyraShopScreen>(&Spawner.GetWorld());
			ASSERT_THAT(IsNotNull(Screen));
			bool bClosed = false;
			Screen->Show(Controller, [&bClosed] { bClosed = true; });

			const FVeyraShopView& View = Screen->GetView();
			const FVeyraShopOffer* Harness = OfferFor(View, TEXT("test_harness"));
			const FVeyraShopOffer* Temper = OfferFor(View, TEXT("test_temper"));
			ASSERT_THAT(IsTrue(Harness && Temper));
			UVeyraShellButton* BuyHarness = Screen->FindButton(UVeyraShopScreen::BuyLabel(Harness->Item, Harness->Price));
			UVeyraShellButton* BuyTemper = Screen->FindButton(UVeyraShopScreen::BuyLabel(Temper->Item, Temper->Price));
			UVeyraShellButton* SellGrip = Screen->FindButton(UVeyraShopScreen::SellLabel(0, View.Slots[0].SaleValue));
			ASSERT_THAT(IsTrue(BuyHarness && BuyTemper && SellGrip));
			ASSERT_THAT(IsTrue(BuyHarness->GetIsEnabled() && !BuyTemper->GetIsEnabled() && SellGrip->GetIsEnabled()));
			ASSERT_THAT(IsTrue(Screen->GetMessage().IsEmpty()));

			// No match runs here, so the request is refused, and the screen says why.
			BuyHarness->Press();
			ASSERT_THAT(IsTrue(Controller.GetShopRefusalCount() == 1));
			ASSERT_THAT(AreEqual(VeyraShopModel::DescribeRefusal(EVeyraShopRefusal::NotNow).ToString(), Screen->GetMessage().ToString()));

			Screen->FindButton(FText::FromString(TEXT("Close")))->Press();
			ASSERT_THAT(IsTrue(bClosed));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER && WITH_VEYRA_UI
