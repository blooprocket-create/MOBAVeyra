// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"

#if ENABLE_PIE_NETWORK_TEST

#include "AbilitySystemComponent.h"
#include "Attributes/VeyraOffenceSet.h"
#include "Gold/VeyraGoldComponent.h"
#include "Inventory/VeyraInventoryComponent.h"
#include "Life/VeyraLifeComponent.h"
#include "Shop/VeyraShopSubsystem.h"
#include "Tests/Net/VeyraMatchNetTestHelpers.h"
#include "Tests/Net/VeyraNetTestHelpers.h"
#include "Slots/VeyraAbilitySlot.h"
#include "Tuning/VeyraItemsTuningSubsystem.h"
#include "VeyraCombatVerbs.h"
#include "VeyraPlayerController.h"
#include "VeyraPlayerState.h"

namespace VeyraNetTests
{
	// Veyra.Net.ShopRequests.*: a player buys and sells through its controller; the server applies it,
	// and every machine sees the item (Economy & Progression Bible §10, §12; ADR-012 §7).
	NETWORK_TEST_CLASS(ShopRequests, "Veyra.Net")
	{
		struct FState : public FBasePIENetworkComponentState
		{
			double PowerBefore = 0.0;
			double GoldBeforeSale = 0.0;
		};

		FPIENetworkComponent<FState> Network{ TestRunner, TestCommandBuilder, bInitializing };
		TUniquePtr<FScopedExpectedPlayers> ExpectedPlayers;
		TUniquePtr<FScopedMatchTuning> Tuning;
		FVeyraGreyboxLayout Layout;

		// Fixture values.
		static constexpr double ShortPreparationSeconds = 0.1;
		static constexpr double RespawnSeconds = 0.5;
		static constexpr double OneOrderPerSecond = 1.0;

		BEFORE_EACH()
		{
			IgnoreKnownIrisWarnings(*TestRunner);
			ASSERT_THAT(IsTrue(VeyraGreybox::LoadLayout(Layout).IsEmpty()));
			Tuning = MakeUnique<FScopedMatchTuning>();
			Tuning->Tuning.Phases.PreparationSeconds = ShortPreparationSeconds;
			ExpectedPlayers = MakeUnique<FScopedExpectedPlayers>(MatchClientCount);
			BuildMatchNetwork(Network);
		}

		AFTER_EACH()
		{
			Tuning.Reset();
			ExpectedPlayers.Reset();
		}

		/** The committed catalog's cheapest Physical Power component. */
		static FVeyraContentId Grip()
		{
			return FVeyraContentId::FromText(TEXT("iron_grip")).GetValue();
		}

		/** The committed catalog's consumable. */
		static FVeyraContentId Tonic()
		{
			return FVeyraContentId::FromText(TEXT("field_tonic")).GetValue();
		}

		static AVeyraPlayerState& ServerParticipant(FState& State, int32 ClientIndex = 0)
		{
			return *ServerControllerOf(State, ClientIndex)->GetPlayerState<AVeyraPlayerState>();
		}

		static int32 CountOf(const AVeyraPlayerState& Participant, const FVeyraContentId& Item)
		{
			int32 Count = 0;
			for (const FVeyraInventorySlot& Slot : Participant.FindComponentByClass<UVeyraInventoryComponent>()->GetSlots())
			{
				Count += !Slot.IsEmpty() && Slot.Item == Item ? Slot.Count : 0;
			}
			return Count;
		}

		static bool IsAtFountain(FState& State)
		{
			return ServerParticipant(State).FindComponentByClass<UVeyraInventoryComponent>()->IsAtFountain();
		}

		static bool HoldsGrip(const AVeyraPlayerState& Participant)
		{
			return Participant.FindComponentByClass<UVeyraInventoryComponent>()->GetSlots().ContainsByPredicate(
				[](const FVeyraInventorySlot& Slot) { return !Slot.IsEmpty() && Slot.Item == Grip(); });
		}

		static double PowerOf(const AVeyraPlayerState& Participant)
		{
			return Participant.GetAbilitySystemComponent()->GetNumericAttribute(UVeyraOffenceSet::GetPhysicalPowerAttribute());
		}

		TEST_METHOD(AGripBoughtAtTheFountainArrivesAndSellsBack)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Note the power before"), [](FState& State) { State.PowerBefore = PowerOf(ServerParticipant(State)); })
				.ThenClient(TEXT("Buy a grip at the fountain"), 0, [](FState& State) { LocalControllerOf(State.World)->RequestBuyItem(Grip()); })
				.UntilServer(TEXT("It arrives, with its Physical Power"), [](FState& State) {
					const AVeyraPlayerState& Participant = ServerParticipant(State);
					const double Power = UVeyraItemsTuningSubsystem::FindItem(Grip())->Stats.PhysicalPower;
					return HoldsGrip(Participant) && FMath::IsNearlyEqual(PowerOf(Participant), State.PowerBefore + Power);
				})
				.UntilClient(TEXT("The buyer sees it in its slots, at its fountain, and that undo can take it back"), 0, [](FState& State) {
					const AVeyraPlayerState& Own = *LocalControllerOf(State.World)->GetPlayerState<AVeyraPlayerState>();
					const UVeyraInventoryComponent& Inventory = *Own.FindComponentByClass<UVeyraInventoryComponent>();
					return HoldsGrip(Own) && Inventory.IsAtFountain() && Inventory.GetUndoStepCount() == 1;
				})
				.ThenServer(TEXT("Note the Gold before the sale"), [](FState& State) {
					State.GoldBeforeSale = ServerParticipant(State).FindComponentByClass<UVeyraGoldComponent>()->GetGold();
				})
				.ThenClient(TEXT("Sell it back"), 0, [](FState& State) { LocalControllerOf(State.World)->RequestSellItem(0); })
				.UntilServer(TEXT("It is gone, and some Gold is back"), [](FState& State) {
					const AVeyraPlayerState& Participant = ServerParticipant(State);
					return !HoldsGrip(Participant) && Participant.FindComponentByClass<UVeyraGoldComponent>()->GetGold() > State.GoldBeforeSale;
				});
		}

