// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"

#if ENABLE_PIE_NETWORK_TEST

#include "AbilitySystemComponent.h"
#include "Bots/VeyraMatchEvents.h"
#include "Brain/VeyraBotBrainComponent.h"
#include "Tests/Net/VeyraMatchNetTestHelpers.h"
#include "Tests/Net/VeyraNetTestHelpers.h"
#include "Tuning/VeyraBotsTuningSubsystem.h"
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

		TEST_METHOD(PlayingBotsAreAnnouncedWithTheirSeats)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Seat two playing bots on one side"), [this](FState& State) {
					TArray<TPair<AVeyraPlayerState*, FVeyraBotSeat>> Announced;
					UVeyraMatchEvents* Events = State.World->GetSubsystem<UVeyraMatchEvents>();
					ASSERT_THAT(IsNotNull(Events));
					const FDelegateHandle Handle = Events->OnBotAdded.AddLambda([&Announced](AVeyraPlayerState& Bot, const FVeyraBotSeat& Seat) {
						Announced.Emplace(&Bot, Seat);
					});
					const FVeyraContentId Vanguard = TestVanguardId();
					AVeyraPlayerState* First = GameModeOf(State.World)->AddPlayingBot(TEXT("First"), EVeyraTeam::B, Vanguard, EVeyraBotDifficulty::Beginner);
					AVeyraPlayerState* Second = GameModeOf(State.World)->AddPlayingBot(TEXT("Second"), EVeyraTeam::B, Vanguard, EVeyraBotDifficulty::Intermediate);
					// A target seated by AddBotParticipant is not announced: it gets no brain.
					const AVeyraPlayerState* Target = GameModeOf(State.World)->AddBotParticipant(TEXT("Target"));
					Events->OnBotAdded.Remove(Handle);
					ASSERT_THAT(IsTrue(First && Second && Target && Announced.Num() == 2));
					ASSERT_THAT(IsTrue(Announced[0].Key == First && Announced[0].Value.Seat == 0 && Announced[0].Value.Difficulty == EVeyraBotDifficulty::Beginner));
					ASSERT_THAT(IsTrue(Announced[1].Key == Second && Announced[1].Value.Seat == 1 && Announced[1].Value.Difficulty == EVeyraBotDifficulty::Intermediate));
					ASSERT_THAT(IsTrue(Announced[1].Value.Side == EVeyraTeam::B && Announced[1].Value.Vanguard == Vanguard));
				});
		}

		TEST_METHOD(BotsSeatedInPreparationAreDealtTheirPlacesTogether)
		{
			// Each spawns as it is seated in preparation; its side still deals the places of all it seated before the
			// match goes live (ADR-039 §5). Varkesh plays Top before Mid, Eudora Mid before Top: seated in that order
			// in the Mid and Top seats, they swap.
			StartMatch(Network, Layout, EVeyraMatchPhase::Preparation)
				.ThenServer(TEXT("Seat Varkesh, then Eudora, on one side"), [this](FState& State) {
					const AVeyraPlayerState* First = GameModeOf(State.World)->AddPlayingBot(TEXT("First"), EVeyraTeam::B, FVeyraContentId::FromText(TEXT("varkesh")).GetValue(), EVeyraBotDifficulty::Beginner);
					const AVeyraPlayerState* Second = GameModeOf(State.World)->AddPlayingBot(TEXT("Second"), EVeyraTeam::B, FVeyraContentId::FromText(TEXT("eudora")).GetValue(), EVeyraBotDifficulty::Beginner);
					ASSERT_THAT(IsTrue(First && Second && First->GetPawn(), TEXT("seated in preparation, each has its Vanguard at once")));
					const TArray<FVeyraBotSeatTuning>& Seats = UVeyraBotsTuningSubsystem::Get().Seats;
					const auto RoleOf = [](const AVeyraPlayerState& Bot) {
						const UVeyraBotBrainComponent* Brain = Bot.GetVanguardController()->FindComponentByClass<UVeyraBotBrainComponent>();
						return Brain ? Brain->GetRole() : EVeyraBotRole::Jungle;
					};
					ASSERT_THAT(IsTrue(RoleOf(*First) == Seats[1].Role && RoleOf(*Second) == Seats[0].Role,
						FString::Printf(TEXT("Varkesh plays %s, Eudora %s"), *UEnum::GetValueAsString(RoleOf(*First)), *UEnum::GetValueAsString(RoleOf(*Second)))));
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
