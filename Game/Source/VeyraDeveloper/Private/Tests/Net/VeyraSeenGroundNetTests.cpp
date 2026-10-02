// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"

#if ENABLE_PIE_NETWORK_TEST

#include "State/VeyraVisionTeamState.h"
#include "Tests/Net/VeyraMatchNetTestHelpers.h"
#include "Tests/Net/VeyraNetTestHelpers.h"
#include "VeyraPlayerController.h"
#include "VeyraPlayerState.h"
#include "VeyraVanguardCharacter.h"
#include "VeyraVisionSubsystem.h"

namespace VeyraNetTests
{
	// Veyra.Net.SeenGround.*: each side's seen ground reaches that side's clients alone, and follows its units
	// (ADR-054 §2). One player a side.
	NETWORK_TEST_CLASS(SeenGround, "Veyra.Net")
	{
		struct FState : public FBasePIENetworkComponentState
		{
		};

		FPIENetworkComponent<FState> Network{ TestRunner, TestCommandBuilder, bInitializing };
		TUniquePtr<FScopedExpectedPlayers> ExpectedPlayers;
		TUniquePtr<FScopedMatchTuning> Tuning;
		FVeyraGreyboxLayout Layout;

		static constexpr double ShortPreparationSeconds = 0.1;
		// Fixture value: how far the first player's Vanguard is sent, as a fraction of the floor's half extent.
		static constexpr double SentFraction = 0.6;

		BEFORE_EACH()
		{
			IgnoreKnownIrisWarnings(*TestRunner);
			ASSERT_THAT(IsTrue(VeyraGreybox::LoadLayout(Layout).IsEmpty()));
			Tuning = MakeUnique<FScopedMatchTuning>();
			Tuning->Tuning.Phases.PreparationSeconds = ShortPreparationSeconds;
			// The committed sight and presentation, not the see-everything fixture.
			Tuning->Vision.Tuning = Tuning->Vision.Committed;
			ExpectedPlayers = MakeUnique<FScopedExpectedPlayers>(MatchClientCount);
			BuildMatchNetwork(Network);
		}

		AFTER_EACH()
		{
			Tuning.Reset();
			ExpectedPlayers.Reset();
		}

		double HalfExtent() const
		{
			return Layout.Floor.LengthX / 2.0;
		}

		/** This client's own side's seen ground, if it has arrived. */
		static const FVeyraSeenGround* OwnGround(const UWorld* World)
		{
			const AVeyraPlayerController* Player = LocalControllerOf(World);
			const AVeyraPlayerState* Participant = Player ? Player->GetPlayerState<AVeyraPlayerState>() : nullptr;
			const AVeyraVisionTeamState* Side = Participant ? AVeyraVisionTeamState::Find(World, Participant->GetVeyraTeam()) : nullptr;
			return Side && Side->GetSeenGround().CellsAcross > 0 ? &Side->GetSeenGround() : nullptr;
		}

		/** Whether this client's side sees the cell holding Point. */
		static bool SeesOwn(const UWorld* World, const FVector2D& Point)
		{
			const FVeyraSeenGround* Ground = OwnGround(World);
			if (!Ground)
			{
				return false;
			}
			return Ground->IsSeen(FMath::FloorToInt32((Point.X - Ground->Min.X) / Ground->CellSize), FMath::FloorToInt32((Point.Y - Ground->Min.Y) / Ground->CellSize));
		}

		TEST_METHOD(EachSideSeesItsOwnGroundAndItFollowsItsUnits)
		{
			const FVector2D Sent(-HalfExtent() * SentFraction, HalfExtent() * SentFraction);
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Publish the seen ground over the floor"), [this](FState& State) {
					State.World->GetSubsystem<UVeyraVisionSubsystem>()->SetSeenGroundArea(FVector2D::ZeroVector, HalfExtent());
				})
				.UntilClients(TEXT("Each side sees the ground under its own Vanguard"), [](FState& State) {
					const APawn* Vanguard = LocalControllerOf(State.World)->GetVanguard();
					return Vanguard && SeesOwn(State.World, FVector2D(Vanguard->GetActorLocation()));
				})
				.ThenClients(TEXT("And has only its own side's"), [this](FState& State) {
					const EVeyraTeam Own = LocalControllerOf(State.World)->GetPlayerState<AVeyraPlayerState>()->GetVeyraTeam();
					const EVeyraTeam Other = Own == EVeyraTeam::A ? EVeyraTeam::B : EVeyraTeam::A;
					const AVeyraVisionTeamState* Theirs = AVeyraVisionTeamState::Find(State.World, Other);
					ASSERT_THAT(IsTrue(!Theirs || Theirs->GetSeenGround().Cells.IsEmpty(), TEXT("the fog gate keeps the other side's to it")));
				})
				.ThenServer(TEXT("Send the first player's Vanguard across the floor"), [Sent](FState& State) {
					APawn* Vanguard = ServerControllerOf(State, 0)->GetPlayerState<AVeyraPlayerState>()->GetPawn();
					Vanguard->SetActorLocation(FVector(Sent.X, Sent.Y, Vanguard->GetActorLocation().Z), /*bSweep*/ false, nullptr, ETeleportType::TeleportPhysics);
				})
				.UntilClient(TEXT("Its side sees the ground it went to"), 0, [Sent](FState& State) { return SeesOwn(State.World, Sent); });
		}
	};
}

#endif // ENABLE_PIE_NETWORK_TEST