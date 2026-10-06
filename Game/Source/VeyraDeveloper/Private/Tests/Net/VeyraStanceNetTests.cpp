// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"

#if ENABLE_PIE_NETWORK_TEST

#include "GameFramework/PlayerState.h"
#include "Loadout/VeyraAbilityLoadoutComponent.h"
#include "Tests/Net/VeyraMatchNetTestHelpers.h"
#include "Tests/Net/VeyraNetTestHelpers.h"
#include "VeyraPlayerState.h"

namespace VeyraNetTests
{
	// Veyra.Net.ReplicatedStance.*: the stance whose set a Vanguard's slots hold reaches every machine, so its body can
	// show it to everyone, while what each slot holds stays its own client's (ADR-031 §3; ADR-064 §1).
	NETWORK_TEST_CLASS(ReplicatedStance, "Veyra.Net")
	{
		struct FState : public FBasePIENetworkComponentState
		{
		};

		FPIENetworkComponent<FState> Network{ TestRunner, TestCommandBuilder, bInitializing };
		TUniquePtr<FScopedExpectedPlayers> ExpectedPlayers;
		TUniquePtr<FScopedMatchTuning> Tuning;
		FVeyraGreyboxLayout Layout;

		// Fixture values: a short preparation, and the stance the first client's Vanguard takes.
		static constexpr double ShortPreparationSeconds = 0.1;
		static constexpr const TCHAR* StanceId = TEXT("test_other_stance");

		int32 HolderId = INDEX_NONE;

		BEFORE_EACH()
		{
			IgnoreKnownIrisWarnings(*TestRunner);
			ASSERT_THAT(IsTrue(VeyraGreybox::LoadLayout(Layout).IsEmpty()));
			Tuning = MakeUnique<FScopedMatchTuning>();
			Tuning->Tuning.Phases.PreparationSeconds = ShortPreparationSeconds;
			// Loading waits for both clients rather than the loading timeout.
			ExpectedPlayers = MakeUnique<FScopedExpectedPlayers>(MatchClientCount);
			BuildMatchNetwork(Network);
		}

		AFTER_EACH()
		{
			Tuning.Reset();
			ExpectedPlayers.Reset();
		}

		static UVeyraAbilityLoadoutComponent* HolderLoadout(FState& State)
		{
			const AVeyraPlayerController* Holder = ServerControllerOf(State, 0);
			const AVeyraPlayerState* Participant = Holder ? Holder->GetPlayerState<AVeyraPlayerState>() : nullptr;
			return Participant ? Participant->FindComponentByClass<UVeyraAbilityLoadoutComponent>() : nullptr;
		}

		/** The holder's stance as this machine sees it. */
		FVeyraContentId StanceSeenBy(FState& State) const
		{
			const AVeyraGameState* GameState = GameStateOf(State.World);
			const TObjectPtr<APlayerState>* Participant = GameState
				? GameState->PlayerArray.FindByPredicate([this](const APlayerState* Candidate) { return Candidate && Candidate->GetPlayerId() == HolderId; })
				: nullptr;
			const UVeyraAbilityLoadoutComponent* Loadout = Participant ? (*Participant)->FindComponentByClass<UVeyraAbilityLoadoutComponent>() : nullptr;
			return Loadout ? Loadout->GetStance() : FVeyraContentId();
		}

		TEST_METHOD(EveryClientSeesTheStanceItsSlotsHold)
		{
			const FVeyraContentId Stance = FVeyraContentId::FromText(StanceId).GetValue();
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("The first client's Vanguard takes a stance"), [this, Stance](FState& State) {
					UVeyraAbilityLoadoutComponent* Loadout = HolderLoadout(State);
					ASSERT_THAT(IsNotNull(Loadout));
					HolderId = ServerControllerOf(State, 0)->GetPlayerState<AVeyraPlayerState>()->GetPlayerId();
					Loadout->SetStance(Stance);
				})
				.UntilClients(TEXT("Every client sees it, its own and the other"), [this, Stance](FState& State) { return StanceSeenBy(State) == Stance; })
				.ThenServer(TEXT("It returns to its own set"), [](FState& State) { HolderLoadout(State)->SetStance(FVeyraContentId()); })
				.UntilClients(TEXT("Every client sees it back"), [this](FState& State) { return !StanceSeenBy(State).IsValid(); });
		}
	};
}

#endif // ENABLE_PIE_NETWORK_TEST
