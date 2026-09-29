// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"

#if ENABLE_PIE_NETWORK_TEST

#include "Absence/VeyraAbsenceSubsystem.h"
#include "EngineUtils.h"
#include "Tests/Net/VeyraMatchNetTestHelpers.h"
#include "Tests/Net/VeyraNetTestHelpers.h"
#include "VeyraPlayerState.h"
#include "VeyraTeamStart.h"
#include "VeyraVanguardController.h"

namespace VeyraNetTests
{
	// Veyra.Net.Autopilot.*: an absent participant's Vanguard is walked to safety and only moved; an
	// idle player counts as absent, and its next order takes control back (Match Flow Bible §4–§5;
	// ADR-019 §2–§3). The grey box has no towers, so the autopilot walks home at once.
	NETWORK_TEST_CLASS(Autopilot, "Veyra.Net")
	{
		struct FState : public FBasePIENetworkComponentState
		{
		};

		FPIENetworkComponent<FState> Network{ TestRunner, TestCommandBuilder, bInitializing };
		TUniquePtr<FScopedMatchTuning> Tuning;
		TUniquePtr<FScopedTestTickets> Tickets;
		TUniquePtr<FScopedMatchAssignment> Assignment;
		FVeyraGreyboxLayout Layout;
		TWeakObjectPtr<AVeyraPlayerState> Watched;
		double StartingDistance = 0.0;

		// Fixture values: short enough for a test, in match seconds.
		static constexpr double ShortPreparationSeconds = 0.1;
		static constexpr double ShortAfkSeconds = 0.5;
		static constexpr double LongAfkSeconds = 600.0;
		// The client in this PIE instance is the absent one.
		static constexpr int32 AbsentInstance = 1;
		// How far from home the absent Vanguard is placed before it goes absent.
		static constexpr double AwayFromHome = 1500.0;

		BEFORE_EACH()
		{
			IgnoreKnownIrisWarnings(*TestRunner);
			ASSERT_THAT(IsTrue(VeyraGreybox::LoadLayout(Layout).IsEmpty()));
			Tuning = MakeUnique<FScopedMatchTuning>();
			Tuning->Tuning.Phases.PreparationSeconds = ShortPreparationSeconds;
			Tickets = MakeUnique<FScopedTestTickets>();
			Assignment = MakeUnique<FScopedMatchAssignment>(TArray<EVeyraTeam>{ EVeyraTeam::A, EVeyraTeam::B });
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

		static UWorld* CurrentWorldOf(int32 PIEInstance)
		{
			const FWorldContext* Context = GEngine->GetWorldContextFromPIEInstance(PIEInstance);
			return Context ? Context->World() : nullptr;
		}

		static AVeyraPlayerState* FindServerParticipant(const UWorld* World, const FString& AccountId)
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

		static FVector HomeOf(UWorld* World, EVeyraTeam Team)
		{
			for (TActorIterator<AVeyraTeamStart> It(World); It; ++It)
			{
				if (It->GetVeyraTeam() == Team)
				{
					return It->GetActorLocation();
				}
			}
			return FVector::ZeroVector;
		}

		double DistanceHome(UWorld* World) const
		{
			const APawn* Vanguard = Watched.IsValid() ? Watched->GetPawn() : nullptr;
			return Vanguard ? FVector::Dist2D(Vanguard->GetActorLocation(), HomeOf(World, Watched->GetVeyraTeam())) : TNumericLimits<double>::Max();
		}

		/** Places the watched participant's Vanguard away from home, toward the map's centre. */
		void SendAway(UWorld* World)
		{
			Watched = FindServerParticipant(World, TestAccountForPIEInstance(AbsentInstance));
			ASSERT_THAT(IsTrue(Watched.IsValid() && Watched->GetPawn() != nullptr));
			APawn* Vanguard = Watched->GetPawn();
			const FVector Home = HomeOf(World, Watched->GetVeyraTeam());
			const FVector Toward = (FVector::ZeroVector - Home).GetSafeNormal2D();
			Vanguard->SetActorLocation(Home + Toward * AwayFromHome + FVector(0.0, 0.0, Vanguard->GetActorLocation().Z - Home.Z));
			StartingDistance = DistanceHome(World);
		}

		TEST_METHOD(ADisconnectedVanguardIsWalkedHomeAndOnlyMoved)
		{
			Tuning->Tuning.Absence.AfkAfterSeconds = LongAfkSeconds;
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Its Vanguard stands away from home"), [this](FState& State) { SendAway(State.World); })
				.ThenServer(TEXT("Its player leaves"), [](FState& /*State*/) { GEngine->Exec(CurrentWorldOf(AbsentInstance), TEXT("disconnect")); })
				.UntilServer(TEXT("The server counts it disconnected"), [this](FState& State) {
					const FVeyraAbsenceRecord* Record = Watched.IsValid() ? State.World->GetSubsystem<UVeyraAbsenceSubsystem>()->Find(*Watched) : nullptr;
					return Record && Record->Absence == EVeyraAbsence::Disconnected;
				})
				.UntilServer(TEXT("With no tower standing, the autopilot walks it home"), [this](FState& State) {
					const AVeyraVanguardController* Controller = Watched.IsValid() ? Watched->GetVanguardController() : nullptr;
					return Controller && DistanceHome(State.World) < StartingDistance - AwayFromHome / 2.0 && Controller->GetAttackTarget() == nullptr;
				});
		}

		TEST_METHOD(AnIdlePlayerIsWalkedToSafetyAndItsNextOrderTakesControlBack)
		{
			Tuning->Tuning.Absence.AfkAfterSeconds = ShortAfkSeconds;
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Its Vanguard stands away from home"), [this](FState& State) { SendAway(State.World); })
				.UntilServer(TEXT("Doing nothing, it becomes AFK and walks home"), [this](FState& State) {
					const FVeyraAbsenceRecord* Record = Watched.IsValid() ? State.World->GetSubsystem<UVeyraAbsenceSubsystem>()->Find(*Watched) : nullptr;
					const AVeyraVanguardController* Controller = Watched->GetVanguardController();
					return Record && Record->Absence == EVeyraAbsence::Afk && Controller && Controller->GetMoveOrder().IsSet();
				})
				.ThenClients(TEXT("Each player orders a move"), [](FState& State) {
					LocalControllerOf(State.World)->IssueMoveOrder(FVector::ZeroVector);
				})
				.UntilServer(TEXT("It is present again"), [this](FState& State) {
					const FVeyraAbsenceRecord* Record = State.World->GetSubsystem<UVeyraAbsenceSubsystem>()->Find(*Watched);
					return Record && Record->Absence == EVeyraAbsence::Present && Record->ReturnedAt.IsSet();
				});
		}
	};
}

#endif // ENABLE_PIE_NETWORK_TEST
