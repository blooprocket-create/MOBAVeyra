// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER && WITH_VEYRA_UI

#include "Blueprint/UserWidget.h"
#include "Components/ActorTestSpawner.h"
#include "Gold/VeyraGoldComponent.h"
#include "Hud/VeyraHudModel.h"
#include "Inventory/VeyraInventoryComponent.h"
#include "Loadout/VeyraAbilityLoadoutComponent.h"
#include "Rewards/VeyraEconomyTuningSubsystem.h"
#include "Shell/VeyraShellButton.h"
#include "Shop/VeyraShopModel.h"
#include "Shop/VeyraShopScreen.h"
#include "Shop/VeyraShopSubsystem.h"
#include "State/VeyraVisionTeamState.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "Tests/Items/VeyraItemsTestCatalog.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "Tuning/VeyraFluxTuningSubsystem.h"
#include "Tuning/VeyraItemsTuningSubsystem.h"
#include "Tuning/VeyraVisionTuningSubsystem.h"
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

		TEST_METHOD(TheModelOffersEachSpellSlotItsSwaps)
		{
			const TArray<FVeyraContentId>& Roster = UVeyraAbilitiesTuningSubsystem::Get().FluxSpells.Roster;
			ASSERT_THAT(IsTrue(Roster.Num() >= 2));
			UVeyraAbilityLoadoutComponent& Loadout = *Participant->FindComponentByClass<UVeyraAbilityLoadoutComponent>();
			ASSERT_THAT(IsTrue(Loadout.Grant(*Participant->GetAbilitySystemComponent(), EVeyraAbilitySlot::Spell1, Roster[0])));

			FVeyraShopView View = VeyraShopModel::Describe(*Participant);
			ASSERT_THAT(IsTrue(View.SpellSlots.Num() == 2 && View.SpellSwapCost == UVeyraEconomyTuningSubsystem::Get().FluxSpells.SwapCost));
			ASSERT_THAT(IsTrue(View.SpellSlots[0].Spell == Roster[0] && View.SpellSlots[0].bLocked && !View.SpellSlots[1].Spell.IsValid()));
			ASSERT_THAT(IsTrue(View.SpellSlots[1].Offers[1].Refusal == EVeyraShopRefusal::NotAtFountain, TEXT("never from afar")));
			ASSERT_THAT(IsTrue(View.SpellSlots[1].Offers[0].Refusal == EVeyraShopRefusal::AlreadyEquipped, TEXT("the first slot holds it")));

			Subsystem->SetAtFountain(*Participant, true);
			View = VeyraShopModel::Describe(*Participant);
			ASSERT_THAT(IsTrue(View.SpellSlots[1].Offers[1].Refusal == EVeyraShopRefusal::None));
			// The screen offers each swap the model allows, and none to the spell a slot holds.
			AVeyraPlayerController& Controller = Spawner.SpawnActor<AVeyraPlayerController>();
			Controller.PlayerState = Participant;
			UVeyraShopScreen* Screen = CreateWidget<UVeyraShopScreen>(&Spawner.GetWorld());
			ASSERT_THAT(IsNotNull(Screen));
			Screen->Show(Controller, [] {});
			ASSERT_THAT(IsNull(Screen->FindButton(UVeyraShopScreen::SwapLabel(1, Roster[1])), TEXT("the swaps have their own tab")));
			Screen->FindButton(UVeyraShopScreen::SpellsTabLabel())->Press();
			const UVeyraShellButton* Swap = Screen->FindButton(UVeyraShopScreen::SwapLabel(1, Roster[1]));
			ASSERT_THAT(IsTrue(Swap && Swap->GetIsEnabled()));
			ASSERT_THAT(IsNull(Screen->FindButton(UVeyraShopScreen::SwapLabel(0, Roster[0]))));
		}

		TEST_METHOD(TheModelOffersEachOtherVisionToolAndTheScreenItsSwaps)
		{
			// Any other tool, at the fountain, for the same cost each time (Vision Bible §3; ADR-016 §6).
			FVeyraShopView View = VeyraShopModel::Describe(*Participant);
			ASSERT_THAT(IsTrue(View.bHasVisionTool && View.VisionTool == EVeyraVisionTool::PersistentWard));
			ASSERT_THAT(IsTrue(View.VisionToolSwapCost == UVeyraEconomyTuningSubsystem::Get().VisionTools.SwapCost));
			ASSERT_THAT(AreEqual(View.VisionToolOffers.Num(), 3));
			ASSERT_THAT(IsTrue(View.VisionToolOffers[0].Refusal == EVeyraShopRefusal::AlreadyEquipped));
			ASSERT_THAT(IsTrue(View.VisionToolOffers[1].Refusal == EVeyraShopRefusal::NotAtFountain, TEXT("never from afar")));

			Subsystem->SetAtFountain(*Participant, true);
			View = VeyraShopModel::Describe(*Participant);
			ASSERT_THAT(IsTrue(View.VisionToolOffers[1].Refusal == EVeyraShopRefusal::None && View.VisionToolOffers[2].Refusal == EVeyraShopRefusal::None));
			AVeyraPlayerController& Controller = Spawner.SpawnActor<AVeyraPlayerController>();
			Controller.PlayerState = Participant;
			UVeyraShopScreen* Screen = CreateWidget<UVeyraShopScreen>(&Spawner.GetWorld());
			ASSERT_THAT(IsNotNull(Screen));
			Screen->Show(Controller, [] {});
			const UVeyraShellButton* Sweeper = Screen->FindButton(UVeyraShopScreen::VisionToolName(EVeyraVisionTool::Sweeper));
			ASSERT_THAT(IsTrue(Sweeper && Sweeper->GetIsEnabled()));
			ASSERT_THAT(IsNull(Screen->FindButton(UVeyraShopScreen::VisionToolName(EVeyraVisionTool::PersistentWard)), TEXT("not the tool in the slot")));
		}

		TEST_METHOD(TheHudShowsItsSidesPingsFadingAndItsOutlines)
		{
			const double Cadence = UVeyraVisionTuningSubsystem::Get().Presence.PingEverySeconds;
			// Fixture values: a fog circle, a moment, and where an outlined enemy stands.
			const FVector2D Bush(1000.0, 500.0);
			constexpr double BushRadius = 300.0;
			constexpr double Now = 100.0;
			const FVector Outlined(1100.0, 500.0, 0.0);
			AVeyraVisionTeamState& State = Spawner.SpawnActor<AVeyraVisionTeamState>();
			State.SetVeyraTeam(EVeyraTeam::A);
			State.AddPing(FVeyraPresencePing{ Bush, BushRadius, Now }, Now, Cadence);
			State.SetOutlines({ FVeyraOutline{ Outlined, Now + Cadence } });

			FVeyraHudVision Vision = VeyraHud::DescribeVision(&Spawner.GetWorld(), EVeyraTeam::A, Now);
			ASSERT_THAT(IsTrue(Vision.Pings.Num() == 1 && Vision.Pings[0].Centre.Equals(Bush) && Vision.Pings[0].Radius == BushRadius && Vision.Pings[0].Fade == 1.0));
			ASSERT_THAT(IsTrue(Vision.Outlines.Num() == 1 && Vision.Outlines[0].Equals(Outlined)));
			Vision = VeyraHud::DescribeVision(&Spawner.GetWorld(), EVeyraTeam::A, Now + Cadence / 2.0);
			ASSERT_THAT(IsTrue(Vision.Pings.Num() == 1 && FMath::IsNearlyEqual(Vision.Pings[0].Fade, 0.5), TEXT("it fades until the next")));
			Vision = VeyraHud::DescribeVision(&Spawner.GetWorld(), EVeyraTeam::A, Now + Cadence);
			ASSERT_THAT(IsTrue(Vision.Pings.IsEmpty() && Vision.Outlines.IsEmpty(), TEXT("gone by then")));
			ASSERT_THAT(IsTrue(VeyraHud::DescribeVision(&Spawner.GetWorld(), EVeyraTeam::B, Now).Pings.IsEmpty(), TEXT("another side's state is not the viewer's")));
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
			FVeyraItemStatsTuning Crit;
			Crit.CritChance = 0.15;
			Crit.MagicPowerFraction = 0.08;
			ASSERT_THAT(AreEqual(FString(TEXT("+15% Crit Chance, +8% Magic Power")), VeyraShopModel::DescribeStats(Crit).ToString()));
		}

		TEST_METHOD(TheHudShowsEachSpellSlotLockedUntilItsFluxThenReady)
		{
			const TArray<FVeyraContentId>& Roster = UVeyraAbilitiesTuningSubsystem::Get().FluxSpells.Roster;
			const TArray<double>& Thresholds = UVeyraFluxTuningSubsystem::Get().SpellSlots.Thresholds;
			UVeyraAbilityLoadoutComponent& Loadout = *Participant->FindComponentByClass<UVeyraAbilityLoadoutComponent>();
			ASSERT_THAT(IsTrue(Loadout.Grant(*Participant->GetAbilitySystemComponent(), EVeyraAbilitySlot::Spell1, Roster[0])));
			FVeyraHudPlayer Player = VeyraHud::DescribePlayer(*Participant, Spawner.GetWorld().GetTimeSeconds());
			ASSERT_THAT(AreEqual(2, Player.Spells.Num()));
			ASSERT_THAT(IsTrue(Player.Spells[0].Slot == EVeyraAbilitySlot::Spell1 && Player.Spells[0].Spell == Roster[0] && Player.Spells[0].bLocked));
			ASSERT_THAT(IsTrue(Player.Spells[0].UnlockFlux == Thresholds[0] && Player.Spells[1].UnlockFlux == Thresholds[1]));
			ASSERT_THAT(IsTrue(!Player.Spells[1].Spell.IsValid() && Player.Spells[1].bLocked));

			Loadout.SetUnlockedSpellSlots(1);
			Player = VeyraHud::DescribePlayer(*Participant, Spawner.GetWorld().GetTimeSeconds());
			ASSERT_THAT(IsTrue(!Player.Spells[0].bLocked && Player.Spells[0].CooldownSeconds == 0.0, TEXT("unlocked, and ready")));
			ASSERT_THAT(IsTrue(Player.Spells[1].bLocked));
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
			// As in League, a tile chooses its item, and the one purchase button buys what is chosen.
			ASSERT_THAT(IsNull(Screen->FindButton(UVeyraShopScreen::BuyLabel(Harness->Item, Harness->Price)), TEXT("nothing is chosen yet")));
			Screen->FindButton(UVeyraShopScreen::TileLabel(Temper->Item))->Press();
			ASSERT_THAT(IsTrue(Screen->GetSelectedItem() == Temper->Item));
			const UVeyraShellButton* BuyTemper = Screen->FindButton(UVeyraShopScreen::BuyLabel(Temper->Item, Temper->Price));
			ASSERT_THAT(IsTrue(BuyTemper && !BuyTemper->GetIsEnabled(), TEXT("a Masterwork costs more than is held")));
			// The chosen item shows what it is made from: the harness is built on the grip.
			Screen->FindButton(UVeyraShopScreen::TileLabel(ItemId(TEXT("test_grip"))))->Press();
			ASSERT_THAT(IsNotNull(Screen->FindButton(UVeyraShopScreen::TileLabel(Harness->Item)), TEXT("what the grip builds into")));
			Screen->FindButton(UVeyraShopScreen::TileLabel(Harness->Item))->Press();
			UVeyraShellButton* BuyHarness = Screen->FindButton(UVeyraShopScreen::BuyLabel(Harness->Item, Harness->Price));
			ASSERT_THAT(IsTrue(BuyHarness && BuyHarness->GetIsEnabled()));
			// A slot chosen in the inventory offers its sale at the foot.
			ASSERT_THAT(IsNull(Screen->FindButton(UVeyraShopScreen::SellLabel(0, View.Slots[0].SaleValue))));
			Screen->FindButton(UVeyraShopScreen::SlotLabel(0))->Press();
			const UVeyraShellButton* SellGrip = Screen->FindButton(UVeyraShopScreen::SellLabel(0, View.Slots[0].SaleValue));
			ASSERT_THAT(IsTrue(SellGrip && SellGrip->GetIsEnabled() && Screen->GetSelectedItem() == ItemId(TEXT("test_grip"))));
			ASSERT_THAT(IsTrue(Screen->GetMessage().IsEmpty()));
			Screen->FindButton(UVeyraShopScreen::TileLabel(Harness->Item))->Press();
			BuyHarness = Screen->FindButton(UVeyraShopScreen::BuyLabel(Harness->Item, Harness->Price));
			ASSERT_THAT(IsNotNull(BuyHarness));

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
