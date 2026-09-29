// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"

#if ENABLE_PIE_NETWORK_TEST

#include "Absence/VeyraAbsenceSubsystem.h"
#include "EngineUtils.h"
#include "Gold/VeyraGoldComponent.h"
#include "Join/VeyraMatchHostSubsystem.h"
#include "Tests/Net/VeyraMatchNetTestHelpers.h"
#include "Tests/Net/VeyraNetTestHelpers.h"
#include "VeyraLocalPlayer.h"
#include "VeyraPlayerState.h"
#include "VeyraVanguardCharacter.h"

namespace VeyraNetTests
{
	namespace ReconnectNet
	{
		/** A client's world as it is now: leaving and returning replace the one the network began with. */
		UWorld* CurrentWorldOf(int32 PIEInstance)
		{
			const FWorldContext* Context = GEngine->GetWorldContextFromPIEInstance(PIEInstance);
			return Context ? Context->World() : nullptr;
		}

		AVeyraPlayerState* FindServerParticipant(const UWorld* World, const FString& AccountId)
		{
			for (APlayerState* Member : GameStateOf(World)->PlayerArray)
			{
				AVeyraPlayerState* Candidate = Cast<AVeyraPlayerState>(Member);
				if (Candidate && Candidate->GetAccountId() == AccountId)
				{
					return Candidate;
				}
			}
			return nullptr;
		}

		template <typename ActorType>
		int32 CountOf(UWorld* World)
		{
			int32 Count = 0;
			for (TActorIterator<ActorType> It(World); It; ++It)
			{
				Count += IsValid(*It) ? 1 : 0;
			}
			return Count;
		}
	}

