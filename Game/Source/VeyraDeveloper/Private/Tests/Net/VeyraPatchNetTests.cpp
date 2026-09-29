// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"

#if ENABLE_PIE_NETWORK_TEST

#include "Absorption/VeyraDamageAbsorptionComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "Movement/VeyraMovementComponent.h"
#include "Tests/Net/VeyraMatchNetTestHelpers.h"
#include "Tests/Net/VeyraNetTestHelpers.h"
#include "Tests/Net/VeyraVanguardNetTestHelpers.h"

namespace VeyraNetTests
{
	// Veyra.Net.Vanguards.Patch.*: Patch's kit in a match (Roster Bible §5): Bear Hug's leap, hold and
	// throw, Play Dead's Fear pulse, and The Thing Inside's growth and chill. Both players are Patch at
	// level 6 with a rank in each ability; amounts come from the tuning.
	NETWORK_TEST_CLASS(Patch, "Veyra.Net.Vanguards")
	{
		struct FState : public FBasePIENetworkComponentState
		{
		};

		FPIENetworkComponent<FState> Network{ TestRunner, TestCommandBuilder, bInitializing };
		TUniquePtr<FScopedExpectedPlayers> ExpectedPlayers;
		TUniquePtr<FScopedMatchTuning> Tuning;
		FVeyraGreyboxLayout Layout;
		FVanguardDuel Duel;

		// Fixture values: a short preparation, the level that opens the first ultimate rank, how far
		// apart the Vanguards stand for the leap and for the pulse, and where the radius was before.
		static constexpr double ShortPreparationSeconds = 0.1;
		static constexpr int32 UltimateLevel = 6;
		static constexpr double LeapDistance = 400.0;
		static constexpr double CloseDistance = 250.0;
		double RadiusBefore = 0.0;

		BEFORE_EACH()
		{
			IgnoreKnownIrisWarnings(*TestRunner);
			ASSERT_THAT(IsTrue(VeyraGreybox::LoadLayout(Layout).IsEmpty()));
			Tuning = MakeUnique<FScopedMatchTuning>();
			Tuning->Tuning.Phases.PreparationSeconds = ShortPreparationSeconds;
			Tuning->Tuning.DeveloperMatch.Vanguards = { ContentId(TEXT("patch")) };
			Tuning->Tuning.DeveloperMatch.StartingRank = EVeyraDeveloperStartingRank::None;
			ExpectedPlayers = MakeUnique<FScopedExpectedPlayers>(MatchClientCount);
			BuildMatchNetwork(Network);
		}

		AFTER_EACH()
		{
			Tuning.Reset();
			ExpectedPlayers.Reset();
		}

		void PrepareDuel(FState& State, double Distance)
		{
			ASSERT_THAT(IsTrue(Duel.Prepare(State, ContentId(TEXT("patch")), UltimateLevel, Distance)));
		}

		static UVeyraMovementComponent* MovementOf(FState& State, int32 Client)
		{
			const APawn* Body = ParticipantOf(State, Client)->GetPawn();
			return Body ? Body->FindComponentByClass<UVeyraMovementComponent>() : nullptr;
		}

		TEST_METHOD(BearHugLeapsOnAndItsRecastThrowsHimOff)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Prepare the duel"), [this](FState& State) { PrepareDuel(State, LeapDistance); })
				.ThenClient(TEXT("Hug"), 0, [this](FState& State) { Duel.CastAtTheTarget(State.World, EVeyraAbilitySlot::Q); })
				.UntilServer(TEXT("He holds on"), [](FState& State) {
					const UVeyraMovementComponent* Movement = MovementOf(State, 0);
					return Movement && Movement->IsAttached() && Movement->GetAttachHost() == ParticipantOf(State, 1)->GetPawn();
				})
				.UntilClients(TEXT("Every client sees the enemy slowed"), [this](FState& State) { return SeesStatus(State.World, Duel.TargetId, EVeyraStatusKind::Slow); })
				.ThenClient(TEXT("Throw"), 0, [this](FState& State) { Duel.CastAtTheTarget(State.World, EVeyraAbilitySlot::Q); })
				.UntilServer(TEXT("He lets go, thrown back"), [](FState& State) { return !MovementOf(State, 0)->IsAttached(); })
				.ThenServer(TEXT("The hug hurt"), [this](FState& State) { ASSERT_THAT(IsTrue(HealthLost(ParticipantOf(State, 1)) > 0.0)); });
		}

		TEST_METHOD(PlayDeadEndsInAFear)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Prepare the duel"), [this](FState& State) { PrepareDuel(State, CloseDistance); })
				.ThenClient(TEXT("Play dead"), 0, [this](FState& State) { Duel.CastAtTheTarget(State.World, EVeyraAbilitySlot::W); })
				.UntilClients(TEXT("Every client sees the enemy feared"), [this](FState& State) { return SeesStatus(State.World, Duel.TargetId, EVeyraStatusKind::Fear); });
		}

		TEST_METHOD(TheThingInsideGrowsHimAndChillsWhoIsNear)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Prepare the duel"), [this](FState& State) {
					PrepareDuel(State, CloseDistance);
					RadiusBefore = CastChecked<ACharacter>(ParticipantOf(State, 0)->GetPawn())->GetCapsuleComponent()->GetUnscaledCapsuleRadius();
				})
				.ThenClient(TEXT("Let it out"), 0, [this](FState& State) { Duel.CastAtTheTarget(State.World, EVeyraAbilitySlot::R); })
				.UntilClients(TEXT("Every client sees the enemy chilled"), [this](FState& State) { return SeesStatus(State.World, Duel.TargetId, EVeyraStatusKind::Slow); })
				.ThenServer(TEXT("He is bigger, with Temporary Health"), [this](FState& State) {
					const AVeyraPlayerState* Holder = ParticipantOf(State, 0);
					const double Radius = CastChecked<ACharacter>(Holder->GetPawn())->GetCapsuleComponent()->GetUnscaledCapsuleRadius();
					ASSERT_THAT(IsTrue(Radius > RadiusBefore, FString::Printf(TEXT("radius %g, before %g"), Radius, RadiusBefore)));
					ASSERT_THAT(IsTrue(Holder->FindComponentByClass<UVeyraDamageAbsorptionComponent>()->GetLedger().TemporaryHealth.Num() == 1));
				});
		}
	};
}

#endif // ENABLE_PIE_NETWORK_TEST
