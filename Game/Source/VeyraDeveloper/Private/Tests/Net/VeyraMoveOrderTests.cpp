// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"

#if ENABLE_PIE_NETWORK_TEST

#include "GameFramework/PlayerState.h"
#include "Tests/Net/VeyraMatchNetTestHelpers.h"
#include "Tests/Net/VeyraNetTestHelpers.h"

namespace VeyraNetTests
{
	// Veyra.Net.VanguardMoveOrders.*: a player's move order reaches the server, which validates it
	// and moves the Vanguard; every client sees the result (ADR-006 §7, no client prediction).
	NETWORK_TEST_CLASS(VanguardMoveOrders, "Veyra.Net")
	{
		struct FState : public FBasePIENetworkComponentState
		{
		};

		FPIENetworkComponent<FState> Network{ TestRunner, TestCommandBuilder, bInitializing };
		TUniquePtr<FScopedExpectedPlayers> ExpectedPlayers;
		TUniquePtr<FScopedMatchTuning> Tuning;
		FVeyraGreyboxLayout Layout;

		// Fixture values: a short preparation, and a comparison allowance for float positions.
		static constexpr double ShortPreparationSeconds = 0.1;
		static constexpr double PositionSlack = 1.0;

		int32 MoverId = INDEX_NONE;

		BEFORE_EACH()
		{
			IgnoreLoginViewTargetRpc(*TestRunner);
			ASSERT_THAT(IsTrue(VeyraGreybox::LoadLayout(Layout).IsEmpty()));
			Tuning = MakeUnique<FScopedMatchTuning>();
			Tuning->Tuning.Phases.PreparationSeconds = ShortPreparationSeconds;
			// Loading waits for both clients rather than the loading timeout.
			ExpectedPlayers = MakeUnique<FScopedExpectedPlayers>(MatchClientCount);
			BuildMatchNetwork(Network);
		}

		AFTER_EACH()
		{
			Tuning.Reset();
			ExpectedPlayers.Reset();
		}

		bool IsNear2D(const AActor* Actor, const FVector& Point) const
		{
			return Actor && FVector::Dist2D(Actor->GetActorLocation(), Point) <= Tuning->Tuning.Orders.ArrivalTolerance + PositionSlack;
		}

		TEST_METHOD(ValidOrderMovesTheVanguardForEveryone)
		{
			const FVector Destination = FVector::ZeroVector;
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer([this](FState& State) {
					const AVeyraPlayerController* Mover = ServerControllerOf(State, 0);
					ASSERT_THAT(IsNotNull(Mover));
					MoverId = Mover->PlayerState->GetPlayerId();
				})
				.ThenClient(0, [Destination](FState& State) { LocalControllerOf(State.World)->IssueMoveOrder(Destination); })
				.UntilServer(TEXT("The server moves the Vanguard"), [this, Destination](FState& State) {
					return IsNear2D(FindVanguard(State.World, MoverId), Destination);
				})
				.UntilClients(TEXT("Every client sees it arrive"), [this, Destination](FState& State) {
					return IsNear2D(FindVanguard(State.World, MoverId), Destination);
				})
				.ThenClient(0, [this](FState& State) { ASSERT_THAT(AreEqual(LocalControllerOf(State.World)->GetOrderRejectionCount(), 0)); });
		}

		TEST_METHOD(OrdersBeforeTheMatchIsLiveAreRefused)
		{
			// Long enough that the order arrives during preparation.
			constexpr double LongPreparationSeconds = 600.0;
			Tuning->Tuning.Phases.PreparationSeconds = LongPreparationSeconds;
			StartMatch(Network, Layout, EVeyraMatchPhase::Preparation)
				.ThenClient(0, [](FState& State) { LocalControllerOf(State.World)->IssueMoveOrder(FVector::ZeroVector); })
				.UntilClient(0, [](FState& State) {
					return LocalControllerOf(State.World)->GetLastOrderRejection() == EVeyraOrderRejection::WrongPhase;
				});
		}

		TEST_METHOD(UnusableDestinationsAreRefused)
		{
			const FVector OffTheMap(Layout.Floor.LengthX * 10.0, 0.0, 0.0);
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenClient(0, [](FState& State) { LocalControllerOf(State.World)->IssueMoveOrder(FVector(NAN, 0.0, 0.0)); })
				.UntilClient(0, [](FState& State) {
					return LocalControllerOf(State.World)->GetLastOrderRejection() == EVeyraOrderRejection::InvalidOrder;
				})
				.ThenClient(0, [OffTheMap](FState& State) { LocalControllerOf(State.World)->IssueMoveOrder(OffTheMap); })
				.UntilClient(0, [](FState& State) {
					return LocalControllerOf(State.World)->GetLastOrderRejection() == EVeyraOrderRejection::Unreachable;
				});
		}

		TEST_METHOD(OrdersBeyondTheRateAreRefused)
		{
			constexpr double OrdersPerSecond = 2.0;
			constexpr int32 OrdersSent = 3;
			Tuning->Tuning.Orders.MaxPerSecond = OrdersPerSecond;
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenClient(0, [](FState& State) {
					for (int32 Order = 0; Order < OrdersSent; ++Order)
					{
						LocalControllerOf(State.World)->IssueMoveOrder(FVector::ZeroVector);
					}
				})
				.UntilClient(0, [](FState& State) {
					return LocalControllerOf(State.World)->GetLastOrderRejection() == EVeyraOrderRejection::TooFrequent;
				})
				.ThenClient(0, [this](FState& State) { ASSERT_THAT(AreEqual(LocalControllerOf(State.World)->GetOrderRejectionCount(), 1)); });
		}
	};
}

#endif // ENABLE_PIE_NETWORK_TEST
