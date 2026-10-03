// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"

#if ENABLE_PIE_NETWORK_TEST && WITH_VEYRA_UI

#include "Algo/AnyOf.h"
#include "Cues/VeyraCombatCueSubsystem.h"
#include "Tests/Net/VeyraNetTestHelpers.h"
#include "Tests/Net/VeyraVanguardNetTestHelpers.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "VeyraVanguardController.h"

namespace VeyraNetTests
{
	// Veyra.Net.FightCues.*: every client reads a fight's moments from the state it receives (ADR-063 §1): an
	// attack's commit and the hit it lands, and a cast's commit even with no windup. Both players are Cairn.
	NETWORK_TEST_CLASS(FightCues, "Veyra.Net")
	{
		struct FState : public FBasePIENetworkComponentState
		{
		};

		FPIENetworkComponent<FState> Network{ TestRunner, TestCommandBuilder, bInitializing };
		TUniquePtr<FScopedExpectedPlayers> ExpectedPlayers;
		TUniquePtr<FScopedMatchTuning> Tuning;
		TUniquePtr<FScopedAbilitiesTuning> Abilities;
		FVeyraGreyboxLayout Layout;
		FVanguardDuel Duel;

		// Fixture values: a short preparation, the level that opens every rank, and the duel's distance.
		static constexpr double ShortPreparationSeconds = 0.1;
		static constexpr int32 UltimateLevel = 6;
		static constexpr double Apart = 300.0;

		/** The cues each client raised, by client. */
		TArray<TArray<FVeyraCombatCue>> Raised;

		BEFORE_EACH()
		{
			IgnoreKnownIrisWarnings(*TestRunner);
			ASSERT_THAT(IsTrue(VeyraGreybox::LoadLayout(Layout).IsEmpty()));
			Tuning = MakeUnique<FScopedMatchTuning>();
			Tuning->Tuning.Phases.PreparationSeconds = ShortPreparationSeconds;
			Tuning->Tuning.DeveloperMatch.Vanguards = { ContentId(TEXT("cairn")) };
			Tuning->Tuning.DeveloperMatch.StartingRank = EVeyraDeveloperStartingRank::None;
			// Cairn's committed kit, with his slam cast at once: no windup shows it.
			const FVeyraAbilitiesTuning Committed = UVeyraAbilitiesTuningSubsystem::Get();
			Abilities = MakeUnique<FScopedAbilitiesTuning>();
			Abilities->Tuning = Committed;
			Abilities->Tuning.Area.FindChecked(ContentId(TEXT("cairn_crushing_hold"))).Cast.WindupSeconds = 0.0;
			ExpectedPlayers = MakeUnique<FScopedExpectedPlayers>(MatchClientCount);
			Raised.SetNum(MatchClientCount);
			BuildMatchNetwork(Network);
		}

		AFTER_EACH()
		{
			Abilities.Reset();
			Tuning.Reset();
			ExpectedPlayers.Reset();
		}

		/** Prepares the duel, and has every client listen for cues. */
		FPIENetworkComponent<FState>& Prepare(FPIENetworkComponent<FState>& Chain)
		{
			return Chain.ThenServer(TEXT("Prepare the duel"), [this](FState& State) { ASSERT_THAT(IsTrue(Duel.Prepare(State, ContentId(TEXT("cairn")), UltimateLevel, Apart))); })
				.ThenClients(TEXT("Listen for cues"), [this](FState& State) {
					UVeyraCombatCueSubsystem* Cues = State.World->GetSubsystem<UVeyraCombatCueSubsystem>();
					ASSERT_THAT(IsNotNull(Cues));
					Cues->OnCue.AddLambda([this, Index = State.ClientIndex](const FVeyraCombatCue& Cue) { Raised[Index].Add(Cue); });
				});
		}

		/** Whether the client at State's index raised a cue of Kind whose unit is the Vanguard with PlayerId. */
		bool Saw(FState& State, EVeyraCombatCueKind Kind, int32 PlayerId) const
		{
			const AActor* Unit = FindVanguard(State.World, PlayerId);
			return Unit && Algo::AnyOf(Raised[State.ClientIndex], [Kind, Unit](const FVeyraCombatCue& Cue) { return Cue.Kind == Kind && Cue.Unit.Get() == Unit; });
		}

		TEST_METHOD(EveryClientSeesAnAttackCommitAndItsHit)
		{
			Prepare(StartMatch(Network, Layout, EVeyraMatchPhase::Live))
				.ThenServer(TEXT("The first attacks the second"), [this](FState& State) {
					APawn* Target = ParticipantOf(State, 1)->GetPawn();
					ASSERT_THAT(IsTrue(ParticipantOf(State, 0)->GetVanguardController()->AttackUnit(*Target) == EVeyraOrderRejection::None));
				})
				.UntilClients(TEXT("Each client sees the attack commit and land"), [this](FState& State) {
					return Saw(State, EVeyraCombatCueKind::AttackCommit, Duel.CasterId) && Saw(State, EVeyraCombatCueKind::Hit, Duel.TargetId);
				});
		}

		TEST_METHOD(EveryClientSeesACastCommitWithNoWindup)
		{
			Prepare(StartMatch(Network, Layout, EVeyraMatchPhase::Live))
				.ThenClient(TEXT("Slam at the enemy"), 0, [this](FState& State) { Duel.CastAtTheTarget(State.World, EVeyraAbilitySlot::W); })
				.UntilClients(TEXT("Each client sees the cast commit"), [this](FState& State) {
					const AActor* Caster = FindVanguard(State.World, Duel.CasterId);
					return Caster && Algo::AnyOf(Raised[State.ClientIndex], [Caster](const FVeyraCombatCue& Cue) {
						return Cue.Kind == EVeyraCombatCueKind::CastCommit && Cue.Unit.Get() == Caster && Cue.Ability == ContentId(TEXT("cairn_crushing_hold"));
					});
				});
		}
	};
}

#endif // ENABLE_PIE_NETWORK_TEST && WITH_VEYRA_UI
