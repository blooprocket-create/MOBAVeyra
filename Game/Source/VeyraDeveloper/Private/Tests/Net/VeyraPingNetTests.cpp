// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"

#if ENABLE_PIE_NETWORK_TEST

#include "HAL/PlatformTime.h"
#include "Join/VeyraMatchHostSubsystem.h"
#include "Tests/Net/VeyraMatchNetTestHelpers.h"
#include "Tests/Net/VeyraNetTestHelpers.h"
#include "VeyraPlayerState.h"

namespace VeyraNetTests
{
	// Veyra.Net.TeamPings.*: a ping reaches the sender's side and no one else, and a spammer is held
	// back (ADR-020 §2). Two players on one side and one on the other.
	NETWORK_TEST_CLASS(TeamPings, "Veyra.Net")
	{
		struct FState : public FBasePIENetworkComponentState
		{
		};

		FPIENetworkComponent<FState> Network{ TestRunner, TestCommandBuilder, bInitializing };
		TUniquePtr<FScopedMatchTuning> Tuning;
		TUniquePtr<FScopedTestTickets> Tickets;
		TUniquePtr<FScopedMatchAssignment> Assignment;
		FVeyraGreyboxLayout Layout;
		double SentAt = 0.0;

		// Fixture values.
		static constexpr double ShortPreparationSeconds = 0.1;
		static constexpr int32 TwoPings = 2;
		static constexpr double LongWindowSeconds = 600.0;
		// Time enough for a ping to have reached every client it may.
		static constexpr double DeliveryGraceSeconds = 1.0;
		// The first two clients are teammates; the third plays against them.
		static constexpr int32 ClientCount = 3;
		static constexpr int32 SenderIndex = 0;
		static constexpr int32 TeammateIndex = 1;
		static constexpr int32 OpponentIndex = 2;

		BEFORE_EACH()
		{
			IgnoreKnownIrisWarnings(*TestRunner);
			ASSERT_THAT(IsTrue(VeyraGreybox::LoadLayout(Layout).IsEmpty()));
			Tuning = MakeUnique<FScopedMatchTuning>();
			Tuning->Tuning.Phases.PreparationSeconds = ShortPreparationSeconds;
			Tuning->Tuning.Pings.MaxPerWindow = TwoPings;
			Tuning->Tuning.Pings.WindowSeconds = LongWindowSeconds;
			Tickets = MakeUnique<FScopedTestTickets>();
			Assignment = MakeUnique<FScopedMatchAssignment>(TArray<EVeyraTeam>{ EVeyraTeam::A, EVeyraTeam::A, EVeyraTeam::B });
			ASSERT_THAT(IsTrue(Assignment->Problems.IsEmpty(), FString::Join(Assignment->Problems, TEXT(" | "))));
			BuildMatchNetwork(Network, ClientCount);
		}

		AFTER_EACH()
		{
			Assignment.Reset();
			Tickets.Reset();
			Tuning.Reset();
		}

		TEST_METHOD(APingReachesOnlyItsSideAndASpammerIsHeldBack)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenClient(TEXT("A player pings danger"), SenderIndex, [](FState& State) {
					LocalControllerOf(State.World)->RequestPing(FVector(100.0, 200.0, 0.0), EVeyraPingKind::Danger);
				})
				.UntilClients(TEXT("Its side receives it, sender and all"), [](FState& State) {
					const AVeyraPlayerController* Local = LocalControllerOf(State.World);
					const AVeyraPlayerState* Own = Local ? Local->GetPlayerState<AVeyraPlayerState>() : nullptr;
					if (!Own || Own->GetVeyraTeam() != EVeyraTeam::A)
					{
						return true;
					}
					return Local->GetPings().Num() == 1 && Local->GetPings()[0].Ping.Kind == EVeyraPingKind::Danger
						&& Local->GetPings()[0].Ping.Point.Equals(FVector(100.0, 200.0, 0.0));
				})
				.ThenServer(TEXT("Note the time"), [this](FState& /*State*/) { SentAt = FPlatformTime::Seconds(); })
				.UntilServer(TEXT("Give it time to reach everyone it may"), [this](FState& /*State*/) { return FPlatformTime::Seconds() - SentAt >= DeliveryGraceSeconds; })
				.ThenClient(TEXT("Nothing reached the other side"), OpponentIndex, [this](FState& State) {
					ASSERT_THAT(IsTrue(LocalControllerOf(State.World)->GetPings().IsEmpty()));
				})
				.ThenClient(TEXT("The teammate pings three times at once"), TeammateIndex, [](FState& State) {
					LocalControllerOf(State.World)->RequestPing(FVector::ZeroVector, EVeyraPingKind::Look);
					LocalControllerOf(State.World)->RequestPing(FVector::ZeroVector, EVeyraPingKind::Look);
					LocalControllerOf(State.World)->RequestPing(FVector::ZeroVector, EVeyraPingKind::Look);
				})
				.UntilClient(TEXT("Its third inside the window is refused"), TeammateIndex, [](FState& State) {
					return LocalControllerOf(State.World)->GetLastPingRefusal() == EVeyraPingRefusal::TooMany;
				})
				.UntilClient(TEXT("And the sender has only the two that counted, besides its own"), SenderIndex, [](FState& State) {
					return LocalControllerOf(State.World)->GetPings().Num() == 1 + TwoPings;
				});
		}
	};
}

#endif // ENABLE_PIE_NETWORK_TEST