		TEST_METHOD(AGripBoughtWhileDeadWorksOnlyAfterRespawn)
		{
			// One timer at every level, and no lengthening with the match clock.
			Tuning->Tuning.Respawn.SecondsByLevel = { RespawnSeconds };
			Tuning->Tuning.Respawn.Elapsed = FVeyraRespawnElapsedTuning();
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Kill the buyer, and buy a grip while it is dead"), [this](FState& State) {
					AVeyraPlayerState& Buyer = ServerParticipant(State);
					FVeyraRawDamageEvent Lethal;
					Lethal.Components.Add({ EVeyraDamageType::TrueDamage, TestVanguard().BaseStats.MaxHealth });
					ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*ServerParticipant(State, 1).GetAbilitySystemComponent(), *Buyer.GetAbilitySystemComponent(), Lethal)));
					ASSERT_THAT(IsFalse(Buyer.FindComponentByClass<UVeyraLifeComponent>()->IsAlive()));
					State.PowerBefore = PowerOf(Buyer);
					ASSERT_THAT(IsTrue(State.World->GetSubsystem<UVeyraShopSubsystem>()->Buy(Buyer, Grip()) == EVeyraShopRefusal::None));
					ASSERT_THAT(IsTrue(HoldsGrip(Buyer), TEXT("assigned at once")));
					ASSERT_THAT(IsTrue(PowerOf(Buyer) == State.PowerBefore, TEXT("and giving nothing until respawn (ADR-012 §9)")));
				})
				.UntilServer(TEXT("It respawns with the grip's Physical Power"), [](FState& State) {
					const AVeyraPlayerState& Buyer = ServerParticipant(State);
					const double Power = UVeyraItemsTuningSubsystem::FindItem(Grip())->Stats.PhysicalPower;
					return Buyer.FindComponentByClass<UVeyraLifeComponent>()->IsAlive() && FMath::IsNearlyEqual(PowerOf(Buyer), State.PowerBefore + Power);
				});
		}

		TEST_METHOD(UsingATonicTakesOneOrder)
		{
			// An allowance of one order: a use charged twice would be refused.
			Tuning->Tuning.Orders.MaxPerSecond = OneOrderPerSecond;
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.UntilServer(TEXT("The player stands at its fountain"), [](FState& State) { return IsAtFountain(State); })
				.ThenServer(TEXT("Give it a tonic"), [this](FState& State) {
					ASSERT_THAT(IsTrue(State.World->GetSubsystem<UVeyraShopSubsystem>()->Buy(ServerParticipant(State), Tonic()) == EVeyraShopRefusal::None));
					ASSERT_THAT(IsTrue(CountOf(ServerParticipant(State), Tonic()) == 1));
				})
				.ThenClient(TEXT("Drink it with its item key"), 0, [](FState& State) {
					LocalControllerOf(State.World)->IssueCastOrder(VeyraAbilitySlots::Items[0], nullptr);
				})
				.UntilServer(TEXT("It is drunk"), [](FState& State) { return CountOf(ServerParticipant(State), Tonic()) == 0; });
		}
	};
}

#endif // ENABLE_PIE_NETWORK_TEST
