// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"

#if ENABLE_PIE_NETWORK_TEST

#include "State/VeyraTeamFluxState.h"
#include "Tests/Net/VeyraMatchNetTestHelpers.h"
#include "Tests/Net/VeyraNetTestHelpers.h"
#include "Tuning/VeyraFluxTuningSubsystem.h"
#include "VeyraTeamFluxSubsystem.h"

namespace VeyraNetTests
{
	// Veyra.Net.TeamFlux.*: the server's Team Flux reaches every client, both teams' (ADR-011 §10).
	NETWORK_TEST_CLASS(TeamFlux, "Veyra.Net")
	{
		struct FState : public FBasePIENetworkComponentState
		{
		};

		FPIENetworkComponent<FState> Network{ TestRunner, TestCommandBuilder, bInitializing };
		TUniquePtr<FScopedExpectedPlayers> ExpectedPlayers;
		TUniquePtr<FScopedMatchTuning> Tuning;
		FVeyraGreyboxLayout Layout;

		// Fixture value: a short preparation.
		static constexpr double ShortPreparationSeconds = 0.1;

		BEFORE_EACH()
		{
			IgnoreKnownIrisWarnings(*TestRunner);
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

		TEST_METHOD(EveryClientSeesBothTeamsFlux)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Grant each team Flux"), [](FState& State) {
					UVeyraTeamFluxSubsystem* Flux = State.World->GetSubsystem<UVeyraTeamFluxSubsystem>();
					Flux->Grant(EVeyraTeam::A, EVeyraFluxSource::Inhibitor);
					Flux->Grant(EVeyraTeam::B, EVeyraFluxSource::LaneSpire);
				})
				.UntilClients(TEXT("Every client sees them"), [](FState& State) {
					const FVeyraFluxGrantsTuning& Grants = UVeyraFluxTuningSubsystem::Get().Grants;
					const AVeyraTeamFluxState* Published = AVeyraTeamFluxState::Find(State.World);
					const FVeyraTeamFluxView* TeamA = Published ? Published->Find(EVeyraTeam::A) : nullptr;
					const FVeyraTeamFluxView* TeamB = Published ? Published->Find(EVeyraTeam::B) : nullptr;
					return TeamA && TeamB && TeamA->Permanent == 0.0 && TeamA->Temporary.Num() == 1 && TeamA->Temporary[0].Amount == Grants.Inhibitor.Amount
						&& TeamB->Permanent == Grants.LaneSpire.Amount && TeamB->Temporary.IsEmpty();
				});
		}
	};
}

#endif // ENABLE_PIE_NETWORK_TEST
