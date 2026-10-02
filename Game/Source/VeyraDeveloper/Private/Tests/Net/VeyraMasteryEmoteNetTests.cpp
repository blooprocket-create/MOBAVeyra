// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"

#if ENABLE_PIE_NETWORK_TEST

#include "GameFramework/GameStateBase.h"
#include "Join/VeyraMatchHostSubsystem.h"
#include "Tests/Net/VeyraMatchNetTestHelpers.h"
#include "Tests/Net/VeyraNetTestHelpers.h"
#include "VeyraPlayerController.h"
#include "VeyraPlayerState.h"
#if WITH_VEYRA_UI
#include "Hud/VeyraHudModel.h"
#endif

namespace VeyraNetTests
{
	// Veyra.Net.MasteryEmote.*: the assignment's Mastery reaches each player's state, and a player's mastery
	// emote shows their Mastery above their Vanguard to every player (ADR-045 §9).
	NETWORK_TEST_CLASS(MasteryEmote, "Veyra.Net")
	{
		struct FState : public FBasePIENetworkComponentState
		{
		};

		FPIENetworkComponent<FState> Network{ TestRunner, TestCommandBuilder, bInitializing };
		TUniquePtr<FScopedMatchTuning> Tuning;
		TUniquePtr<FScopedTestTickets> Tickets;
		TUniquePtr<FScopedMatchAssignment> Assignment;
		FVeyraGreyboxLayout Layout;

		// Fixture values: the first player has played their Vanguard to Mastery Level 4, the emote's second tier.
		static constexpr double ShortPreparationSeconds = 0.1;
		static constexpr double LongEmoteSeconds = 600.0;
		static constexpr double LongCooldownSeconds = 3600.0;
		static constexpr int32 FirstMasteryLevel = 4;
		static constexpr int32 FirstEmoteTier = 2;
		static constexpr int32 EmoterIndex = 0;
		static constexpr int32 WatcherIndex = 1;

		BEFORE_EACH()
		{
			IgnoreKnownIrisWarnings(*TestRunner);
			ASSERT_THAT(IsTrue(VeyraGreybox::LoadLayout(Layout).IsEmpty()));
			Tuning = MakeUnique<FScopedMatchTuning>();
			Tuning->Tuning.Phases.PreparationSeconds = ShortPreparationSeconds;
			Tuning->Tuning.MasteryEmote.Seconds = LongEmoteSeconds;
			Tuning->Tuning.MasteryEmote.CooldownSeconds = LongCooldownSeconds;
			Tickets = MakeUnique<FScopedTestTickets>();
			Assignment = MakeUnique<FScopedMatchAssignment>(TArray<EVeyraTeam>{ EVeyraTeam::A, EVeyraTeam::B });
			Assignment->Assignment.Participants[EmoterIndex].MasteryLevel = FirstMasteryLevel;
			Assignment->Assignment.Participants[EmoterIndex].EmoteTier = FirstEmoteTier;
			Assignment->Problems = UVeyraMatchHostSubsystem::Get()->SetAssignment(Assignment->Assignment);
			ASSERT_THAT(IsTrue(Assignment->Problems.IsEmpty(), FString::Join(Assignment->Problems, TEXT(" | "))));
			BuildMatchNetwork(Network);
		}

		AFTER_EACH()
		{
			Assignment.Reset();
			Tickets.Reset();
			Tuning.Reset();
		}

		static AVeyraPlayerState& ServerParticipant(FState& State, int32 ClientIndex)
		{
			return *ServerControllerOf(State, ClientIndex)->GetPlayerState<AVeyraPlayerState>();
		}

		/** On a client: the participant with Mastery to show, as that client sees them. */
		static const AVeyraPlayerState* SeenEmoter(FState& State)
		{
			const AGameStateBase* GameState = State.World ? State.World->GetGameState() : nullptr;
			for (const APlayerState* Player : GameState ? GameState->PlayerArray : TArray<TObjectPtr<APlayerState>>())
			{
				const AVeyraPlayerState* Participant = Cast<AVeyraPlayerState>(Player);
				if (Participant && Participant->GetMasteryLevel() == FirstMasteryLevel)
				{
					return Participant;
				}
			}
			return nullptr;
		}

		TEST_METHOD(AnEmoteShowsItsPlayersMasteryToEveryPlayer)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("The assignment's Mastery reached each player"), [this](FState& State) {
					ASSERT_THAT(IsTrue(ServerParticipant(State, EmoterIndex).GetMasteryLevel() == FirstMasteryLevel
						&& ServerParticipant(State, EmoterIndex).GetEmoteTier() == FirstEmoteTier));
					ASSERT_THAT(IsTrue(ServerParticipant(State, WatcherIndex).GetMasteryLevel() == 0, TEXT("the other has none")));
					ASSERT_THAT(IsTrue(ServerParticipant(State, EmoterIndex).GetMasteryEmoteUntil() == 0.0, TEXT("no emote yet")));
				})
				.ThenClient(TEXT("The first player plays the emote"), EmoterIndex, [](FState& State) { LocalControllerOf(State.World)->RequestMasteryEmote(); })
				.UntilServer(TEXT("The server shows it for the emote's length"), [](FState& State) {
					const double Now = State.World->GetGameState()->GetServerWorldTimeSeconds();
					return ServerParticipant(State, EmoterIndex).GetMasteryEmoteUntil() > Now + LongEmoteSeconds / 2.0;
				})
				.UntilClient(TEXT("The other player sees it above the first player's Vanguard, with its Mastery and tier"), WatcherIndex, [](FState& State) {
					const AVeyraPlayerState* Emoter = SeenEmoter(State);
					const double Now = State.World->GetGameState()->GetServerWorldTimeSeconds();
					if (!Emoter || !(Emoter->GetMasteryEmoteUntil() > Now) || Emoter->GetEmoteTier() != FirstEmoteTier)
					{
						return false;
					}
#if WITH_VEYRA_UI
					// The HUD draws it while the Vanguard is in view.
					const APawn* Vanguard = Emoter->GetPawn();
					const TOptional<FVeyraHudMasteryEmote> Shown = Vanguard ? VeyraHud::MasteryEmoteOf(*Vanguard, Now) : TOptional<FVeyraHudMasteryEmote>();
					return Shown.IsSet() && Shown->Level == FirstMasteryLevel && Shown->Tier == FirstEmoteTier;
#else
					return true;
#endif
				});
		}
	};
}

#endif
