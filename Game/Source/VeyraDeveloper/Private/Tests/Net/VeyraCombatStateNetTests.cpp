// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"

#if ENABLE_PIE_NETWORK_TEST

#include "AbilitySystemComponent.h"
#include "CombatState/VeyraCombatStateComponent.h"
#include "GameFramework/PlayerState.h"
#include "Targeting/VeyraTargeting.h"
#include "Tests/Net/VeyraMatchNetTestHelpers.h"
#include "Tests/Net/VeyraNetTestHelpers.h"
#include "Tuning/VeyraCombatTuningSubsystem.h"
#include "VeyraCombatVerbs.h"
#include "VeyraPlayerState.h"

namespace VeyraNetTests
{
	// Veyra.Net.CombatStateReplication.*: a fight puts both Vanguards in Combat State for every client
	// to see, and the server takes them out of it once the delay passes (Combat Bible §28).
	NETWORK_TEST_CLASS(CombatStateReplication, "Veyra.Net")
	{
		struct FState : public FBasePIENetworkComponentState
		{
		};

		FPIENetworkComponent<FState> Network{ TestRunner, TestCommandBuilder, bInitializing };
		TUniquePtr<FScopedExpectedPlayers> ExpectedPlayers;
		TUniquePtr<FScopedMatchTuning> Tuning;
		FVeyraCombatTuning CombatTuning;
		FVeyraGreyboxLayout Layout;

		// Fixture values: a short preparation, a short out-of-combat delay, and a small hit.
		static constexpr double ShortPreparationSeconds = 0.1;
		static constexpr double ShortOutOfCombatSeconds = 1.0;
		static constexpr double SmallHit = 10.0;

		TArray<int32> FighterIds;
		double FoughtAt = 0.0;

		BEFORE_EACH()
		{
			IgnoreLoginViewTargetRpc(*TestRunner);
			ASSERT_THAT(IsTrue(VeyraGreybox::LoadLayout(Layout).IsEmpty()));
			Tuning = MakeUnique<FScopedMatchTuning>();
			Tuning->Tuning.Phases.PreparationSeconds = ShortPreparationSeconds;
			CombatTuning = UVeyraCombatTuningSubsystem::Get();
			CombatTuning.CombatState.OutOfCombatSeconds = ShortOutOfCombatSeconds;
			UVeyraCombatTuningSubsystem::SetTestOverride(&CombatTuning);
			ExpectedPlayers = MakeUnique<FScopedExpectedPlayers>(MatchClientCount);
			BuildMatchNetwork(Network);
		}

		AFTER_EACH()
		{
			UVeyraCombatTuningSubsystem::SetTestOverride(nullptr);
			Tuning.Reset();
			ExpectedPlayers.Reset();
		}

		/** Whether every fighter is in Combat State as this machine sees it. */
		bool FightersInCombat(FState& State, bool bExpected) const
		{
			const AVeyraGameState* GameState = GameStateOf(State.World);
			if (!GameState)
			{
				return false;
			}
			int32 Matching = 0;
			for (const APlayerState* Participant : GameState->PlayerArray)
			{
				const UVeyraCombatStateComponent* CombatState = Participant ? Participant->FindComponentByClass<UVeyraCombatStateComponent>() : nullptr;
				Matching += CombatState && FighterIds.Contains(Participant->GetPlayerId()) && CombatState->IsInCombat() == bExpected ? 1 : 0;
			}
			return Matching == FighterIds.Num();
		}

		TEST_METHOD(AFightShowsForEveryoneUntilTheDelayPasses)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("The two sides trade a hit"), [this](FState& State) {
					AVeyraPlayerState* First = ServerControllerOf(State, 0)->GetPlayerState<AVeyraPlayerState>();
					AVeyraPlayerState* Second = ServerControllerOf(State, 1)->GetPlayerState<AVeyraPlayerState>();
					ASSERT_THAT(IsTrue(First && Second && VeyraTargeting::AreHostile(First, Second)));
					FighterIds = { First->GetPlayerId(), Second->GetPlayerId() };
					FVeyraRawDamageEvent Hit;
					Hit.Components.Add({ EVeyraDamageType::TrueDamage, SmallHit });
					ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*First->GetAbilitySystemComponent(), *Second->GetAbilitySystemComponent(), Hit)));
					FoughtAt = State.World->GetTimeSeconds();
				})
				.UntilClients(TEXT("Every client sees both in combat"), [this](FState& State) { return FightersInCombat(State, true); })
				.UntilServer(TEXT("The delay passes"), [this](FState& State) { return FightersInCombat(State, false); })
				.ThenServer(TEXT("Not before the delay"), [this](FState& State) {
					ASSERT_THAT(IsTrue(State.World->GetTimeSeconds() >= FoughtAt + ShortOutOfCombatSeconds));
				})
				.UntilClients(TEXT("Every client sees both leave combat"), [this](FState& State) { return FightersInCombat(State, false); });
		}
	};
}

#endif // ENABLE_PIE_NETWORK_TEST
