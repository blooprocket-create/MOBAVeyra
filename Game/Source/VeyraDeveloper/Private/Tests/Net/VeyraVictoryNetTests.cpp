// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"

#if ENABLE_PIE_NETWORK_TEST

#include "Gold/VeyraGoldComponent.h"
#include "Join/VeyraMatchHostSubsystem.h"
#include "Tests/Net/VeyraBattlegroundNetTestHelpers.h"
#include "Tests/Net/VeyraNetTestHelpers.h"
#include "VeyraPlayerState.h"

namespace VeyraNetTests
{
	namespace VictoryTests
	{
		// Fixture value: a short preparation.
		constexpr double ShortPreparationSeconds = 0.1;

		/**
		 * A battleground match for two clients, the first on team A and hosting a practice match, whose
		 * result it records when it ends.
		 */
		struct FScopedVictoryMatch
		{
			TUniquePtr<FScopedMatchTuning> Tuning;
			TUniquePtr<FScopedTestTickets> Tickets;
			TUniquePtr<FScopedMatchAssignment> Assignment;
			FVeyraGreyboxLayout Greybox;
			TOptional<FVeyraMatchResult> Result;
			FDelegateHandle EndedHandle;

			explicit FScopedVictoryMatch(EVeyraMatchRules Rules, TOptional<FVeyraCustomSettings> Custom = {})
			{
				VeyraGreybox::LoadLayout(Greybox);
				Tuning = MakeUnique<FScopedMatchTuning>();
				Tuning->Tuning.Phases.PreparationSeconds = ShortPreparationSeconds;
				Tickets = MakeUnique<FScopedTestTickets>();
				Assignment = MakeUnique<FScopedMatchAssignment>(TArray<EVeyraTeam>{ EVeyraTeam::A, EVeyraTeam::B }, Rules, TConstArrayView<FVeyraContentId>(),
					TConstArrayView<FVeyraAssignedBot>(), TConstArrayView<TArray<FVeyraContentId>>(), Custom);
				EndedHandle = UVeyraMatchHostSubsystem::Get()->OnMatchEnded.AddLambda([this](const FVeyraMatchResult& Ended) { Result = Ended; });
			}

			~FScopedVictoryMatch()
			{
				UVeyraMatchHostSubsystem::Get()->OnMatchEnded.Remove(EndedHandle);
			}

			UE_NONCOPYABLE(FScopedVictoryMatch);
		};

		/** On the server: the first client, on team A, sieges until nothing more of team B's can fall. Returns how many fell. */
		inline int32 SiegeAll(const FBasePIENetworkComponentState& State)
		{
			const AVeyraPlayerController* Besieger = ServerControllerOf(State, 0);
			const int32 Structures = SeenStructures(State.World).Num();
			int32 Fell = 0;
			while (Besieger && Fell < Structures && GameModeOf(State.World)->HandleDeveloperSiege(*Besieger))
			{
				++Fell;
			}
			return Fell;
		}

		inline const AVeyraStructure* PrimeWellOf(const UWorld* World, EVeyraTeam Team)
		{
			for (const AVeyraStructure* Structure : SeenStructures(World))
			{
				if (Structure->GetVeyraTeam() == Team && Structure->GetStructureKind() == EVeyraStructureKind::PrimeWell)
				{
					return Structure;
				}
			}
			return nullptr;
		}
	}