	// Veyra.Net.Reconnect.*: a participant who disconnects from a live match may come back, and takes
	// back the PlayerState it left, with its Vanguard, Gold and record; nothing is spawned twice
	// (Match Flow Bible §3; ADR-019 §1).
	NETWORK_TEST_CLASS(Reconnect, "Veyra.Net")
	{
		struct FState : public FBasePIENetworkComponentState
		{
		};

		FPIENetworkComponent<FState> Network{ TestRunner, TestCommandBuilder, bInitializing };
		TUniquePtr<FScopedMatchTuning> Tuning;
		TUniquePtr<FScopedTestTickets> Tickets;
		TUniquePtr<FScopedMatchAssignment> Assignment;
		FVeyraGreyboxLayout Layout;
		TWeakObjectPtr<AVeyraPlayerState> Left;
		TWeakObjectPtr<APawn> LeftVanguard;

		// Fixture values.
		static constexpr double ShortPreparationSeconds = 0.1;
		// The client in this PIE instance leaves and comes back.
		static constexpr int32 ReturningInstance = 1;

		BEFORE_EACH()
		{
			IgnoreKnownIrisWarnings(*TestRunner);
			ASSERT_THAT(IsTrue(VeyraGreybox::LoadLayout(Layout).IsEmpty()));
			Tuning = MakeUnique<FScopedMatchTuning>();
			Tuning->Tuning.Phases.PreparationSeconds = ShortPreparationSeconds;
			Tickets = MakeUnique<FScopedTestTickets>();
			Assignment = MakeUnique<FScopedMatchAssignment>(TArray<EVeyraTeam>{ EVeyraTeam::A, EVeyraTeam::B });
			ASSERT_THAT(IsTrue(Assignment->Problems.IsEmpty(), FString::Join(Assignment->Problems, TEXT(" | "))));
			// A client that disconnects returns to the default map in this same process, which loads
			// MovieSceneCapture; load it before the network starts (see Veyra.Net.HostedMatch).
			FModuleManager::Get().LoadModule(TEXT("MovieSceneCapture"));
			BuildMatchNetwork(Network);
		}

		AFTER_EACH()
		{
			Assignment.Reset();
			Tickets.Reset();
			Tuning.Reset();
		}

		TEST_METHOD(AReturningPlayerTakesBackItsVanguard)
		{
			using namespace ReconnectNet;
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Note the returning client's participant and Vanguard"), [this](FState& State) {
					Left = FindServerParticipant(State.World, TestAccountForPIEInstance(ReturningInstance));
					ASSERT_THAT(IsTrue(Left.IsValid()));
					LeftVanguard = Left->GetPawn();
					ASSERT_THAT(IsTrue(LeftVanguard.IsValid()));
				})
				.ThenServer(TEXT("It leaves"), [](FState& /*State*/) { GEngine->Exec(CurrentWorldOf(ReturningInstance), TEXT("disconnect")); })
				.UntilServer(TEXT("The server keeps its PlayerState, inactive"), [this](FState& /*State*/) { return Left.IsValid() && Left->IsInactive(); })
				.ThenServer(TEXT("Its Vanguard stays in the match"), [this](FState& State) {
					ASSERT_THAT(IsTrue(LeftVanguard.IsValid() && LeftVanguard->GetPlayerState() == Left.Get()));
					ASSERT_THAT(AreEqual(CountOf<AVeyraVanguardCharacter>(State.World), MatchClientCount));
				})
				.ThenServer(TEXT("It comes back"), [](FState& /*State*/) { GEngine->Exec(CurrentWorldOf(ReturningInstance), TEXT("reconnect")); })
				.UntilServer(TEXT("A new controller holds the PlayerState it left"), [this](FState& /*State*/) {
					const APlayerController* Owner = Left.IsValid() ? Cast<APlayerController>(Left->GetOwner()) : nullptr;
					return IsValid(Owner) && Owner->PlayerState == Left.Get() && !Left->IsInactive();
				})
				.ThenServer(TEXT("Nothing was made twice"), [this](FState& State) {
					ASSERT_THAT(AreEqual(CountOf<AVeyraVanguardCharacter>(State.World), MatchClientCount));
					ASSERT_THAT(AreEqual(GameStateOf(State.World)->PlayerArray.Num(), MatchClientCount));
					ASSERT_THAT(IsTrue(Left->GetPawn() == LeftVanguard.Get(), TEXT("the Vanguard it left")));
				})
				.UntilServer(TEXT("The returning client sees its own Vanguard and Gold"), [this](FState& /*State*/) {
					const AVeyraPlayerController* Local = LocalControllerOf(CurrentWorldOf(ReturningInstance));
					const AVeyraPlayerState* Own = Local ? Local->GetPlayerState<AVeyraPlayerState>() : nullptr;
					const UVeyraGoldComponent* Gold = Own ? Own->FindComponentByClass<UVeyraGoldComponent>() : nullptr;
					const UVeyraGoldComponent* ServerGold = Left.IsValid() ? Left->FindComponentByClass<UVeyraGoldComponent>() : nullptr;
					return Own && Own->GetPawn() && Gold && ServerGold
						&& Gold->GetGold() > 0.0 && FMath::IsNearlyEqual(Gold->GetGold(), ServerGold->GetGold());
				});
		}
	};

	// Veyra.Net.NoShow.*: a rostered player who has not connected when loading times out still has its
	// seat and Vanguard, absent from the start, and takes them when it comes late; a full match still
	// lets it in (Match Flow Bible §3; ADR-019 §1).
	NETWORK_TEST_CLASS(NoShow, "Veyra.Net")
	{
		struct FState : public FBasePIENetworkComponentState
		{
		};

		FPIENetworkComponent<FState> Network{ TestRunner, TestCommandBuilder, bInitializing };
		TUniquePtr<FScopedMatchTuning> Tuning;
		TUniquePtr<FScopedTestTickets> Tickets;
		TUniquePtr<FScopedMatchAssignment> Assignment;
		FVeyraGreyboxLayout Layout;
		TWeakObjectPtr<AVeyraPlayerState> Seat;
		TWeakObjectPtr<APawn> SeatVanguard;

		// Fixture values.
		static constexpr double ShortPreparationSeconds = 0.1;
		static constexpr double ShortLoadingTimeoutSeconds = 0.5;
		// Two a side, so the roster below fills the match.
		static constexpr int32 FullTeamSize = 2;
		// The roster's third and fourth players (on sides A and B) never connect.
		static constexpr int32 FirstNoShowInstance = MatchClientCount + 1;
		// The client in this PIE instance, on side B, leaves and comes back as side B's no-show.
		static constexpr int32 LateInstance = 2;
		static constexpr int32 LateSeatInstance = FirstNoShowInstance + 1;
		// A remake needs both of a side's two votes: the one present's, and the no-show's automatic YES.
		static constexpr int32 WholeTeam = FullTeamSize;
		static constexpr int32 FirstClientIndex = 0;

		BEFORE_EACH()
		{
			IgnoreKnownIrisWarnings(*TestRunner);
			ASSERT_THAT(IsTrue(VeyraGreybox::LoadLayout(Layout).IsEmpty()));
			Tuning = MakeUnique<FScopedMatchTuning>();
			Tuning->Tuning.Phases.PreparationSeconds = ShortPreparationSeconds;
			Tuning->Tuning.Phases.LoadingTimeoutSeconds = ShortLoadingTimeoutSeconds;
			Tuning->Tuning.Teams.MaxTeamSize = FullTeamSize;
			Tickets = MakeUnique<FScopedTestTickets>();
			Assignment = MakeUnique<FScopedMatchAssignment>(TArray<EVeyraTeam>{ EVeyraTeam::A, EVeyraTeam::B, EVeyraTeam::A, EVeyraTeam::B });
			ASSERT_THAT(IsTrue(Assignment->Problems.IsEmpty(), FString::Join(Assignment->Problems, TEXT(" | "))));
			FModuleManager::Get().LoadModule(TEXT("MovieSceneCapture"));
			BuildMatchNetwork(Network);
		}

		AFTER_EACH()
		{
			Assignment.Reset();
			Tickets.Reset();
			Tuning.Reset();
		}

		TEST_METHOD(ANoShowStartsAbsentAndTakesItsVanguardWhenItComesLate)
		{
			using namespace ReconnectNet;
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Each no-show has its seat and Vanguard, disconnected from the start"), [this](FState& State) {
					for (int32 Instance = FirstNoShowInstance; Instance <= LateSeatInstance; ++Instance)
					{
						const FString Account = TestAccountForPIEInstance(Instance);
						const AVeyraPlayerState* Kept = FindServerParticipant(State.World, Account);
						ASSERT_THAT(IsTrue(Kept && Kept->IsInactive() && !Kept->IsABot() && Kept->GetPawn(), *Account));
						ASSERT_THAT(IsTrue(Kept->GetVeyraTeam() == Assignment->Assignment.Participants[Instance - 1].Side, *Account));
						const FVeyraAbsenceRecord* Record = State.World->GetSubsystem<UVeyraAbsenceSubsystem>()->Find(*Kept);
						ASSERT_THAT(IsTrue(Record && Record->Absence == EVeyraAbsence::Disconnected && Record->AwaySince.IsSet(), *Account));
					}
					ASSERT_THAT(AreEqual(CountOf<AVeyraVanguardCharacter>(State.World), Assignment->Assignment.Participants.Num()));
					Seat = FindServerParticipant(State.World, TestAccountForPIEInstance(LateSeatInstance));
					SeatVanguard = Seat->GetPawn();
				})
				.ThenServer(TEXT("A client leaves"), [](FState& /*State*/) { GEngine->Exec(CurrentWorldOf(LateInstance), TEXT("disconnect")); })
				.UntilServer(TEXT("The server keeps its PlayerState"), [](FState& State) {
					const AVeyraPlayerState* Left = FindServerParticipant(State.World, TestAccountForPIEInstance(LateInstance));
					return Left && Left->IsInactive();
				})
				.ThenServer(TEXT("It comes back with the no-show's ticket, to a full match"), [](FState& /*State*/) {
					UVeyraLocalPlayer::SetTestTicketProvider([](const ULocalPlayer& /*Player*/) { return TestTicketForPIEInstance(LateSeatInstance); });
					GEngine->Exec(CurrentWorldOf(LateInstance), TEXT("reconnect"));
				})
				.UntilServer(TEXT("Its first connection takes the seat kept for it"), [this](FState& /*State*/) {
					const APlayerController* Owner = Seat.IsValid() ? Cast<APlayerController>(Seat->GetOwner()) : nullptr;
					return IsValid(Owner) && Owner->PlayerState == Seat.Get() && !Seat->IsInactive();
				})
				.ThenServer(TEXT("With the Vanguard it had and its absence closed; nothing was made twice"), [this](FState& State) {
					ASSERT_THAT(IsTrue(Seat->GetPawn() == SeatVanguard.Get()));
					ASSERT_THAT(AreEqual(CountOf<AVeyraVanguardCharacter>(State.World), Assignment->Assignment.Participants.Num()));
					ASSERT_THAT(AreEqual(GameStateOf(State.World)->PlayerArray.Num(), Assignment->Assignment.Participants.Num()));
					const FVeyraAbsenceRecord* Record = State.World->GetSubsystem<UVeyraAbsenceSubsystem>()->Find(*Seat);
					ASSERT_THAT(IsTrue(Record && Record->Absence == EVeyraAbsence::Present && Record->ClosedAbsentSeconds > 0.0,
						TEXT("its absence counted from the start")));
				});
		}

		TEST_METHOD(ANoShowsAutomaticYesCountsTowardItsTeamsRemake)
		{
			Tuning->Tuning.Votes.Remake.YesVotes = WholeTeam;
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenClient(TEXT("The one player of its side who came asks for a remake"), FirstClientIndex, [](FState& State) {
					LocalControllerOf(State.World)->RequestVote(EVeyraVoteKind::Remake);
				})
				.UntilServer(TEXT("With the no-show's YES, the match ends as a remake"), [](FState& State) {
					return GameStateOf(State.World)->GetPhase() == EVeyraMatchPhase::Ended;
				});
		}
	};
}

#endif // ENABLE_PIE_NETWORK_TEST
