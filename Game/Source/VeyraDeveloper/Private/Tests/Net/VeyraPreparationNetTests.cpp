// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"

#if ENABLE_PIE_NETWORK_TEST

#include "EngineUtils.h"
#include "Slots/VeyraAbilitySlot.h"
#include "Tests/Net/VeyraMatchNetTestHelpers.h"
#include "Tests/Net/VeyraNetTestHelpers.h"
#include "VeyraAbilityTypes.h"
#include "VeyraGameMode.h"
#include "VeyraPlayerController.h"
#include "VeyraPlayerState.h"
#include "VeyraTeamStart.h"
#include "VeyraVanguardCharacter.h"

namespace VeyraNetTests
{
	// Veyra.Net.Preparation.*: in fountain preparation a player moves its Vanguard inside its own fountain, never out
	// of it, and gives no other order (Match Flow Bible §1, §3; ADR-054 §1).
	NETWORK_TEST_CLASS(Preparation, "Veyra.Net")
	{
		struct FState : public FBasePIENetworkComponentState
		{
			FVector Home = FVector::ZeroVector;
		};

		FPIENetworkComponent<FState> Network{ TestRunner, TestCommandBuilder, bInitializing };
		TUniquePtr<FScopedExpectedPlayers> ExpectedPlayers;
		TUniquePtr<FScopedMatchTuning> Tuning;
		FVeyraGreyboxLayout Layout;
		double HoldStartRealTime = 0.0;

		// Fixture values: preparation outlasts the test, and the fountain is small enough to walk across.
		static constexpr double LongPreparationSeconds = 600.0;
		static constexpr double FountainRadius = 500.0;
		static constexpr double Tolerance = 120.0;
		static constexpr double HoldRealSeconds = 1.0;

		BEFORE_EACH()
		{
			IgnoreKnownIrisWarnings(*TestRunner);
			ASSERT_THAT(IsTrue(VeyraGreybox::LoadLayout(Layout).IsEmpty()));
			Tuning = MakeUnique<FScopedMatchTuning>();
			Tuning->Tuning.Phases.PreparationSeconds = LongPreparationSeconds;
			Tuning->Tuning.Fountain.Radius = FountainRadius;
			ExpectedPlayers = MakeUnique<FScopedExpectedPlayers>(MatchClientCount);
			BuildMatchNetwork(Network);
		}

		AFTER_EACH()
		{
			Tuning.Reset();
			ExpectedPlayers.Reset();
		}

		static AVeyraPlayerState& ServerParticipant(FState& State)
		{
			return *ServerControllerOf(State, 0)->GetPlayerState<AVeyraPlayerState>();
		}

		static FVector HomeOf(UWorld* World, EVeyraTeam Team)
		{
			for (TActorIterator<AVeyraTeamStart> It(World); It; ++It)
			{
				if (It->GetVeyraTeam() == Team)
				{
					return It->GetActorLocation();
				}
			}
			return FVector::ZeroVector;
		}

		static double FromHome(FState& State)
		{
			return FVector::Dist2D(ServerParticipant(State).GetPawn()->GetActorLocation(), State.Home);
		}

		TEST_METHOD(AVanguardMovesInsideItsFountainAndStopsAtItsEdge)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Preparation)
				.ThenServer(TEXT("Find its fountain"), [this](FState& State) {
					ASSERT_THAT(IsTrue(GameStateOf(State.World)->GetPhase() == EVeyraMatchPhase::Preparation));
					State.Home = HomeOf(State.World, ServerParticipant(State).GetVeyraTeam());
				})
				.ThenClient(TEXT("Order a walk inside the fountain"), 0, [](FState& State) {
					AVeyraPlayerController* Player = LocalControllerOf(State.World);
					Player->IssueMoveOrder(Player->GetVanguard()->GetActorLocation() + FVector(0.0, FountainRadius / 2.0, 0.0));
				})
				.UntilServer(TEXT("It walks"), [](FState& State) {
					const APawn* Vanguard = ServerParticipant(State).GetPawn();
					return Vanguard && Vanguard->GetVelocity().Size2D() > 0.0;
				})
				// Each machine has its own state, so the client finds the way itself: toward the battleground's centre.
				.ThenClient(TEXT("Order it out toward the battleground"), 0, [](FState& State) {
					AVeyraPlayerController* Player = LocalControllerOf(State.World);
					const FVector At = Player->GetVanguard()->GetActorLocation();
					Player->IssueMoveOrder(At + (FVector::ZeroVector - At).GetSafeNormal2D() * FountainRadius * 6.0);
				})
				.UntilServer(TEXT("It stops at the fountain's edge"), [](FState& State) {
					const APawn* Vanguard = ServerParticipant(State).GetPawn();
					return Vanguard && Vanguard->GetVelocity().IsNearlyZero() && FMath::Abs(FromHome(State) - FountainRadius) < Tolerance;
				})
				.ThenServer([this](FState& State) { HoldStartRealTime = State.World->GetRealTimeSeconds(); })
				.UntilServer(TEXT("Give it time to go further"), [this](FState& State) { return State.World->GetRealTimeSeconds() - HoldStartRealTime >= HoldRealSeconds; })
				.ThenServer(TEXT("It never left, and no other order is taken"), [this](FState& State) {
					ASSERT_THAT(IsTrue(FromHome(State) < FountainRadius + Tolerance, TEXT("still inside its fountain")));
					AVeyraGameMode* GameMode = State.World->GetAuthGameMode<AVeyraGameMode>();
					AVeyraPlayerState& Participant = ServerParticipant(State);
					AVeyraPlayerState& Other = *ServerControllerOf(State, 1)->GetPlayerState<AVeyraPlayerState>();
					ASSERT_THAT(IsTrue(GameMode->HandleAttackOrder(&Participant, Other.GetPawn()) == EVeyraOrderRejection::WrongPhase, TEXT("no attack")));
					ASSERT_THAT(IsTrue(GameMode->HandleAttackMoveOrder(&Participant, State.Home, EVeyraAttackMoveTarget::ClosestToVanguard) == EVeyraOrderRejection::WrongPhase, TEXT("no attack-move")));
					FVeyraCastTarget Target;
					Target.bHasLocation = true;
					Target.Location = State.Home;
					ASSERT_THAT(IsTrue(GameMode->HandleCastOrder(&Participant, EVeyraAbilitySlot::Q, Target) == EVeyraCastRejection::WrongPhase, TEXT("no cast")));
					ASSERT_THAT(IsTrue(GameMode->HandleRecallOrder(&Participant) == EVeyraOrderRejection::WrongPhase, TEXT("no Recall")));
				});
		}

		TEST_METHOD(AVanguardFoundOutsideItsFountainIsBroughtBack)
		{
			// A route navigation found around something near the rim might take it out; the fountain tick brings it back.
			StartMatch(Network, Layout, EVeyraMatchPhase::Preparation)
				.ThenServer(TEXT("Find its fountain, and put the Vanguard outside it"), [](FState& State) {
					State.Home = HomeOf(State.World, ServerParticipant(State).GetVeyraTeam());
					APawn* Vanguard = ServerParticipant(State).GetPawn();
					const FVector Out = State.Home + (FVector::ZeroVector - State.Home).GetSafeNormal2D() * FountainRadius * 2.0;
					Vanguard->SetActorLocation(FVector(Out.X, Out.Y, Vanguard->GetActorLocation().Z), /*bSweep*/ false, nullptr, ETeleportType::TeleportPhysics);
				})
				.UntilServer(TEXT("It is back at its fountain's edge"), [](FState& State) { return FromHome(State) <= FountainRadius + 1.0; });
		}
	};
}

#endif // ENABLE_PIE_NETWORK_TEST