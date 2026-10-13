// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"

#if ENABLE_PIE_NETWORK_TEST

#include "AbilitySystemComponent.h"
#include "Attributes/VeyraMobilitySet.h"
#include "GameFramework/PlayerState.h"
#include "Movement/VeyraMovementComponent.h"
#include "VeyraCombatVerbs.h"
#include "Tests/Net/VeyraMatchNetTestHelpers.h"
#include "Tests/Net/VeyraNetTestHelpers.h"
#include "VeyraPlayerState.h"
#include "VeyraVanguardCharacter.h"
#include "VeyraVanguardController.h"

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

		/** How far, at most, another machine drew the mover's capsule from its latest update. */
		double MostPastUpdate = 0.0;

		/** How fast the server walks the mover now. */
		double ServerWalks = 0.0;

		BEFORE_EACH()
		{
			IgnoreKnownIrisWarnings(*TestRunner);
			MostPastUpdate = 0.0;
			ServerWalks = 0.0;
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

		TEST_METHOD(TheServersPathReachesItsOwnPlayerAlone)
		{
			// The player's machine draws its own body ahead along the path the server walks it; where it goes is for that
			// player alone (ADR-077 §3).
			const FVector Destination = FVector::ZeroVector;
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer([this](FState& State) { MoverId = ServerControllerOf(State, 0)->PlayerState->GetPlayerId(); })
				.ThenClient(0, [Destination](FState& State) { LocalControllerOf(State.World)->IssueMoveOrder(Destination); })
				.UntilClient(0, [Destination](FState& State) {
					const AVeyraPlayerController* Local = LocalControllerOf(State.World);
					const AVeyraPlayerState* Own = Local ? Local->GetPlayerState<AVeyraPlayerState>() : nullptr;
					const FVeyraOwnPath* Path = Own ? &Own->GetOwnPath() : nullptr;
					return Path && Path->Points.Num() >= 2 && Path->Body == Local->GetCommandedBody()
						&& FVector::Dist2D(Path->Ordered, Destination) <= PositionSlack;
				})
				.UntilServer(TEXT("The server moves the Vanguard"), [this, Destination](FState& State) {
					return IsNear2D(FindVanguard(State.World, MoverId), Destination);
				})
				.ThenClient(1, [this](FState& State) {
					const AVeyraVanguardCharacter* Mover = FindVanguard(State.World, MoverId);
					const AVeyraPlayerState* Theirs = Mover ? Mover->GetPlayerState<AVeyraPlayerState>() : nullptr;
					ASSERT_THAT(IsTrue(Theirs && Theirs->GetOwnPath().Points.IsEmpty(), TEXT("another player's machine never learns it")));
				});
		}

		TEST_METHOD(TheServersSpeedForTheBodyReachesItsOwnPlayer)
		{
			// The player's machine draws its body ahead at the speed the server walks it at now, slows and all, not its raw
			// Movement Speed (ADR-077 §2). Fixture values: a Slow that halves its speed, for longer than the test.
			constexpr double SlowShare = 0.5;
			constexpr double SlowSeconds = 600.0;
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Slow the mover"), [this](FState& State) {
					AVeyraPlayerState* Mover = ServerControllerOf(State, 0)->GetPlayerState<AVeyraPlayerState>();
					MoverId = Mover->GetPlayerId();
					FVeyraStatusSpec Slow;
					Slow.Id = FVeyraContentId::FromText(TEXT("test_slow")).GetValue();
					Slow.Kind = EVeyraStatusKind::Slow;
					Slow.Magnitude = SlowShare;
					Slow.DurationSeconds = SlowSeconds;
					UAbilitySystemComponent& Self = *Mover->GetAbilitySystemComponent();
					ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(Self, Self, Slow)));
				})
				.UntilServer(TEXT("The server walks it slower than its Movement Speed"), [this](FState& State) {
					const AVeyraVanguardCharacter* Mover = FindVanguard(State.World, MoverId);
					const UVeyraMovementComponent* Movement = Mover ? Mover->FindComponentByClass<UVeyraMovementComponent>() : nullptr;
					const UAbilitySystemComponent* Combatant = Mover ? Mover->GetAbilitySystemComponent() : nullptr;
					ServerWalks = Movement ? Movement->GetMaxSpeed() : 0.0;
					return Combatant && ServerWalks > 0.0 && ServerWalks < Combatant->GetNumericAttribute(UVeyraMobilitySet::GetMoveSpeedAttribute());
				})
				.UntilClient(0, [this](FState& State) {
					const AVeyraPlayerState* Own = LocalControllerOf(State.World)->GetPlayerState<AVeyraPlayerState>();
					return Own && FMath::IsNearlyEqual(Own->GetOwnBodySpeed(), ServerWalks, PositionSlack);
				});
		}

		TEST_METHOD(AnotherMachineDrawsAWalkingVanguardWhereTheServerLastSentIt)
		{
			// Never past the server's latest update: a machine that only shows the body eases it between updates rather than
			// sliding it on and pulling it back at a stop (ADR-077 §1).
			const FVector Destination = FVector::ZeroVector;
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer([this](FState& State) { MoverId = ServerControllerOf(State, 0)->PlayerState->GetPlayerId(); })
				.ThenClient(0, [Destination](FState& State) { LocalControllerOf(State.World)->IssueMoveOrder(Destination); })
				.UntilClient(1, [this, Destination](FState& State) {
					const AVeyraVanguardCharacter* Mover = FindVanguard(State.World, MoverId);
					if (!Mover)
					{
						return false;
					}
					MostPastUpdate = FMath::Max(MostPastUpdate, FVector::Dist2D(Mover->GetActorLocation(), Mover->GetReplicatedMovement().Location));
					return IsNear2D(Mover, Destination);
				})
				.ThenClient(1, [this](FState&) {
					ASSERT_THAT(IsTrue(MostPastUpdate <= PositionSlack, *FString::Printf(TEXT("drawn %.1f units from its latest update"), MostPastUpdate)));
				});
		}

		TEST_METHOD(SteeringAHeldOrderMovesTheVanguard)
		{
			const FVector Destination = FVector::ZeroVector;
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer([this](FState& State) { MoverId = ServerControllerOf(State, 0)->PlayerState->GetPlayerId(); })
				.ThenClient(0, [Destination](FState& State) { LocalControllerOf(State.World)->SteerMoveOrder(Destination); })
				.UntilServer(TEXT("The server moves the Vanguard"), [this, Destination](FState& State) {
					return IsNear2D(FindVanguard(State.World, MoverId), Destination);
				});
		}

		TEST_METHOD(AMoveOntoAnEnemyEndsAtItsEdge)
		{
			// A Vanguard can't stand where an enemy stands, so it arrives at the enemy rather than steering round it
			// without end (ADR-062 §3). Fixture value: how far apart the two start.
			constexpr double Apart = 600.0;
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Stand the enemy in the way, then walk onto it"), [this](FState& State) {
					AVeyraPlayerState* Mover = ServerControllerOf(State, 0)->GetPlayerState<AVeyraPlayerState>();
					APawn* Enemy = ServerControllerOf(State, 1)->GetPlayerState<AVeyraPlayerState>()->GetPawn();
					ASSERT_THAT(IsTrue(Mover && Mover->GetPawn() && Enemy));
					MoverId = Mover->GetPlayerId();
					const FVector From = Mover->GetPawn()->GetActorLocation();
					Enemy->SetActorLocation(From + FVector(-From.X, 0.0, 0.0).GetSafeNormal() * Apart);
					ASSERT_THAT(IsTrue(Mover->GetVanguardController()->MoveToDestination(Enemy->GetActorLocation()) == EVeyraOrderRejection::None));
				})
				.UntilServer(TEXT("The order ends"), [](FState& State) {
					return !ServerControllerOf(State, 0)->GetPlayerState<AVeyraPlayerState>()->GetVanguardController()->GetMoveOrder().IsSet();
				})
				.ThenServer(TEXT("At the enemy's edge"), [this](FState& State) {
					const APawn* Mover = FindVanguard(State.World, MoverId);
					const APawn* Enemy = ServerControllerOf(State, 1)->GetPlayerState<AVeyraPlayerState>()->GetPawn();
					const double Gap = FVector::Dist2D(Mover->GetActorLocation(), Enemy->GetActorLocation()) - Mover->GetSimpleCollisionRadius()
						- Enemy->GetSimpleCollisionRadius();
					ASSERT_THAT(IsTrue(Gap <= Tuning->Tuning.Orders.ArrivalTolerance + PositionSlack, *FString::Printf(TEXT("%.1f units short"), Gap)));
				});
		}

		TEST_METHOD(OrdersBeforeTheMatchIsLiveAreRefusedSaveMoving)
		{
			// Long enough that the order arrives during preparation, which takes only moves inside the fountain
			// (Veyra.Net.Preparation; ADR-054 §1).
			constexpr double LongPreparationSeconds = 600.0;
			Tuning->Tuning.Phases.PreparationSeconds = LongPreparationSeconds;
			StartMatch(Network, Layout, EVeyraMatchPhase::Preparation)
				.ThenClient(0, [](FState& State) { LocalControllerOf(State.World)->IssueAttackMoveOrder(FVector::ZeroVector); })
				.UntilClient(0, [](FState& State) {
					return LocalControllerOf(State.World)->GetLastOrderRejection() == EVeyraOrderRejection::WrongPhase;
				});
		}

		/** Whether the local player's latest order mark is one the server refused. */
		static bool LatestOrderRefused(FState& State)
		{
			const TOptional<FVeyraOrderMark>& Mark = LocalControllerOf(State.World)->GetOrderMark();
			return Mark.IsSet() && Mark->bRefused;
		}

		TEST_METHOD(UnusableDestinationsAreRefused)
		{
			// The refusal names the order it answers, so the player's machine stops drawing its body that way (ADR-077 §2); a
			// later order is not refused for it.
			const FVector OffTheMap(Layout.Floor.LengthX * 10.0, 0.0, 0.0);
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenClient(0, [](FState& State) { LocalControllerOf(State.World)->IssueMoveOrder(FVector(NAN, 0.0, 0.0)); })
				.UntilClient(0, [](FState& State) {
					return LocalControllerOf(State.World)->GetLastOrderRejection() == EVeyraOrderRejection::InvalidOrder && LatestOrderRefused(State);
				})
				.ThenClient(0, [OffTheMap](FState& State) { LocalControllerOf(State.World)->IssueMoveOrder(OffTheMap); })
				.ThenClient(0, [this](FState& State) { ASSERT_THAT(IsFalse(LatestOrderRefused(State), TEXT("a new order is not refused for the last"))); })
				.UntilClient(0, [](FState& State) {
					return LocalControllerOf(State.World)->GetLastOrderRejection() == EVeyraOrderRejection::Unreachable && LatestOrderRefused(State);
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
				.ThenClient(0, [this](FState& State) {
					ASSERT_THAT(AreEqual(LocalControllerOf(State.World)->GetOrderRejectionCount(), 1));
					// The order over the rate is the latest: it is the one refused.
					ASSERT_THAT(IsTrue(LatestOrderRefused(State)));
				});
		}
	};
}

#endif // ENABLE_PIE_NETWORK_TEST
