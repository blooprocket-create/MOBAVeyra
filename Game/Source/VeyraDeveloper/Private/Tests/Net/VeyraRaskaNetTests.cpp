// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"

#if ENABLE_PIE_NETWORK_TEST

#include "Movement/VeyraMovementComponent.h"
#include "Passives/VeyraMomentumPassive.h"
#include "Tests/Net/VeyraMatchNetTestHelpers.h"
#include "Tests/Net/VeyraNetTestHelpers.h"
#include "Tests/Net/VeyraVanguardNetTestHelpers.h"
#include "VeyraVanguardController.h"

namespace VeyraNetTests
{
	// Veyra.Net.Vanguards.Raska.*: Raska's kit in a match (Roster Bible §1): Momentum by the distance
	// she moves, Kickstart's ride and Bail Out sending Hound on, and NO BRAKES ending in Last Exit.
	// Both players are Raska at level 6 with a rank in each ability; amounts come from the tuning.
	NETWORK_TEST_CLASS(Raska, "Veyra.Net.Vanguards")
	{
		struct FState : public FBasePIENetworkComponentState
		{
		};

		FPIENetworkComponent<FState> Network{ TestRunner, TestCommandBuilder, bInitializing };
		TUniquePtr<FScopedExpectedPlayers> ExpectedPlayers;
		TUniquePtr<FScopedMatchTuning> Tuning;
		FVeyraGreyboxLayout Layout;
		FVanguardDuel Duel;

		// Fixture values: a short preparation, the level that opens the first ultimate rank, how far apart
		// the Vanguards stand, and how far she walks.
		static constexpr double ShortPreparationSeconds = 0.1;
		static constexpr int32 UltimateLevel = 6;
		static constexpr double Apart = 500.0;
		static constexpr int32 WalkedPoints = 8;

		BEFORE_EACH()
		{
			IgnoreKnownIrisWarnings(*TestRunner);
			ASSERT_THAT(IsTrue(VeyraGreybox::LoadLayout(Layout).IsEmpty()));
			Tuning = MakeUnique<FScopedMatchTuning>();
			Tuning->Tuning.Phases.PreparationSeconds = ShortPreparationSeconds;
			Tuning->Tuning.DeveloperMatch.Vanguards = { ContentId(TEXT("raska")) };
			Tuning->Tuning.DeveloperMatch.StartingRank = EVeyraDeveloperStartingRank::None;
			ExpectedPlayers = MakeUnique<FScopedExpectedPlayers>(MatchClientCount);
			BuildMatchNetwork(Network);
		}

		AFTER_EACH()
		{
			Tuning.Reset();
			ExpectedPlayers.Reset();
		}

		static UVeyraMomentumPassive* MomentumOf(FState& State)
		{
			return Cast<UVeyraMomentumPassive>(ParticipantOf(State, 0)->GetPassive());
		}

		static UVeyraMovementComponent* MovementOf(FState& State)
		{
			const APawn* Body = ParticipantOf(State, 0)->GetPawn();
			return Body ? Body->FindComponentByClass<UVeyraMovementComponent>() : nullptr;
		}

		TEST_METHOD(WalkingBuildsMomentum)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Prepare the duel, then walk to the enemy"), [this](FState& State) {
					ASSERT_THAT(IsTrue(Duel.Prepare(State, ContentId(TEXT("raska")), UltimateLevel, Apart)));
					const FVector Where = ParticipantOf(State, 1)->GetPawn()->GetActorLocation();
					ASSERT_THAT(IsTrue(ParticipantOf(State, 0)->GetVanguardController()->MoveToDestination(Where) == EVeyraOrderRejection::None));
				})
				.UntilServer(TEXT("She arrives"), [](FState& State) { return !ParticipantOf(State, 0)->GetVanguardController()->GetMoveOrder().IsSet(); })
				.UntilServer(TEXT("Her Momentum shows it"), [](FState& State) { return MomentumOf(State) && MomentumOf(State)->GetMomentum() >= WalkedPoints; });
		}

		TEST_METHOD(KickstartRidesAndBailOutSendsHoundOn)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Prepare the duel, facing the enemy"), [this](FState& State) {
					ASSERT_THAT(IsTrue(Duel.Prepare(State, ContentId(TEXT("raska")), UltimateLevel, Apart)));
					APawn* Body = ParticipantOf(State, 0)->GetPawn();
					Body->SetActorRotation((ParticipantOf(State, 1)->GetPawn()->GetActorLocation() - Body->GetActorLocation()).GetSafeNormal2D().Rotation());
				})
				.ThenClient(TEXT("Kickstart"), 0, [this](FState& State) { Duel.CastAtTheTarget(State.World, EVeyraAbilitySlot::E); })
				.UntilServer(TEXT("She rides"), [](FState& State) { return MovementOf(State) && MovementOf(State)->IsRiding(); })
				.ThenClient(TEXT("Bail out toward the enemy"), 0, [this](FState& State) { Duel.CastAtTheTarget(State.World, EVeyraAbilitySlot::E); })
				.UntilServer(TEXT("Hound hits the enemy"), [](FState& State) { return HealthLost(ParticipantOf(State, 1)) > 0.0; })
				.ThenServer(TEXT("And she is off the ride"), [this](FState& State) { ASSERT_THAT(IsFalse(MovementOf(State)->IsRiding())); });
		}

		TEST_METHOD(NoBrakesEndsInLastExit)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Prepare the duel"), [this](FState& State) { ASSERT_THAT(IsTrue(Duel.Prepare(State, ContentId(TEXT("raska")), UltimateLevel, Apart))); })
				.ThenClient(TEXT("No brakes"), 0, [this](FState& State) { Duel.CastAtTheTarget(State.World, EVeyraAbilitySlot::R); })
				.UntilServer(TEXT("She rides, Unstoppable"), [](FState& State) { return MovementOf(State) && MovementOf(State)->IsRiding(); })
				.ThenClient(TEXT("Last Exit onto the enemy"), 0, [this](FState& State) { Duel.CastAtTheTarget(State.World, EVeyraAbilitySlot::R); })
				.UntilClients(TEXT("Every client sees the enemy knocked up"), [this](FState& State) { return SeesStatus(State.World, Duel.TargetId, EVeyraStatusKind::Knockup); });
		}
	};
}

#endif // ENABLE_PIE_NETWORK_TEST
