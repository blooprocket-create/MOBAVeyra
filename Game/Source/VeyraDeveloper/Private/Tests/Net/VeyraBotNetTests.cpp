// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"

#if ENABLE_PIE_NETWORK_TEST

#include "AbilitySystemComponent.h"
#include "Tests/Net/VeyraMatchNetTestHelpers.h"
#include "Tests/Net/VeyraNetTestHelpers.h"
#include "VeyraPlayerState.h"
#include "VeyraVanguardController.h"

namespace VeyraNetTests
{
	// Veyra.Net.Bots.*: an AI-controlled participant has its own PlayerState, a side and a Vanguard,
	// through the same path as a player (ADR-006 §4), and every client sees it.
	NETWORK_TEST_CLASS(Bots, "Veyra.Net")
	{
		struct FState : public FBasePIENetworkComponentState
		{
		};

		FPIENetworkComponent<FState> Network{ TestRunner, TestCommandBuilder, bInitializing };
		TUniquePtr<FScopedExpectedPlayers> ExpectedPlayers;
		TUniquePtr<FScopedMatchTuning> Tuning;
		FVeyraGreyboxLayout Layout;

		static constexpr double ShortPreparationSeconds = 0.1;

		int32 BotId = INDEX_NONE;

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

		TEST_METHOD(ABotJoinsWithItsOwnPlayerStateAndVanguard)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Add a bot"), [this](FState& State) {
					AVeyraPlayerState* Bot = GameModeOf(State.World)->AddBotParticipant(TEXT("TestBot"));
					ASSERT_THAT(IsNotNull(Bot));
					BotId = Bot->GetPlayerId();
					ASSERT_THAT(IsTrue(Bot->IsABot()));
					ASSERT_THAT(IsTrue(Bot->GetVeyraTeam() != EVeyraTeam::None));
					APawn* Body = Bot->GetPawn();
					ASSERT_THAT(IsNotNull(Body));
					ASSERT_THAT(IsTrue(Body->GetController() == Bot->GetVanguardController()));
					ASSERT_THAT(IsTrue(Bot->GetVanguardController()->PlayerState == Bot));
					ASSERT_THAT(IsTrue(Bot->GetAbilitySystemComponent()->GetAvatarActor() == Body));
				})
				.UntilClients(TEXT("Every client sees the bot and its Vanguard"), [this](FState& State) {
					const AVeyraVanguardCharacter* Body = FindVanguard(State.World, BotId);
					return Body && Body->GetPlayerState() && Body->GetPlayerState()->IsABot();
				});
		}

		TEST_METHOD(AFullMatchRefusesABot)
		{
			// One per side, and both players are already in.
			Tuning->Tuning.Teams.MaxTeamSize = 1;
			TestRunner->AddExpectedMessagePlain(TEXT("Cannot add TestBot: the match is full."), ELogVerbosity::Warning,
				EAutomationExpectedMessageFlags::Contains, 1);
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Try to add a bot"), [this](FState& State) {
					ASSERT_THAT(IsNull(GameModeOf(State.World)->AddBotParticipant(TEXT("TestBot"))));
				});
		}
	};
}

#endif // ENABLE_PIE_NETWORK_TEST
