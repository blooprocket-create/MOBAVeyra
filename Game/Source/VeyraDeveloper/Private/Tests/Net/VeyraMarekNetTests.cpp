// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"

#if ENABLE_PIE_NETWORK_TEST

#include "Companions/VeyraCompanion.h"
#include "Companions/VeyraCompanionSubsystem.h"
#include "Targeting/VeyraTargeting.h"
#include "Tests/Net/VeyraMatchNetTestHelpers.h"
#include "Tests/Net/VeyraNetTestHelpers.h"
#include "Tests/Net/VeyraVanguardNetTestHelpers.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"

namespace VeyraNetTests
{
	// Veyra.Net.Vanguards.Marek.*: Marek and Nix in a match (Roster Bible §10; ADR-034 §3, §4). Accord
	// summons Nix before Marek's body spawns; Nix forms beside that body and follows it without ever
	// standing in its way. Both players are Marek.
	NETWORK_TEST_CLASS(Marek, "Veyra.Net.Vanguards")
	{
		struct FState : public FBasePIENetworkComponentState
		{
		};

		FPIENetworkComponent<FState> Network{ TestRunner, TestCommandBuilder, bInitializing };
		TUniquePtr<FScopedExpectedPlayers> ExpectedPlayers;
		TUniquePtr<FScopedMatchTuning> Tuning;
		FVeyraGreyboxLayout Layout;
		int32 MoverId = INDEX_NONE;
		FVector Start = FVector::ZeroVector;
		FVector Ahead = FVector::ZeroVector;

		// Fixture values: a short preparation, how far Marek walks toward the lane's centre before he turns
		// back, and a comparison allowance for float positions.
		static constexpr double ShortPreparationSeconds = 0.1;
		static constexpr double WalkDistance = 900.0;
		static constexpr double PositionSlack = 1.0;

		BEFORE_EACH()
		{
			IgnoreKnownIrisWarnings(*TestRunner);
			ASSERT_THAT(IsTrue(VeyraGreybox::LoadLayout(Layout).IsEmpty()));
			Tuning = MakeUnique<FScopedMatchTuning>();
			Tuning->Tuning.Phases.PreparationSeconds = ShortPreparationSeconds;
			Tuning->Tuning.DeveloperMatch.Vanguards = { ContentId(TEXT("marek")) };
			Tuning->Tuning.DeveloperMatch.StartingRank = EVeyraDeveloperStartingRank::None;
			ExpectedPlayers = MakeUnique<FScopedExpectedPlayers>(MatchClientCount);
			BuildMatchNetwork(Network);
		}

		AFTER_EACH()
		{
			Tuning.Reset();
			ExpectedPlayers.Reset();
		}

		static const AVeyraCompanion* NixOf(const FState& State, int32 ClientIndex)
		{
			const AVeyraPlayerState* Participant = ParticipantOf(State, ClientIndex);
			const UVeyraCompanionSubsystem* Keeper = State.World ? State.World->GetSubsystem<UVeyraCompanionSubsystem>() : nullptr;
			return Participant && Keeper ? Keeper->Find(*Participant->GetAbilitySystemComponent()) : nullptr;
		}

		bool IsNear2D(const AActor* Actor, const FVector& Point) const
		{
			return Actor && FVector::Dist2D(Actor->GetActorLocation(), Point) <= Tuning->Tuning.Orders.ArrivalTolerance + PositionSlack;
		}

		TEST_METHOD(NixFormsBesideMarekAndNeverHoldsHimUp)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				// Accord summons Nix before Marek's body spawns; the companion's keeper forms it at its next thought after.
				.UntilServer(TEXT("Nix forms for each Marek"), [](FState& State) {
					for (int32 ClientIndex = 0; ClientIndex < MatchClientCount; ++ClientIndex)
					{
						if (!NixOf(State, ClientIndex))
						{
							return false;
						}
					}
					return true;
				})
				.ThenServer(TEXT("Nix stands beside each Marek"), [this](FState& State) {
					const FVeyraCompanionTuning* Nix = UVeyraAbilitiesTuningSubsystem::FindCompanion(ContentId(TEXT("marek_nix")));
					ASSERT_THAT(IsNotNull(Nix));
					for (int32 ClientIndex = 0; ClientIndex < MatchClientCount; ++ClientIndex)
					{
						const APawn* Body = ParticipantOf(State, ClientIndex)->GetPawn();
						const AVeyraCompanion* Companion = NixOf(State, ClientIndex);
						ASSERT_THAT(IsTrue(Body && Companion));
						ASSERT_THAT(IsTrue(VeyraTargeting::EdgeToEdgeDistance(*Companion, *Body) <= Nix->FollowDistance,
							TEXT("beside its owner's body, not where its owner's PlayerState stands")));
					}
					const AVeyraPlayerState* Mover = ParticipantOf(State, 0);
					MoverId = Mover->GetPlayerId();
					Start = Mover->GetPawn()->GetActorLocation();
					// Toward the lane's centre, with Nix following behind.
					Ahead = Start - Start.GetSafeNormal2D() * WalkDistance;
				})
				.ThenClient(TEXT("Marek walks toward the centre"), 0, [this](FState& State) { LocalControllerOf(State.World)->IssueMoveOrder(Ahead); })
				.UntilServer(TEXT("He arrives"), [this](FState& State) { return IsNear2D(FindVanguard(State.World, MoverId), Ahead); })
				.ThenClient(TEXT("He turns back, where Nix followed him"), 0, [this](FState& State) { LocalControllerOf(State.World)->IssueMoveOrder(Start); })
				.UntilServer(TEXT("And arrives again"), [this](FState& State) { return IsNear2D(FindVanguard(State.World, MoverId), Start); });
		}
	};
}

#endif // ENABLE_PIE_NETWORK_TEST