	// Veyra.Net.Victory.*: destroying a Prime Well wins a standard match, and the result names the
	// winner (Battleground Bible §18; ADR-011 §13). The developer siege does the destroying, through
	// the damage pipeline, so the structures fall in their real order.
	NETWORK_TEST_CLASS(Victory, "Veyra.Net")
	{
		struct FState : public FBasePIENetworkComponentState
		{
		};

		FPIENetworkComponent<FState> Network{ TestRunner, TestCommandBuilder, bInitializing };
		TUniquePtr<VictoryTests::FScopedVictoryMatch> Match;
		int32 Fell = 0;

		BEFORE_EACH()
		{
			IgnoreKnownIrisWarnings(*TestRunner);
			Match = MakeUnique<VictoryTests::FScopedVictoryMatch>(EVeyraMatchRules::Standard);
			ASSERT_THAT(IsTrue(Match->Assignment->Problems.IsEmpty(), FString::Join(Match->Assignment->Problems, TEXT(" | "))));
			BuildMatchNetwork(Network);
		}

		AFTER_EACH()
		{
			Match.Reset();
		}

		TEST_METHOD(DestroyingAPrimeWellWinsAStandardMatch)
		{
			StartBattleground(Network, Match->Greybox, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Team A sieges team B down to its Prime Well"), [this](FState& State) {
					ASSERT_THAT(IsTrue(ServerControllerOf(State, 0)->GetPlayerState<AVeyraPlayerState>()->GetVeyraTeam() == EVeyraTeam::A));
					Fell = VictoryTests::SiegeAll(State);
				})
				.UntilServer(TEXT("The match ends"), [this](FState& /*State*/) { return Match->Result.IsSet(); })
				.UntilClients(TEXT("Every client sees the end, and team B's Prime Well down"), [](FState& State) {
					const AVeyraStructure* Well = VictoryTests::PrimeWellOf(State.World, EVeyraTeam::B);
					return GameStateOf(State.World)->GetPhase() == EVeyraMatchPhase::Ended && Well && Well->IsDestroyed();
				})
				.ThenServer(TEXT("Team A won by the Prime Well, and lost nothing"), [this](FState& State) {
					const FVeyraMatchResult& Result = Match->Result.GetValue();
					ASSERT_THAT(IsTrue(Result.EndReason == EVeyraMatchEndReason::PrimeWellDestroyed));
					ASSERT_THAT(IsTrue(Result.Winner == EVeyraTeam::A));
					ASSERT_THAT(AreEqual(Result.MatchId, Match->Assignment->Assignment.MatchId));
					// The compact battleground's mid lane, both base towers and the Well, in order.
					ASSERT_THAT(AreEqual(Fell, 7));
					for (const AVeyraStructure* Structure : SeenStructures(State.World))
					{
						ASSERT_THAT(IsTrue(Structure->GetVeyraTeam() == EVeyraTeam::B || !Structure->IsDestroyed()));
					}
				});
		}
	};

	// Veyra.Net.NoPracticeVictory.*: practice has no victory condition, so its match goes on after a
	// Prime Well falls, until the host ends it (ADR-011 §14).
	NETWORK_TEST_CLASS(NoPracticeVictory, "Veyra.Net")
	{
		struct FState : public FBasePIENetworkComponentState
		{
		};

		FPIENetworkComponent<FState> Network{ TestRunner, TestCommandBuilder, bInitializing };
		TUniquePtr<VictoryTests::FScopedVictoryMatch> Match;

		BEFORE_EACH()
		{
			IgnoreKnownIrisWarnings(*TestRunner);
			Match = MakeUnique<VictoryTests::FScopedVictoryMatch>(EVeyraMatchRules::Practice);
			ASSERT_THAT(IsTrue(Match->Assignment->Problems.IsEmpty(), FString::Join(Match->Assignment->Problems, TEXT(" | "))));
			BuildMatchNetwork(Network);
		}

		AFTER_EACH()
		{
			Match.Reset();
		}

		TEST_METHOD(PracticeGoesOnAfterItsPrimeWellFalls)
		{
			StartBattleground(Network, Match->Greybox, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("The host sieges the other side down, Prime Well and all"), [](FState& State) {
					VictoryTests::SiegeAll(State);
				})
				.ThenServer(TEXT("The Well is down, and the match goes on with no result"), [this](FState& State) {
					const AVeyraStructure* Well = VictoryTests::PrimeWellOf(State.World, EVeyraTeam::B);
					ASSERT_THAT(IsTrue(Well && Well->IsDestroyed()));
					ASSERT_THAT(IsTrue(GameStateOf(State.World)->GetPhase() == EVeyraMatchPhase::Live && !Match->Result.IsSet()));
				})
				.ThenClient(TEXT("The host ends it"), 0, [](FState& State) { LocalControllerOf(State.World)->RequestEndCustomMatch(); })
				.UntilServer(TEXT("The match ends"), [this](FState& /*State*/) { return Match->Result.IsSet(); })
				.ThenServer(TEXT("It ended host-ended, with no winner"), [this](FState& /*State*/) {
					ASSERT_THAT(IsTrue(Match->Result->EndReason == EVeyraMatchEndReason::HostEnded && Match->Result->Winner == EVeyraTeam::None));
				});
		}
	};

	// Veyra.Net.CustomVictory.*: a custom match whose host left victory on is won as a standard match
	// is, and its players start with the Gold its host set (ADR-021 §3).
	NETWORK_TEST_CLASS(CustomVictory, "Veyra.Net")
	{
		struct FState : public FBasePIENetworkComponentState
		{
		};

		// Fixture value: starting Gold unlike Economy.json's.
		static constexpr double SessionStartingGold = 1234.0;

		FPIENetworkComponent<FState> Network{ TestRunner, TestCommandBuilder, bInitializing };
		TUniquePtr<VictoryTests::FScopedVictoryMatch> Match;

		BEFORE_EACH()
		{
			IgnoreKnownIrisWarnings(*TestRunner);
			FVeyraCustomSettings Settings;
			Settings.bVictoryEnabled = true;
			Settings.StartingGold = SessionStartingGold;
			Match = MakeUnique<VictoryTests::FScopedVictoryMatch>(EVeyraMatchRules::Custom, Settings);
			ASSERT_THAT(IsTrue(Match->Assignment->Problems.IsEmpty(), FString::Join(Match->Assignment->Problems, TEXT(" | "))));
			BuildMatchNetwork(Network);
		}

		AFTER_EACH()
		{
			Match.Reset();
		}

		TEST_METHOD(ACustomMatchWithVictoryOnStartsWithItsGoldAndIsWon)
		{
			StartBattleground(Network, Match->Greybox, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Everyone started with the session's Gold"), [this](FState& State) {
					for (int32 Index = 0; Index < 2; ++Index)
					{
						const UVeyraGoldComponent* Gold = ServerControllerOf(State, Index)->PlayerState->FindComponentByClass<UVeyraGoldComponent>();
						ASSERT_THAT(IsTrue(Gold && Gold->GetGold() >= SessionStartingGold, FString::Printf(TEXT("%.1f"), Gold ? Gold->GetGold() : -1.0)));
					}
				})
				.UntilClients(TEXT("Every client knows the match can be won"), [](FState& State) {
					return GameStateOf(State.World)->GetMatchRules() == EVeyraMatchRules::Custom && GameStateOf(State.World)->HasVictory();
				})
				.ThenServer(TEXT("Team A sieges team B down"), [](FState& State) { VictoryTests::SiegeAll(State); })
				.UntilServer(TEXT("The match ends"), [this](FState& /*State*/) { return Match->Result.IsSet(); })
				.ThenServer(TEXT("Team A won by the Prime Well"), [this](FState& /*State*/) {
					ASSERT_THAT(IsTrue(Match->Result->EndReason == EVeyraMatchEndReason::PrimeWellDestroyed && Match->Result->Winner == EVeyraTeam::A));
				});
		}
	};

	// Veyra.Net.CustomNoVictory.*: with victory off, a custom match goes on after a Prime Well falls,
	// until its host ends it (ADR-021 §3).
	NETWORK_TEST_CLASS(CustomNoVictory, "Veyra.Net")
	{
		struct FState : public FBasePIENetworkComponentState
		{
		};

		FPIENetworkComponent<FState> Network{ TestRunner, TestCommandBuilder, bInitializing };
		TUniquePtr<VictoryTests::FScopedVictoryMatch> Match;

		BEFORE_EACH()
		{
			IgnoreKnownIrisWarnings(*TestRunner);
			Match = MakeUnique<VictoryTests::FScopedVictoryMatch>(EVeyraMatchRules::Custom, FVeyraCustomSettings());
			ASSERT_THAT(IsTrue(Match->Assignment->Problems.IsEmpty(), FString::Join(Match->Assignment->Problems, TEXT(" | "))));
			BuildMatchNetwork(Network);
		}

		AFTER_EACH()
		{
			Match.Reset();
		}

		TEST_METHOD(ACustomMatchWithVictoryOffGoesOnUntilItsHostEndsIt)
		{
			StartBattleground(Network, Match->Greybox, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("The host sieges the other side down, Prime Well and all"), [](FState& State) { VictoryTests::SiegeAll(State); })
				.ThenServer(TEXT("The Well is down, and the match goes on"), [this](FState& State) {
					const AVeyraStructure* Well = VictoryTests::PrimeWellOf(State.World, EVeyraTeam::B);
					ASSERT_THAT(IsTrue(Well && Well->IsDestroyed()));
					ASSERT_THAT(IsTrue(GameStateOf(State.World)->GetPhase() == EVeyraMatchPhase::Live && !Match->Result.IsSet()));
				})
				.ThenClient(TEXT("A player who is not the host cannot end it"), 1, [](FState& State) { LocalControllerOf(State.World)->RequestEndCustomMatch(); })
				.ThenClient(TEXT("The host ends it"), 0, [](FState& State) { LocalControllerOf(State.World)->RequestEndCustomMatch(); })
				.UntilServer(TEXT("The match ends"), [this](FState& /*State*/) { return Match->Result.IsSet(); })
				.ThenServer(TEXT("It ended host-ended, with no winner"), [this](FState& /*State*/) {
					ASSERT_THAT(IsTrue(Match->Result->EndReason == EVeyraMatchEndReason::HostEnded && Match->Result->Winner == EVeyraTeam::None));
				});
		}
	};
}

#endif // ENABLE_PIE_NETWORK_TEST
