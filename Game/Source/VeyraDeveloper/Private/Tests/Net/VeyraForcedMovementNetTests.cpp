// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"

#if ENABLE_PIE_NETWORK_TEST

#include "AbilitySystemComponent.h"
#include "GameFramework/PlayerState.h"
#include "Movement/VeyraMovementComponent.h"
#include "Targeting/VeyraTargeting.h"
#include "Tests/Net/VeyraMatchNetTestHelpers.h"
#include "Tests/Net/VeyraNetTestHelpers.h"
#include "VeyraCombatVerbs.h"
#include "VeyraPlayerState.h"
#include "VeyraVanguardController.h"

namespace VeyraNetTests
{
	// Veyra.Net.ForcedMovement.*: displacements and dashes run on the server and every client sees
	// the result; a held order resumes afterwards (Combat Bible §9; ADR-009 §2).
	NETWORK_TEST_CLASS(ForcedMovement, "Veyra.Net")
	{
		struct FState : public FBasePIENetworkComponentState
		{
		};

		FPIENetworkComponent<FState> Network{ TestRunner, TestCommandBuilder, bInitializing };
		TUniquePtr<FScopedExpectedPlayers> ExpectedPlayers;
		TUniquePtr<FScopedMatchTuning> Tuning;
		FVeyraGreyboxLayout Layout;

		// Fixture values: a short preparation, a sideways knockback, a fast dash, and comparison
		// allowances for float positions.
		static constexpr double ShortPreparationSeconds = 0.1;
		static constexpr double KnockbackDistance = 300.0;
		static constexpr double KnockbackSpeed = 600.0;
		static constexpr double DashSpeed = 5000.0;
		static constexpr double PositionSlack = 2.0;

		int32 MoverId = INDEX_NONE;
		FVector Expected = FVector::ZeroVector;
		TOptional<FVeyraDashEnd> DashEnd;

		BEFORE_EACH()
		{
			IgnoreLoginViewTargetRpc(*TestRunner);
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

		static AVeyraPlayerState* ParticipantOf(FState& State, int32 ClientIndex)
		{
			const AVeyraPlayerController* Controller = ServerControllerOf(State, ClientIndex);
			return Controller ? Controller->GetPlayerState<AVeyraPlayerState>() : nullptr;
		}

		static UVeyraMovementComponent* MovementOf(const AVeyraPlayerState* Participant)
		{
			const APawn* Body = Participant ? Participant->GetPawn() : nullptr;
			return Body ? Body->FindComponentByClass<UVeyraMovementComponent>() : nullptr;
		}

		bool IsNear2D(const AActor* Actor, const FVector& Point, double Allowance) const
		{
			return Actor && FVector::Dist2D(Actor->GetActorLocation(), Point) <= Allowance;
		}

		/** On the server: knocks the first client's Vanguard sideways, from the second's. */
		void KnockSideways(FState& State)
		{
			AVeyraPlayerState* Mover = ParticipantOf(State, 0);
			AVeyraPlayerState* Other = ParticipantOf(State, 1);
			ASSERT_THAT(IsTrue(Mover && Other && Mover->GetPawn()));
			MoverId = Mover->GetPlayerId();
			Expected = Mover->GetPawn()->GetActorLocation() + FVector(0.0, KnockbackDistance, 0.0);
			ASSERT_THAT(IsTrue(VeyraCombat::Displace(*Other->GetAbilitySystemComponent(), *Mover->GetAbilitySystemComponent(),
				FVeyraDisplacement{ FVector::RightVector, KnockbackDistance, KnockbackSpeed })));
		}

		TEST_METHOD(AKnockbackMovesTheVanguardForEveryone)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Knock the first Vanguard sideways"), [this](FState& State) { KnockSideways(State); })
				.UntilServer(TEXT("It lands"), [this](FState& State) {
					const UVeyraMovementComponent* Movement = MovementOf(ParticipantOf(State, 0));
					return Movement && !Movement->IsDisplaced() && IsNear2D(ParticipantOf(State, 0)->GetPawn(), Expected, PositionSlack);
				})
				.UntilClients(TEXT("Every client sees it land"), [this](FState& State) {
					return IsNear2D(FindVanguard(State.World, MoverId), Expected, PositionSlack);
				});
		}

		TEST_METHOD(AKnockbackStopsWhereWalkableGroundEnds)
		{
			// Well past the floor's side edge.
			const double PastTheEdge = Layout.Floor.WidthY;
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Knock the first Vanguard off the side of the floor"), [this, PastTheEdge](FState& State) {
					AVeyraPlayerState* Mover = ParticipantOf(State, 0);
					AVeyraPlayerState* Other = ParticipantOf(State, 1);
					ASSERT_THAT(IsTrue(Mover && Other && Mover->GetPawn()));
					Expected = Mover->GetPawn()->GetActorLocation();
					ASSERT_THAT(IsTrue(VeyraCombat::Displace(*Other->GetAbilitySystemComponent(), *Mover->GetAbilitySystemComponent(),
						FVeyraDisplacement{ FVector::RightVector, PastTheEdge, KnockbackSpeed * 2.0 })));
				})
				.UntilServer(TEXT("It lands"), [](FState& State) { return !MovementOf(ParticipantOf(State, 0))->IsDisplaced(); })
				.ThenServer(TEXT("At the edge, still on the floor"), [this](FState& State) {
					const FVector Landed = ParticipantOf(State, 0)->GetPawn()->GetActorLocation();
					const double FloorEdgeY = Layout.Floor.WidthY / 2.0;
					ASSERT_THAT(IsTrue(Landed.Y < FloorEdgeY && Landed.Y > Expected.Y + FloorEdgeY / 2.0, FString::Printf(TEXT("landed at Y %g"), Landed.Y)));
				});
		}

		TEST_METHOD(AHeldOrderResumesAfterADisplacement)
		{
			const FVector Destination = FVector::ZeroVector;
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Send the first Vanguard to the centre"), [this, Destination](FState& State) {
					ASSERT_THAT(IsTrue(ParticipantOf(State, 0)->GetVanguardController()->MoveToDestination(Destination) == EVeyraOrderRejection::None));
				})
				.UntilServer(TEXT("It is moving"), [](FState& State) { return !ParticipantOf(State, 0)->GetPawn()->GetVelocity().IsNearlyZero(); })
				.ThenServer(TEXT("Knock it sideways"), [this](FState& State) {
					KnockSideways(State);
					ASSERT_THAT(IsTrue(ParticipantOf(State, 0)->GetVanguardController()->GetMoveOrder().IsSet(), TEXT("the displacement dropped the order")));
				})
				.UntilServer(TEXT("It arrives after the displacement"), [this, Destination](FState& State) {
					return IsNear2D(ParticipantOf(State, 0)->GetPawn(), Destination, Tuning->Tuning.Orders.ArrivalTolerance + PositionSlack);
				});
		}

		TEST_METHOD(ADashStopsAtTheFirstEnemy)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Dash the first Vanguard at the second"), [this](FState& State) {
					AVeyraPlayerState* Mover = ParticipantOf(State, 0);
					AVeyraPlayerState* Target = ParticipantOf(State, 1);
					ASSERT_THAT(IsTrue(Mover && Target && Mover->GetPawn() && Target->GetPawn()));
					const FVector Between = Target->GetPawn()->GetActorLocation() - Mover->GetPawn()->GetActorLocation();
					MovementOf(Mover)->OnDashEnded.AddLambda([this](const FVeyraDashEnd& End) { DashEnd = End; });
					// Longer than the gap, so only the contact stops it.
					ASSERT_THAT(IsTrue(VeyraCombat::Dash(*Mover->GetAbilitySystemComponent(),
						FVeyraDash{ Between, Between.Size2D() * 2.0, DashSpeed, EVeyraDashContact::StopAtFirstEnemy })));
				})
				.UntilServer(TEXT("The dash ends"), [this](FState&) { return DashEnd.IsSet(); })
				.ThenServer(TEXT("Touching the enemy"), [this](FState& State) {
					const APawn* Mover = ParticipantOf(State, 0)->GetPawn();
					const APawn* Target = ParticipantOf(State, 1)->GetPawn();
					ASSERT_THAT(IsTrue(DashEnd->Reason == EVeyraDashEndReason::EnemyContact && DashEnd->Contact.Get() == Target));
					ASSERT_THAT(IsTrue(VeyraTargeting::EdgeToEdgeDistance(*Mover, *Target) <= PositionSlack,
						FString::Printf(TEXT("stopped %g units away"), VeyraTargeting::EdgeToEdgeDistance(*Mover, *Target))));
				});
		}
	};
}

#endif // ENABLE_PIE_NETWORK_TEST
