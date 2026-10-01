// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"

#if ENABLE_PIE_NETWORK_TEST

#include "Brain/VeyraBotBrainComponent.h"
#include "Brain/VeyraBotRoles.h"
#include "EngineUtils.h"
#include "Join/VeyraMatchHostSubsystem.h"
#include "Loadout/VeyraAbilityLoadoutComponent.h"
#include "Tests/Net/VeyraMatchNetTestHelpers.h"
#include "Tests/Net/VeyraNetTestHelpers.h"
#include "Tests/Net/VeyraVanguardNetTestHelpers.h"
#include "Tuning/VeyraBotsTuningSubsystem.h"
#include "Tuning/VeyraTuning.h"
#include "VeyraJoinRules.h"
#include "VeyraPlayerState.h"
#include "VeyraTeamStart.h"
#include "VeyraVanguardController.h"

namespace VeyraNetTests
{
	// Veyra.Net.HostedMatch.*: a server hosting an assigned match admits its roster by join ticket,
	// puts each participant on its rostered side, and reports the match's result when it ends
	// (ADR-007).
	NETWORK_TEST_CLASS(HostedMatch, "Veyra.Net")
	{
		struct FState : public FBasePIENetworkComponentState
		{
		};

		FPIENetworkComponent<FState> Network{ TestRunner, TestCommandBuilder, bInitializing };
		TUniquePtr<FScopedMatchTuning> Tuning;
		TUniquePtr<FScopedTestTickets> Tickets;
		TUniquePtr<FScopedMatchAssignment> Assignment;
		FVeyraGreyboxLayout Layout;
		TOptional<FVeyraMatchResult> Result;
		FDelegateHandle EndedHandle;

		// Fixture values.
		static constexpr double ShortPreparationSeconds = 0.1;
		static constexpr double ShortAbandonSeconds = 0.5;

		BEFORE_EACH()
		{
			IgnoreKnownIrisWarnings(*TestRunner);
			ASSERT_THAT(IsTrue(VeyraGreybox::LoadLayout(Layout).IsEmpty()));
			Tuning = MakeUnique<FScopedMatchTuning>();
			Tuning->Tuning.Phases.PreparationSeconds = ShortPreparationSeconds;
			Tickets = MakeUnique<FScopedTestTickets>();
			// Both on Team A: joining the smaller side would split them, so the roster must decide. Their
			// Vanguards differ from the developer order's, so the roster must decide those too.
			// The first chose two Flux Spells, the second one and an empty slot (ADR-015 §5).
			Assignment = MakeUnique<FScopedMatchAssignment>(TArray<EVeyraTeam>{ EVeyraTeam::A, EVeyraTeam::A }, EVeyraMatchRules::Standard,
				TArray<FVeyraContentId>{ ContentId(TEXT("cairn")), ContentId(TEXT("oriel")) }, TArray<FVeyraAssignedBot>(),
				TArray<TArray<FVeyraContentId>>{ { ContentId(TEXT("blink")), ContentId(TEXT("scorch")) }, { FVeyraContentId(), ContentId(TEXT("mend")) } });
			ASSERT_THAT(IsTrue(Assignment->Problems.IsEmpty(), FString::Join(Assignment->Problems, TEXT(" | "))));
			EndedHandle = UVeyraMatchHostSubsystem::Get()->OnMatchEnded.AddLambda([this](const FVeyraMatchResult& Ended) { Result = Ended; });
			// A client that disconnects returns to the default map in this same process, and starting
			// that standalone world loads MovieSceneCapture, which Iris warns about while the server
			// replicates. Load it before the network starts. A real client leaves from its own process.
			FModuleManager::Get().LoadModule(TEXT("MovieSceneCapture"));
			BuildMatchNetwork(Network);
		}

		AFTER_EACH()
		{
			UVeyraMatchHostSubsystem::Get()->OnMatchEnded.Remove(EndedHandle);
			Assignment.Reset();
			Tickets.Reset();
			Tuning.Reset();
		}

		static FString TunedOptions()
		{
			return TEXT("?") + VeyraJoinRules::MakeTuningHashOption(VeyraTuning::GetCompositeHash());
		}

		static FString PreLoginRefusal(UWorld* World, const FString& Options)
		{
			FString Error;
			GameModeOf(World)->PreLogin(Options, TEXT("test"), FUniqueNetIdRepl(), Error);
			return Error;
		}

		void ExpectRefusals(int32 Count)
		{
			TestRunner->AddExpectedMessagePlain(TEXT("Refused a connection from test"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, Count);
		}

		TEST_METHOD(ParticipantsTakeTheirRosteredSideAndAccount)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Each player is a rostered participant on Team A"), [this](FState& State) {
					TSet<FString> Accounts;
					for (int32 Client = 0; Client < MatchClientCount; ++Client)
					{
						const AVeyraPlayerState* Player = ServerControllerOf(State, Client)->GetPlayerState<AVeyraPlayerState>();
						const FString AccountId = Player->GetAccountId();
						const FVeyraAssignedParticipant* Rostered = Assignment->Assignment.Participants.FindByPredicate(
							[&AccountId](const FVeyraAssignedParticipant& Candidate) { return Candidate.AccountId == AccountId; });
						ASSERT_THAT(IsNotNull(Rostered));
						ASSERT_THAT(IsTrue(Player->GetVeyraTeam() == EVeyraTeam::A));
						ASSERT_THAT(AreEqual(Player->GetPlayerName(), Rostered->DisplayName));
						Accounts.Add(AccountId);
					}
					ASSERT_THAT(AreEqual(Accounts.Num(), MatchClientCount));
				});
		}

		TEST_METHOD(ParticipantsEquipTheFluxSpellsTheyChose)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Each Vanguard spawned with its chosen spells in their slots"), [this](FState& State) {
					for (int32 Client = 0; Client < MatchClientCount; ++Client)
					{
						const AVeyraPlayerState* Player = ServerControllerOf(State, Client)->GetPlayerState<AVeyraPlayerState>();
						const FString AccountId = Player->GetAccountId();
						const FVeyraAssignedParticipant* Rostered = Assignment->Assignment.Participants.FindByPredicate(
							[&AccountId](const FVeyraAssignedParticipant& Candidate) { return Candidate.AccountId == AccountId; });
						ASSERT_THAT(IsNotNull(Rostered));
						const UVeyraAbilityLoadoutComponent* Loadout = Player->FindComponentByClass<UVeyraAbilityLoadoutComponent>();
						for (int32 Index = 0; Index < Rostered->FluxSpells.Num(); ++Index)
						{
							const FVeyraLoadoutEntry* Equipped = Loadout->FindSlot(VeyraAbilitySlots::Spells[Index]);
							const FVeyraContentId& Chosen = Rostered->FluxSpells[Index];
							ASSERT_THAT(IsTrue(Chosen.IsValid() ? Equipped && Equipped->Ability == Chosen : !Equipped,
								FString::Printf(TEXT("%s's spell slot %d"), *Player->GetPlayerName(), Index + 1)));
						}
					}
				});
		}

		TEST_METHOD(ParticipantsPlayTheirRosteredVanguard)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.UntilClients(TEXT("Every client sees each participant's rostered Vanguard"), [this](FState& State) {
					const AVeyraGameState* GameState = GameStateOf(State.World);
					int32 Matched = 0;
					for (const APlayerState* Candidate : GameState->PlayerArray)
					{
						const AVeyraPlayerState* Player = Cast<AVeyraPlayerState>(Candidate);
						const FVeyraAssignedParticipant* Rostered = Player ? Assignment->Assignment.Participants.FindByPredicate(
							[Player](const FVeyraAssignedParticipant& Participant) { return Participant.DisplayName == Player->GetPlayerName(); }) : nullptr;
						Matched += Rostered && Player->GetVanguardId() == Rostered->VanguardId ? 1 : 0;
					}
					return Matched == MatchClientCount;
				});
		}

		TEST_METHOD(AStandardMatchCannotBeEndedByAHost)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenClient(TEXT("A client asks to end the custom match"), 0, [](FState& State) { LocalControllerOf(State.World)->RequestEndCustomMatch(); })
				.UntilClient(TEXT("The server refuses: this is not a custom match"), 0, [](FState& State) {
					return LocalControllerOf(State.World)->GetLastEndCustomMatchRefusal() == EVeyraEndCustomMatchRefusal::NotCustomMatch;
				})
				.ThenServer(TEXT("The match goes on"), [this](FState& State) {
					ASSERT_THAT(IsTrue(GameStateOf(State.World)->GetPhase() == EVeyraMatchPhase::Live));
					ASSERT_THAT(IsFalse(Result.IsSet()));
				});
		}

		TEST_METHOD(RefusesMissingUnknownAndConnectedTickets)
		{
			ExpectRefusals(3);
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Refuse each login"), [this](FState& State) {
					ASSERT_THAT(IsTrue(PreLoginRefusal(State.World, TunedOptions()).Contains(TEXT("sent none"))));
					ASSERT_THAT(IsTrue(PreLoginRefusal(State.World, TunedOptions() + TEXT("?") + VeyraJoinRules::MakeTicketOption(TEXT("vjt_unknown")))
						.Contains(TEXT("not valid for this match"))));
					ASSERT_THAT(IsTrue(PreLoginRefusal(State.World, TunedOptions() + TEXT("?") + VeyraJoinRules::MakeTicketOption(TestTicketForPIEInstance(1)))
						.Contains(TEXT("already connected"))));
				});
		}

		TEST_METHOD(ADeveloperEndReportsTheResult)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenClient(TEXT("A client asks to end the match"), 0, [](FState& State) { LocalControllerOf(State.World)->RequestDeveloperEndMatch(); })
				.UntilServer(TEXT("The match ends"), [this](FState& State) {
					return Result.IsSet() && GameStateOf(State.World)->GetPhase() == EVeyraMatchPhase::Ended;
				})
				.UntilClients(TEXT("Every client sees the end"), [](FState& State) { return GameStateOf(State.World)->GetPhase() == EVeyraMatchPhase::Ended; })
				.ThenServer(TEXT("The result names every participant, and the match takes no more orders"), [this](FState& State) {
					ASSERT_THAT(IsTrue(Result->EndReason == EVeyraMatchEndReason::DeveloperRequest));
					ASSERT_THAT(AreEqual(Result->MatchId, Assignment->Assignment.MatchId));
					ASSERT_THAT(IsTrue(Result->Winner == EVeyraTeam::None));
					ASSERT_THAT(IsTrue(Result->DurationSeconds > 0.0));
					ASSERT_THAT(IsTrue(Result->DurationSeconds == GameStateOf(State.World)->GetMatchClockSeconds()));
					ASSERT_THAT(AreEqual(Result->Participants.Num(), MatchClientCount));
					for (const FVeyraParticipantResult& Participant : Result->Participants)
					{
						ASSERT_THAT(IsTrue(Participant.bJoined && Participant.bConnectedAtEnd));
					}
					// The scoreboard: each rostered player, with the side and Vanguard the roster gave it,
					// recorded from its starting Gold (ADR-017 §5).
					ASSERT_THAT(AreEqual(Result->Players.Num(), MatchClientCount));
					for (const FVeyraAssignedParticipant& Rostered : Assignment->Assignment.Participants)
					{
						const FVeyraPlayerResult* Player = Result->Players.FindByPredicate([&Rostered](const FVeyraPlayerResult& Line) { return Line.AccountId == Rostered.AccountId; });
						ASSERT_THAT(IsTrue(Player && Player->Side == Rostered.Side && Player->VanguardId == Rostered.VanguardId && Player->DisplayName == Rostered.DisplayName));
						ASSERT_THAT(IsTrue(Player->Statistics.Level >= 1 && Player->Statistics.GoldBySource.Starting > 0.0 && Player->Statistics.FluxSpells.Num() == 2));
					}
					ASSERT_THAT(IsTrue(GameModeOf(State.World)->CheckOrdersAllowed() == EVeyraOrderRejection::WrongPhase));
				});
		}

		TEST_METHOD(AMatchEveryoneLeftEndsAsAbandoned)
		{
			ExpectRefusals(1);
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				// Only now, with everyone in: abandonment counts real time, and a slow start could otherwise
				// abandon the match before its players connect.
				.ThenServer(TEXT("Shorten the wait for abandonment"), [this](FState& /*State*/) { Tuning->Tuning.Lifecycle.AbandonAfterSeconds = ShortAbandonSeconds; })
				.ThenClients(TEXT("Every client leaves"), [](FState& State) { LocalControllerOf(State.World)->ConsoleCommand(TEXT("disconnect")); })
				.UntilServer(TEXT("The match ends as abandoned"), [this](FState& /*State*/) { return Result.IsSet(); })
				.ThenServer(TEXT("Nobody was connected at the end, and nobody may join an ended match"), [this](FState& State) {
					ASSERT_THAT(IsTrue(Result->EndReason == EVeyraMatchEndReason::Abandoned));
					for (const FVeyraParticipantResult& Participant : Result->Participants)
					{
						ASSERT_THAT(IsTrue(Participant.bJoined && !Participant.bConnectedAtEnd));
					}
					const FString Rejoin = TunedOptions() + TEXT("?") + VeyraJoinRules::MakeTicketOption(TestTicketForPIEInstance(1));
					ASSERT_THAT(IsTrue(PreLoginRefusal(State.World, Rejoin).Contains(TEXT("has ended"))));
				});
		}
	};

	// Veyra.Net.HostedPractice.*: a practice match's host, and only its host, ends it; it ends
	// host-ended with no winner, and every client sees the phase change (ADR-010 §3, §7). Its bots join
	// their sides as their Vanguards, each with a brain for its seat's lane and its difficulty. The second client stands in for anyone
	// who is not the host.
	NETWORK_TEST_CLASS(HostedPractice, "Veyra.Net")
	{
		struct FState : public FBasePIENetworkComponentState
		{
		};

		FPIENetworkComponent<FState> Network{ TestRunner, TestCommandBuilder, bInitializing };
		TUniquePtr<FScopedMatchTuning> Tuning;
		TUniquePtr<FScopedTestTickets> Tickets;
		TUniquePtr<FScopedMatchAssignment> Assignment;
		FVeyraGreyboxLayout Layout;
		TOptional<FVeyraMatchResult> Result;
		FDelegateHandle EndedHandle;
		/** The last phase each client's GameState announced. */
		TMap<const UWorld*, EVeyraMatchPhase> AnnouncedPhase;
		/** The practice match's bots, both on the side that is not the host's. */
		TArray<FVeyraAssignedBot> PracticeBots;

		// Fixture values.
		static constexpr double ShortPreparationSeconds = 0.1;

		BEFORE_EACH()
		{
			IgnoreKnownIrisWarnings(*TestRunner);
			ASSERT_THAT(IsTrue(VeyraGreybox::LoadLayout(Layout).IsEmpty()));
			Tuning = MakeUnique<FScopedMatchTuning>();
			Tuning->Tuning.Phases.PreparationSeconds = ShortPreparationSeconds;
			Tickets = MakeUnique<FScopedTestTickets>();
			// Seated before the match goes live, the side deals its places by the roles of their Vanguards: Eudora,
			// seated second, takes the first seat's Mid, which she plays before Top, and Varkesh her Top (ADR-038 §5).
			PracticeBots = { { EVeyraTeam::B, ContentId(TEXT("varkesh")), EVeyraBotDifficulty::Beginner },
				{ EVeyraTeam::B, ContentId(TEXT("eudora")), EVeyraBotDifficulty::Intermediate } };
			Assignment = MakeUnique<FScopedMatchAssignment>(TArray<EVeyraTeam>{ EVeyraTeam::A, EVeyraTeam::B }, EVeyraMatchRules::Practice,
				TArray<FVeyraContentId>{}, PracticeBots);
			ASSERT_THAT(IsTrue(Assignment->Problems.IsEmpty(), FString::Join(Assignment->Problems, TEXT(" | "))));
			EndedHandle = UVeyraMatchHostSubsystem::Get()->OnMatchEnded.AddLambda([this](const FVeyraMatchResult& Ended) { Result = Ended; });
			BuildMatchNetwork(Network);
		}

		AFTER_EACH()
		{
			UVeyraMatchHostSubsystem::Get()->OnMatchEnded.Remove(EndedHandle);
			Assignment.Reset();
			Tickets.Reset();
			Tuning.Reset();
		}

		TEST_METHOD(OnlyTheHostEndsItAndEveryoneSeesTheEnd)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.UntilClients(TEXT("Every client sees practice rules and the host"), [](FState& State) {
					const AVeyraGameState* GameState = GameStateOf(State.World);
					return GameState->GetMatchRules() == EVeyraMatchRules::Practice && GameState->GetHost() != nullptr;
				})
				.ThenClients(TEXT("Listen for phase changes"), [this](FState& State) {
					const UWorld* World = State.World;
					GameStateOf(World)->OnPhaseChanged.AddLambda([this, World](EVeyraMatchPhase Phase) { AnnouncedPhase.Add(World, Phase); });
				})
				.ThenClient(TEXT("Someone who is not the host asks to end it"), 1, [](FState& State) { LocalControllerOf(State.World)->RequestEndCustomMatch(); })
				.UntilClient(TEXT("The server refuses them"), 1, [](FState& State) {
					return LocalControllerOf(State.World)->GetLastEndCustomMatchRefusal() == EVeyraEndCustomMatchRefusal::NotHost;
				})
				.ThenServer(TEXT("The match goes on"), [this](FState& State) {
					ASSERT_THAT(IsTrue(GameStateOf(State.World)->GetPhase() == EVeyraMatchPhase::Live && !Result.IsSet()));
				})
				.ThenClient(TEXT("The host ends it"), 0, [](FState& State) { LocalControllerOf(State.World)->RequestEndCustomMatch(); })
				.UntilServer(TEXT("The match ends"), [this](FState& /*State*/) { return Result.IsSet(); })
				.UntilClients(TEXT("Every client's GameState announces the end"), [this](FState& State) {
					return AnnouncedPhase.FindRef(State.World) == EVeyraMatchPhase::Ended;
				})
				.ThenServer(TEXT("It ended host-ended, with no winner, and its result names the players, not the bots"), [this](FState& /*State*/) {
					ASSERT_THAT(IsTrue(Result->EndReason == EVeyraMatchEndReason::HostEnded));
					ASSERT_THAT(IsTrue(Result->Winner == EVeyraTeam::None));
					ASSERT_THAT(AreEqual(Result->Participants.Num(), MatchClientCount));
					// Its scoreboard has the bots too, with no account, side A first (ADR-017 §5).
					ASSERT_THAT(AreEqual(Result->Players.Num(), MatchClientCount + PracticeBots.Num()));
					ASSERT_THAT(IsTrue(Result->Players[0].Side == EVeyraTeam::A));
					for (const FVeyraAssignedBot& Bot : PracticeBots)
					{
						ASSERT_THAT(IsTrue(Result->Players.ContainsByPredicate([&Bot](const FVeyraPlayerResult& Line) {
							return Line.AccountId.IsEmpty() && Line.Side == Bot.Side && Line.VanguardId == Bot.VanguardId;
						})));
					}
				});
		}

		/** The server's bots, in the order the match added them. */
		static TArray<const AVeyraPlayerState*> BotsOf(const UWorld* World)
		{
			TArray<const AVeyraPlayerState*> Bots;
			for (const APlayerState* Member : GameStateOf(World)->PlayerArray)
			{
				if (const AVeyraPlayerState* Bot = Cast<AVeyraPlayerState>(Member); Bot && Bot->IsABot())
				{
					Bots.Add(Bot);
				}
			}
			return Bots;
		}

		TEST_METHOD(ItsBotsJoinTheirSideAsTheirVanguardsWithTheirBrains)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.UntilServer(TEXT("Each bot's Vanguard is on the map"), [this](FState& State) {
					const TArray<const AVeyraPlayerState*> Bots = BotsOf(State.World);
					return Bots.Num() == PracticeBots.Num() && !Bots.ContainsByPredicate([](const AVeyraPlayerState* Bot) { return !Bot->GetPawn(); });
				})
				.ThenServer(TEXT("They play their sides and Vanguards, each with a brain for the place its side dealt it"), [this](FState& State) {
					const TArray<const AVeyraPlayerState*> Bots = BotsOf(State.World);
					const FVeyraBotsTuning& BotTuning = UVeyraBotsTuningSubsystem::Get();
					const TArray<FVeyraBotSeatTuning>& Seats = BotTuning.Seats;
					// Both bots sit on one side, in seats 0 and 1: the side deals those places by the roles their Vanguards play (ADR-038 §5).
					TArray<EVeyraBotRole> Places;
					TArray<TArray<EVeyraBotRole>> Preferences;
					for (int32 Index = 0; Index < PracticeBots.Num(); ++Index)
					{
						Places.Add(Seats[Index % Seats.Num()].Role);
						Preferences.Add(BotTuning.Vanguards.FindChecked(PracticeBots[Index].VanguardId).Roles);
					}
					const TArray<int32> PlaceOf = VeyraBotRoles::Deal(Places, Preferences);
					ASSERT_THAT(IsTrue(PlaceOf.Num() == 2 && PlaceOf[0] == 1 && PlaceOf[1] == 0, TEXT("the side swaps their seats' places")));
					for (int32 Index = 0; Index < Bots.Num(); ++Index)
					{
						ASSERT_THAT(IsTrue(Bots[Index]->GetVeyraTeam() == PracticeBots[Index].Side));
						ASSERT_THAT(IsTrue(Bots[Index]->GetVanguardId() == PracticeBots[Index].VanguardId));
						const UVeyraBotBrainComponent* Brain = Bots[Index]->GetVanguardController()->FindComponentByClass<UVeyraBotBrainComponent>();
						ASSERT_THAT(IsNotNull(Brain));
						const FVeyraBotSeatTuning& Seat = Seats[PlaceOf[Index] % Seats.Num()];
						ASSERT_THAT(IsTrue(Brain->GetDifficulty() == PracticeBots[Index].Difficulty && Brain->GetRole() == Seat.Role));
						// Its seat's Flux Spells, equipped though it spawned before it was seated (ADR-015 §8).
						const UVeyraAbilityLoadoutComponent* Loadout = Bots[Index]->FindComponentByClass<UVeyraAbilityLoadoutComponent>();
						for (int32 Spell = 0; Spell < Seat.FluxSpells.Num(); ++Spell)
						{
							const FVeyraLoadoutEntry* Equipped = Loadout->FindSlot(VeyraAbilitySlots::Spells[Spell]);
							ASSERT_THAT(IsTrue(Equipped && Equipped->Ability == Seat.FluxSpells[Spell], *Bots[Index]->GetPlayerName()));
						}
					}
				})
				.UntilClients(TEXT("Every client sees the bots' Vanguards"), [this](FState& State) {
					int32 Seen = 0;
					for (const APlayerState* Member : GameStateOf(State.World)->PlayerArray)
					{
						Seen += Member->IsABot() && Member->GetPawn() ? 1 : 0;
					}
					return Seen == PracticeBots.Num();
				});
		}
	};
}

#endif // ENABLE_PIE_NETWORK_TEST
