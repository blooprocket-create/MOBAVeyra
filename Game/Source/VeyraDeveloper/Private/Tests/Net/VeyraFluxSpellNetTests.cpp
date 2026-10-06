// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"

#if ENABLE_PIE_NETWORK_TEST

#include "Cooldowns/VeyraCooldownComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Loadout/VeyraAbilityLoadoutComponent.h"
#include "Tests/Net/VeyraMatchNetTestHelpers.h"
#include "Tests/Net/VeyraNetTestHelpers.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "VeyraAbilitiesVerbs.h"
#include "VeyraPlayerController.h"
#include "VeyraPlayerState.h"
#include "VeyraTeamFluxSubsystem.h"

namespace VeyraNetTests
{
	// Veyra.Net.FluxSpells.*: a team's permanent Flux opens its players' Flux Spell slots, anywhere and
	// at once, and each player sees their own; temporary Flux never does (Battleground Bible §14; ADR-015 §4).
	NETWORK_TEST_CLASS(FluxSpells, "Veyra.Net")
	{
		struct FState : public FBasePIENetworkComponentState
		{
		};

		FPIENetworkComponent<FState> Network{ TestRunner, TestCommandBuilder, bInitializing };
		TUniquePtr<FScopedExpectedPlayers> ExpectedPlayers;
		TUniquePtr<FScopedMatchTuning> Tuning;
		FVeyraGreyboxLayout Layout;

		// Fixture value: a short preparation.
		static constexpr double ShortPreparationSeconds = 0.1;

		BEFORE_EACH()
		{
			IgnoreKnownIrisWarnings(*TestRunner);
			SharedSpell = FVeyraContentId();
			CoolingId = INDEX_NONE;
			ASSERT_THAT(IsTrue(VeyraGreybox::LoadLayout(Layout).IsEmpty()));
			Tuning = MakeUnique<FScopedMatchTuning>();
			Tuning->Tuning.Phases.PreparationSeconds = ShortPreparationSeconds;
			ExpectedPlayers = MakeUnique<FScopedExpectedPlayers>(MatchClientCount);
			BuildMatchNetwork(Network);
		}

		AFTER_EACH()
		{
			Tuning.Reset();
			ExpectedPlayers.Reset();
		}

		/** Every participant's first spell, as the server sees it: its cast refusal, or None. */
		static TArray<EVeyraCastRejection> CastFirstSpells(const UWorld& World, EVeyraTeam Team)
		{
			TArray<EVeyraCastRejection> Casts;
			for (APlayerState* Participant : World.GetGameState()->PlayerArray)
			{
				AVeyraPlayerState* VeyraParticipant = Cast<AVeyraPlayerState>(Participant);
				if (VeyraParticipant && VeyraParticipant->GetVeyraTeam() == Team)
				{
					Casts.Add(VeyraAbilities::TryCast(*VeyraParticipant->GetAbilitySystemComponent(), EVeyraAbilitySlot::Spell1, FVeyraCastTarget()));
				}
			}
			return Casts;
		}

		TEST_METHOD(PermanentFluxOpensATeamsSlotsAndTemporaryFluxNever)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Equip each participant with the roster's first spell, and grant temporary Flux"), [this](FState& State) {
					const FVeyraContentId Spell = UVeyraAbilitiesTuningSubsystem::Get().FluxSpells.Roster[0];
					for (APlayerState* Participant : State.World->GetGameState()->PlayerArray)
					{
						AVeyraPlayerState* VeyraParticipant = Cast<AVeyraPlayerState>(Participant);
						ASSERT_THAT(IsTrue(VeyraParticipant && VeyraParticipant->FindComponentByClass<UVeyraAbilityLoadoutComponent>()->Grant(
							*VeyraParticipant->GetAbilitySystemComponent(), EVeyraAbilitySlot::Spell1, Spell)));
					}
					UVeyraTeamFluxSubsystem* Flux = State.World->GetSubsystem<UVeyraTeamFluxSubsystem>();
					Flux->Grant(EVeyraTeam::A, EVeyraFluxSource::Inhibitor);
					Flux->Grant(EVeyraTeam::A, EVeyraFluxSource::FluxWell);
					ASSERT_THAT(IsTrue(Flux->GetActive(EVeyraTeam::A) > 0.0 && Flux->GetPermanent(EVeyraTeam::A) == 0.0));
					for (const EVeyraCastRejection Rejection : CastFirstSpells(*State.World, EVeyraTeam::A))
					{
						ASSERT_THAT(IsTrue(Rejection == EVeyraCastRejection::Locked, TEXT("temporary Flux unlocks nothing")));
					}
				})
				.ThenServer(TEXT("Grant Team A a Spire's permanent Flux"), [this](FState& State) {
					State.World->GetSubsystem<UVeyraTeamFluxSubsystem>()->Grant(EVeyraTeam::A, EVeyraFluxSource::LaneSpire);
					for (const EVeyraCastRejection Rejection : CastFirstSpells(*State.World, EVeyraTeam::A))
					{
						ASSERT_THAT(IsTrue(Rejection != EVeyraCastRejection::Locked, TEXT("the first slot opened at once")));
					}
					for (const EVeyraCastRejection Rejection : CastFirstSpells(*State.World, EVeyraTeam::B))
					{
						ASSERT_THAT(IsTrue(Rejection == EVeyraCastRejection::Locked, TEXT("only for its team")));
					}
				})
				.UntilClients(TEXT("Each player sees their own slots"), [](FState& State) {
					const AVeyraPlayerController* Controller = LocalControllerOf(State.World);
					const AVeyraPlayerState* Own = Controller ? Controller->GetPlayerState<AVeyraPlayerState>() : nullptr;
					const UVeyraAbilityLoadoutComponent* Loadout = Own ? Own->FindComponentByClass<UVeyraAbilityLoadoutComponent>() : nullptr;
					return Loadout && Loadout->GetUnlockedSpellSlots() == (Own->GetVeyraTeam() == EVeyraTeam::A ? 1 : 0);
				});
		}

		TEST_METHOD(EveryPlayerSeesEachParticipantsFluxSpellsAndWhenTheyAreReady)
		{
			// Whoever selects a participant sees its Flux Spells, whether its team has unlocked them, and their cooldowns
			// (ADR-066 §4); its other slots and cooldowns stay its own.
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Equip everyone with a spell, open Team A's slot, and start one of Team A's spells cooling"), [this](FState& State) {
					SharedSpell = UVeyraAbilitiesTuningSubsystem::Get().FluxSpells.Roster[0];
					for (APlayerState* Participant : State.World->GetGameState()->PlayerArray)
					{
						AVeyraPlayerState* VeyraParticipant = Cast<AVeyraPlayerState>(Participant);
						ASSERT_THAT(IsTrue(VeyraParticipant && VeyraParticipant->FindComponentByClass<UVeyraAbilityLoadoutComponent>()->Grant(
							*VeyraParticipant->GetAbilitySystemComponent(), EVeyraAbilitySlot::Spell1, SharedSpell)));
						if (VeyraParticipant->GetVeyraTeam() == EVeyraTeam::A && CoolingId == INDEX_NONE)
						{
							CoolingId = VeyraParticipant->GetPlayerId();
							VeyraParticipant->FindComponentByClass<UVeyraCooldownComponent>()->StartCooldown(SharedSpell, SpellCooldownSeconds, EVeyraCooldownHaste::Fixed);
						}
					}
					ASSERT_THAT(IsTrue(CoolingId != INDEX_NONE));
					State.World->GetSubsystem<UVeyraTeamFluxSubsystem>()->Grant(EVeyraTeam::A, EVeyraFluxSource::LaneSpire);
				})
				.UntilClients(TEXT("Every machine sees every participant's spell, its unlock and its cooldown"), [this](FState& State) {
					const double Now = State.World->GetGameState()->GetServerWorldTimeSeconds();
					int32 Seen = 0;
					for (const APlayerState* Participant : State.World->GetGameState()->PlayerArray)
					{
						const AVeyraPlayerState* VeyraParticipant = Cast<AVeyraPlayerState>(Participant);
						const UVeyraAbilityLoadoutComponent* Loadout = VeyraParticipant ? VeyraParticipant->FindComponentByClass<UVeyraAbilityLoadoutComponent>() : nullptr;
						const UVeyraCooldownComponent* Cooldowns = VeyraParticipant ? VeyraParticipant->FindComponentByClass<UVeyraCooldownComponent>() : nullptr;
						if (!Loadout || !Cooldowns || Loadout->GetSharedSpells().IsEmpty() || Loadout->GetSharedSpells()[0] != SharedSpell
							|| Loadout->GetUnlockedSpellSlots() != (VeyraParticipant->GetVeyraTeam() == EVeyraTeam::A ? 1 : 0))
						{
							return false;
						}
						const bool bCooling = Cooldowns->GetSharedRemainingSeconds(SharedSpell, Now) > 0.0;
						if (bCooling != (VeyraParticipant->GetPlayerId() == CoolingId))
						{
							return false;
						}
						++Seen;
					}
					return Seen == MatchClientCount;
				});
		}

		/** The spell every participant holds in the sharing test, and the participant whose spell cools. */
		FVeyraContentId SharedSpell;
		int32 CoolingId = INDEX_NONE;

		/** Fixture value: a cooldown that outlasts the test. */
		static constexpr double SpellCooldownSeconds = 600.0;
	};
}

#endif // ENABLE_PIE_NETWORK_TEST
