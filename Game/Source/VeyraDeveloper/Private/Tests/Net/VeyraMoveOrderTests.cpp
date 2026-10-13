// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"

#if ENABLE_PIE_NETWORK_TEST

#include "AbilitySystemComponent.h"
#include "Attributes/VeyraMobilitySet.h"
#include "Engine/NetDriver.h"
#include "GameFramework/PlayerState.h"
#include "Movement/VeyraMovementComponent.h"
#include "Movement/VeyraMotion.h"
#include "Movement/VeyraServerClock.h"
#include "Tuning/VeyraCombatTuningSubsystem.h"
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
		// Fixture values: half a hosted round trip of lag on each side, with jitter, and what counts as a hitch.
		static constexpr int32 OneWayLagMs = 30;
		static constexpr int32 JitterMs = 20;
		static constexpr double HitchShare = 0.5;
		static constexpr double EdgeSeconds = 0.2;
		static constexpr int32 FewestFrames = 20;

		int32 MoverId = INDEX_NONE;

		/** How far, at most, another machine drew the mover's capsule from its latest update. */
		double MostPastUpdate = 0.0;

		/** How fast the server walks the mover now. */
		double ServerWalks = 0.0;
		uint16 GivenOrder = 0;
		double ToldLength = 0.0;
		double ToldSpeed = 0.0;

		/** A walking body's drawn steps, frame by frame: how many were measured, and how many fell short (a hitch). */
		struct FStride
		{
			TOptional<FVector> Last;
			int32 Frames = 0;
			int32 Hitches = 0;
		};
		FStride Stride;
		/** Where the mover stood before its first walk, which it walks back to. */
		FVector WalkedFrom = FVector::ZeroVector;
		/** The latest motion another machine was told of the mover, by its start, and when, by that machine's world clock. */
		double SeenMotionStart = -1.0;
		double SeenMotionAt = 0.0;

		/**
		 * Measures one frame of Body's drawn walk on World: a frame mid-walk in which the body covers less than half of what
		 * its motion walks is a hitch. The first and last moments of a walk, where it starts and stops, are not measured.
		 */
		void MeasureStride(const UWorld& World, const APawn& Body)
		{
			const UVeyraMovementComponent* Movement = Body.FindComponentByClass<UVeyraMovementComponent>();
			const UVeyraServerClock* Clock = UVeyraServerClock::Get(&World);
			const FVector Here = Body.GetActorLocation();
			const TOptional<FVector> Before = Stride.Last;
			Stride.Last = Here;
			if (!Movement || !Clock || !Before.IsSet())
			{
				return;
			}
			const FVeyraMotion& Drawn = Movement->GetDrawnMotion();
			const double Now = Clock->Now() + Movement->GetDrawnLeadSeconds();
			const bool bMidWalk = Drawn.Kind == EVeyraMotionKind::Walk && Now > Drawn.StartTime + EdgeSeconds && Now < VeyraMotion::EndTime(Drawn) - EdgeSeconds;
			if (!bMidWalk)
			{
				return;
			}
			++Stride.Frames;
			const double Expected = Drawn.Speed * World.GetDeltaSeconds();
			Stride.Hitches += FVector::Dist2D(Here, Before.GetValue()) < Expected * HitchShare ? 1 : 0;
		}

		/** Puts a hosted connection's lag and jitter on World's outgoing packets. */
		static void Roughen(UWorld& World)
		{
#if DO_ENABLE_NET_TEST
			if (UNetDriver* Driver = World.GetNetDriver())
			{
				FPacketSimulationSettings Rough;
				Rough.PktLag = OneWayLagMs;
				Rough.PktLagVariance = JitterMs;
				Driver->SetPacketSimulationSettings(Rough);
			}
#endif
		}

		BEFORE_EACH()
		{
			IgnoreKnownIrisWarnings(*TestRunner);
			MostPastUpdate = 0.0;
			ServerWalks = 0.0;
			GivenOrder = 0;
			SeenMotionStart = -1.0;
			SeenMotionAt = 0.0;
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

		TEST_METHOD(EveryMachineLearnsTheWalkButNotWhereItEnds)
		{
			// Every machine that sees the body is told its walk as a motion, but only as far as it walks in the tuning's horizon,
			// so where a Vanguard is going stays its own (ADR-079 §2).
			const FVector Destination = FVector::ZeroVector;
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer([this](FState& State) { MoverId = ServerControllerOf(State, 0)->PlayerState->GetPlayerId(); })
				.ThenClient(0, [Destination](FState& State) { LocalControllerOf(State.World)->IssueMoveOrder(Destination); })
				.UntilClient(1, [this](FState& State) {
					const AVeyraVanguardCharacter* Mover = FindVanguard(State.World, MoverId);
					const UVeyraMovementComponent* Movement = Mover ? Mover->FindComponentByClass<UVeyraMovementComponent>() : nullptr;
					if (!Movement || Movement->GetMotion().Kind != EVeyraMotionKind::Walk)
					{
						return false;
					}
					ToldLength = VeyraMotion::Length(Movement->GetMotion());
					ToldSpeed = Movement->GetMotion().Speed;
					return true;
				})
				.ThenClient(1, [this](FState&) {
					const double Horizon = UVeyraCombatTuningSubsystem::Get().Motion.HorizonSeconds * ToldSpeed;
					ASSERT_THAT(IsTrue(ToldLength <= Horizon + PositionSlack, *FString::Printf(TEXT("told %.0f units of its walk"), ToldLength)));
				})
				.UntilServer(TEXT("The server moves the Vanguard"), [this, Destination](FState& State) {
					return IsNear2D(FindVanguard(State.World, MoverId), Destination);
				});
		}

		TEST_METHOD(ItsMotionCarriesTheSpeedTheServerWalksItAt)
		{
			// The player's machine walks an order it gives at the speed the server walks the body at now, slows and all, not its
			// raw Movement Speed: every motion carries it (ADR-079 §5). Fixture values: a Slow that halves its speed, for longer
			// than the test.
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
					const APawn* Own = LocalControllerOf(State.World)->GetCommandedBody();
					const UVeyraMovementComponent* Movement = Own ? Own->FindComponentByClass<UVeyraMovementComponent>() : nullptr;
					return Movement && FMath::IsNearlyEqual(Movement->GetMotion().Speed, ServerWalks, PositionSlack);
				});
		}

		TEST_METHOD(AnotherMachineDrawsAWalkingVanguardOnItsMotion)
		{
			// A machine that only shows the body draws it where its motion has it on the server's clock now, once its ease onto a
			// newly told motion has closed, and walks it to the very place the server's body stops (ADR-079 §3).
			const FVector Destination = FVector::ZeroVector;
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer([this](FState& State) { MoverId = ServerControllerOf(State, 0)->PlayerState->GetPlayerId(); })
				.ThenClient(0, [Destination](FState& State) { LocalControllerOf(State.World)->IssueMoveOrder(Destination); })
				.UntilClient(1, [this, Destination](FState& State) {
					const AVeyraVanguardCharacter* Mover = FindVanguard(State.World, MoverId);
					const UVeyraMovementComponent* Movement = Mover ? Mover->FindComponentByClass<UVeyraMovementComponent>() : nullptr;
					const UVeyraServerClock* Clock = UVeyraServerClock::Get(State.World);
					if (!Movement || !Clock || !Movement->HasMotion())
					{
						return false;
					}
					// Once its ease onto the latest motion has closed, the body is drawn where that motion has it.
					const double WorldNow = State.World->GetTimeSeconds();
					if (Movement->GetMotion().StartTime != SeenMotionStart)
					{
						SeenMotionStart = Movement->GetMotion().StartTime;
						SeenMotionAt = WorldNow;
					}
					if (WorldNow - SeenMotionAt > UVeyraCombatTuningSubsystem::Get().Motion.CorrectionSeconds)
					{
						// This check may run before or after the body is drawn this frame: a frame's walk either way.
						const FVector OnMotion = VeyraMotion::Sample(Movement->GetMotion(), Clock->Now()).Location;
						const double FramesWalk = Movement->GetMotion().Speed * State.World->GetDeltaSeconds();
						MostPastUpdate = FMath::Max(MostPastUpdate, FVector::Dist2D(Mover->GetActorLocation(), OnMotion) - FramesWalk);
					}
					return IsNear2D(Mover, Destination) && Movement->GetMotion().Kind == EVeyraMotionKind::Stand;
				})
				.ThenClient(1, [this](FState&) {
					const double Allowed = PositionSlack;
					ASSERT_THAT(IsTrue(MostPastUpdate <= Allowed, *FString::Printf(TEXT("drawn %.1f units from its motion"), MostPastUpdate)));
				});
		}

		TEST_METHOD(ItsOwnMachineWalksAnOrderTheMomentItIsGiven)
		{
			// The player's own machine draws its body carrying out the order at once, and the server's answer to that very order
			// takes over (ADR-079 §5).
			const FVector Destination = FVector::ZeroVector;
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenClient(0, [this, Destination](FState& State) {
					AVeyraPlayerController* Local = LocalControllerOf(State.World);
					Local->IssueMoveOrder(Destination);
					const APawn* Own = Local->GetCommandedBody();
					const UVeyraMovementComponent* Movement = Own ? Own->FindComponentByClass<UVeyraMovementComponent>() : nullptr;
					ASSERT_THAT(IsNotNull(Movement));
					ASSERT_THAT(IsTrue(Local->GetOrderMark().IsSet()));
					GivenOrder = Local->GetOrderMark()->Number;
					ASSERT_THAT(IsTrue(Movement->GetDrawnMotion().Kind == EVeyraMotionKind::Walk && Movement->GetDrawnMotion().OrderNumber == GivenOrder,
						TEXT("walks the order the frame it is given")));
				})
				.UntilClient(0, [this](FState& State) {
					const APawn* Own = LocalControllerOf(State.World)->GetCommandedBody();
					const UVeyraMovementComponent* Movement = Own ? Own->FindComponentByClass<UVeyraMovementComponent>() : nullptr;
					return Movement && Movement->GetMotion().OrderNumber == GivenOrder && &Movement->GetDrawnMotion() == &Movement->GetMotion();
				});
		}

		TEST_METHOD(BodiesWalkSmoothlyThroughLagAndJitter)
		{
			// With a hosted connection's lag and jitter, a walking body never hitches: not on another player's machine, which
			// draws it from its motion, nor on its own player's, which draws its order at once and then the server's answer
			// (ADR-079 §3, §5).
			const FVector Destination = FVector::ZeroVector;
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer([this](FState& State) {
					MoverId = ServerControllerOf(State, 0)->PlayerState->GetPlayerId();
					Roughen(*State.World);
				})
				.ThenClient(0, [](FState& State) { Roughen(*State.World); })
				.ThenClient(1, [](FState& State) { Roughen(*State.World); })
				.ThenClient(0, [this](FState& State) {
					Stride = FStride();
					AVeyraPlayerController* Local = LocalControllerOf(State.World);
					WalkedFrom = Local->GetCommandedBody()->GetActorLocation();
					Local->IssueMoveOrder(FVector::ZeroVector);
				})
				.UntilClient(1, [this, Destination](FState& State) {
					const AVeyraVanguardCharacter* Mover = FindVanguard(State.World, MoverId);
					if (!Mover)
					{
						return false;
					}
					MeasureStride(*State.World, *Mover);
					return IsNear2D(Mover, Destination) && Mover->FindComponentByClass<UVeyraMovementComponent>()->GetMotion().Kind == EVeyraMotionKind::Stand;
				})
				.ThenClient(1, [this](FState&) {
					ASSERT_THAT(IsTrue(Stride.Frames >= FewestFrames, *FString::Printf(TEXT("measured %d frames"), Stride.Frames)));
					ASSERT_THAT(AreEqual(Stride.Hitches, 0, TEXT("another machine drew the walk hitching")));
				})
				.ThenClient(0, [this](FState& State) {
					Stride = FStride();
					AVeyraPlayerController* Local = LocalControllerOf(State.World);
					Local->IssueMoveOrder(WalkedFrom);
				})
				.UntilClient(0, [this](FState& State) {
					const APawn* Own = LocalControllerOf(State.World)->GetCommandedBody();
					const UVeyraMovementComponent* Movement = Own ? Own->FindComponentByClass<UVeyraMovementComponent>() : nullptr;
					if (!Movement)
					{
						return false;
					}
					MeasureStride(*State.World, *Own);
					return Stride.Frames > 0 && Movement->GetMotion().Kind == EVeyraMotionKind::Stand && &Movement->GetDrawnMotion() == &Movement->GetMotion();
				})
				.ThenClient(0, [this](FState&) {
					ASSERT_THAT(IsTrue(Stride.Frames >= FewestFrames, *FString::Printf(TEXT("measured %d frames"), Stride.Frames)));
					ASSERT_THAT(AreEqual(Stride.Hitches, 0, TEXT("its own machine drew the walk hitching")));
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
