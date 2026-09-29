// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"

#if ENABLE_PIE_NETWORK_TEST

#include "Movement/VeyraMovementComponent.h"
#include "Tests/Net/VeyraMatchNetTestHelpers.h"
#include "Tests/Net/VeyraNetTestHelpers.h"
#include "Tests/Net/VeyraVanguardNetTestHelpers.h"

namespace VeyraNetTests
{
	// Veyra.Net.Vanguards.Gorraveth.*: Gorraveth's movement in a match (Roster Bible §23): Rip Through's
	// lunge hurts what it drags through, and Ravine Bound lands among its target's side. Both players
	// are Gorraveth at level 6 with a rank in each ability; amounts come from the tuning.
	NETWORK_TEST_CLASS(Gorraveth, "Veyra.Net.Vanguards")
	{
		struct FState : public FBasePIENetworkComponentState
		{
		};

		FPIENetworkComponent<FState> Network{ TestRunner, TestCommandBuilder, bInitializing };
		TUniquePtr<FScopedExpectedPlayers> ExpectedPlayers;
		TUniquePtr<FScopedMatchTuning> Tuning;
		FVeyraGreyboxLayout Layout;
		FVanguardDuel Duel;

		// Fixture values: a short preparation, the level that opens the first ultimate rank, and how far
		// apart the Vanguards stand for the lunge and for the leap.
		static constexpr double ShortPreparationSeconds = 0.1;
		static constexpr int32 UltimateLevel = 6;
		static constexpr double LungeDistance = 250.0;
		static constexpr double LeapDistance = 550.0;

		BEFORE_EACH()
		{
			IgnoreKnownIrisWarnings(*TestRunner);
			ASSERT_THAT(IsTrue(VeyraGreybox::LoadLayout(Layout).IsEmpty()));
			Tuning = MakeUnique<FScopedMatchTuning>();
			Tuning->Tuning.Phases.PreparationSeconds = ShortPreparationSeconds;
			Tuning->Tuning.DeveloperMatch.Vanguards = { ContentId(TEXT("gorraveth")) };
			Tuning->Tuning.DeveloperMatch.StartingRank = EVeyraDeveloperStartingRank::None;
			ExpectedPlayers = MakeUnique<FScopedExpectedPlayers>(MatchClientCount);
			BuildMatchNetwork(Network);
		}

		AFTER_EACH()
		{
			Tuning.Reset();
			ExpectedPlayers.Reset();
		}

		static bool IsDashing(FState& State)
		{
			const APawn* Body = ParticipantOf(State, 0)->GetPawn();
			const UVeyraMovementComponent* Movement = Body ? Body->FindComponentByClass<UVeyraMovementComponent>() : nullptr;
			return Movement && Movement->IsDashing();
		}

		TEST_METHOD(RipThroughHurtsWhatItDragsThrough)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Prepare the duel"), [this](FState& State) { ASSERT_THAT(IsTrue(Duel.Prepare(State, ContentId(TEXT("gorraveth")), UltimateLevel, LungeDistance))); })
				.ThenClient(TEXT("Lunge"), 0, [this](FState& State) { Duel.CastAtTheTarget(State.World, EVeyraAbilitySlot::Q); })
				.UntilServer(TEXT("The enemy is cut"), [](FState& State) { return HealthLost(ParticipantOf(State, 1)) > 0.0; });
		}

		TEST_METHOD(RavineBoundLandsAmongThem)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Prepare the duel"), [this](FState& State) { ASSERT_THAT(IsTrue(Duel.Prepare(State, ContentId(TEXT("gorraveth")), UltimateLevel, LeapDistance))); })
				.ThenClient(TEXT("Leap"), 0, [this](FState& State) { Duel.CastAtTheTarget(State.World, EVeyraAbilitySlot::E); })
				.UntilServer(TEXT("He lands on the enemy"), [](FState& State) { return HealthLost(ParticipantOf(State, 1)) > 0.0; })
				.ThenServer(TEXT("And the leap is over"), [this](FState& State) { ASSERT_THAT(IsFalse(IsDashing(State))); });
		}
	};
}

#endif // ENABLE_PIE_NETWORK_TEST
