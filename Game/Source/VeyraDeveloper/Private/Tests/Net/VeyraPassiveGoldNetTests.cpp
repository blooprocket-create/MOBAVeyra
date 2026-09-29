// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"

#if ENABLE_PIE_NETWORK_TEST

#include "Gold/VeyraGoldComponent.h"
#include "Rewards/VeyraEconomyTuningSubsystem.h"
#include "Tests/Net/VeyraMatchNetTestHelpers.h"
#include "Tests/Net/VeyraNetTestHelpers.h"
#include "VeyraPlayerController.h"
#include "VeyraPlayerState.h"

namespace VeyraNetTests
{
	// Veyra.Net.PassiveGold.*: once the match is live, every player earns passive Gold, and its owner
	// sees it (author ruling, 2026-09-28, amending Economy & Progression Bible §1).
	NETWORK_TEST_CLASS(PassiveGold, "Veyra.Net")
	{
		struct FState : public FBasePIENetworkComponentState
		{
		};

		FPIENetworkComponent<FState> Network{ TestRunner, TestCommandBuilder, bInitializing };
		TUniquePtr<FScopedExpectedPlayers> ExpectedPlayers;
		TUniquePtr<FScopedMatchTuning> Tuning;
		FVeyraEconomyTuning Economy;
		FVeyraGreyboxLayout Layout;
		/** Each player's Gold as the match went live, for the client steps. */
		TArray<double> GoldAtLive;

		// Fixture values: a quick income from the moment the match goes live.
		static constexpr double ShortPreparationSeconds = 0.1;
		static constexpr double Payment = 9.0;
		static constexpr double ShortIntervalSeconds = 0.5;

		BEFORE_EACH()
		{
			IgnoreKnownIrisWarnings(*TestRunner);
			ASSERT_THAT(IsTrue(VeyraGreybox::LoadLayout(Layout).IsEmpty()));
			Tuning = MakeUnique<FScopedMatchTuning>();
			Tuning->Tuning.Phases.PreparationSeconds = ShortPreparationSeconds;
			Economy = UVeyraEconomyTuningSubsystem::Get();
			Economy.PassiveGold.PerPayment = Payment;
			Economy.PassiveGold.IntervalSeconds = ShortIntervalSeconds;
			Economy.PassiveGold.StartSeconds = 0.0;
			UVeyraEconomyTuningSubsystem::SetTestOverride(&Economy);
			ExpectedPlayers = MakeUnique<FScopedExpectedPlayers>(MatchClientCount);
			BuildMatchNetwork(Network);
		}

		AFTER_EACH()
		{
			UVeyraEconomyTuningSubsystem::SetTestOverride(nullptr);
			Tuning.Reset();
			ExpectedPlayers.Reset();
		}

		static double GoldOf(const APlayerState& Participant)
		{
			return Participant.FindComponentByClass<UVeyraGoldComponent>()->GetGold();
		}

		TEST_METHOD(ALiveMatchPaysEveryPlayer)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Note each player's Gold"), [this](FState& State) {
					GoldAtLive.Reset();
					for (int32 Client = 0; Client < MatchClientCount; ++Client)
					{
						GoldAtLive.Add(GoldOf(*ServerControllerOf(State, Client)->GetPlayerState<AVeyraPlayerState>()));
					}
				})
				.UntilServer(TEXT("Every player earns a payment"), [this](FState& State) {
					for (int32 Client = 0; Client < MatchClientCount; ++Client)
					{
						if (GoldOf(*ServerControllerOf(State, Client)->GetPlayerState<AVeyraPlayerState>()) < GoldAtLive[Client] + Payment)
						{
							return false;
						}
					}
					return true;
				})
				.UntilClient(TEXT("Its owner sees it"), 0, [this](FState& State) {
					const AVeyraPlayerState* Own = LocalControllerOf(State.World)->GetPlayerState<AVeyraPlayerState>();
					return Own && GoldOf(*Own) >= GoldAtLive[0] + Payment;
				});
		}
	};
}

#endif // ENABLE_PIE_NETWORK_TEST
