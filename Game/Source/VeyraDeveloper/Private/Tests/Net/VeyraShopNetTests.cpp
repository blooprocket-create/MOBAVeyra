// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"

#if ENABLE_PIE_NETWORK_TEST

#include "AbilitySystemComponent.h"
#include "Attributes/VeyraOffenceSet.h"
#include "Gold/VeyraGoldComponent.h"
#include "Inventory/VeyraInventoryComponent.h"
#include "Tests/Net/VeyraMatchNetTestHelpers.h"
#include "Tests/Net/VeyraNetTestHelpers.h"
#include "Tuning/VeyraItemsTuningSubsystem.h"
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

		static AVeyraPlayerState& ServerParticipant(FState& State)
		{
			return *ServerControllerOf(State, 0)->GetPlayerState<AVeyraPlayerState>();
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
	};
}

#endif // ENABLE_PIE_NETWORK_TEST
