// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"

#if ENABLE_PIE_NETWORK_TEST

#include "EngineUtils.h"
#include "Gold/VeyraGoldComponent.h"
#include "Join/VeyraMatchHostSubsystem.h"
#include "Tests/Net/VeyraMatchNetTestHelpers.h"
#include "Tests/Net/VeyraNetTestHelpers.h"
#include "VeyraPlayerState.h"
#include "VeyraVanguardCharacter.h"

namespace VeyraNetTests
{
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

		/** A client's world as it is now: leaving and returning replace the one the network began with. */
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

		template <typename ActorType>
		static int32 CountOf(UWorld* World)
		{
			int32 Count = 0;
			for (TActorIterator<ActorType> It(World); It; ++It)
			{
				Count += IsValid(*It) ? 1 : 0;
			}
			return Count;
		}

		TEST_METHOD(AReturningPlayerTakesBackItsVanguard)
		{
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
}

#endif // ENABLE_PIE_NETWORK_TEST
