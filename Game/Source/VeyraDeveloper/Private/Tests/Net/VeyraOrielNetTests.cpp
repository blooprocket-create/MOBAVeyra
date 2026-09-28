// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"

#if ENABLE_PIE_NETWORK_TEST

#include "Casting/VeyraCastStateComponent.h"
#include "Cooldowns/VeyraCooldownComponent.h"
#include "Delivery/VeyraEffectDelivery.h"
#include "Delivery/VeyraProjectile.h"
#include "EngineUtils.h"
#include "Passives/VeyraGatheringLightPassive.h"
#include "Tests/Net/VeyraMatchNetTestHelpers.h"
#include "Tests/Net/VeyraNetTestHelpers.h"
#include "Tests/Net/VeyraVanguardNetTestHelpers.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"

namespace VeyraNetTests
{
	// Veyra.Net.Vanguards.Oriel.*: Oriel's kit in a match (Character Bible §20): Splinter Lance,
	// Shattered Sky's delayed fall, Mirror Veil, Final Radiance's channelled beam, and Gathering Light
	// primed by three casts and spent by a fourth. Both players are Oriel at level 6 with a rank in each
	// ability; amounts come from the tuning.
	NETWORK_TEST_CLASS(Oriel, "Veyra.Net.Vanguards")
	{
		struct FState : public FBasePIENetworkComponentState
		{
		};

		FPIENetworkComponent<FState> Network{ TestRunner, TestCommandBuilder, bInitializing };
		TUniquePtr<FScopedExpectedPlayers> ExpectedPlayers;
		TUniquePtr<FScopedMatchTuning> Tuning;
		FVeyraGreyboxLayout Layout;
		FVanguardDuel Duel;

		// Fixture values: a short preparation, the level that opens the first ultimate rank, how far
		// apart the Vanguards stand (inside every ability's reach), and a tolerance for shields.
		static constexpr double ShortPreparationSeconds = 0.1;
		static constexpr int32 UltimateLevel = 6;
		static constexpr double DuelDistance = 600.0;
		static constexpr double Tolerance = 1e-2;

		/** The target's Health lost after the beam's first tick. */
		double FirstTickLoss = 0.0;

		BEFORE_EACH()
		{
			IgnoreKnownIrisWarnings(*TestRunner);
			ASSERT_THAT(IsTrue(VeyraGreybox::LoadLayout(Layout).IsEmpty()));
			Tuning = MakeUnique<FScopedMatchTuning>();
			Tuning->Tuning.Phases.PreparationSeconds = ShortPreparationSeconds;
			Tuning->Tuning.DeveloperMatch.Vanguards = { ContentId(TEXT("oriel")) };
			Tuning->Tuning.DeveloperMatch.StartingRank = EVeyraDeveloperStartingRank::None;
			ExpectedPlayers = MakeUnique<FScopedExpectedPlayers>(MatchClientCount);
			BuildMatchNetwork(Network);
		}

		AFTER_EACH()
		{
			Tuning.Reset();
			ExpectedPlayers.Reset();
		}

		void PrepareDuel(FState& State)
		{
			ASSERT_THAT(IsTrue(Duel.Prepare(State, ContentId(TEXT("oriel")), UltimateLevel, DuelDistance)));
		}

		static const UVeyraGatheringLightPassive* GatheringLightOf(FState& State)
		{
			return Cast<UVeyraGatheringLightPassive>(ParticipantOf(State, 0)->GetPassive());
		}

		static bool IsFree(FState& State)
		{
			return !ParticipantOf(State, 0)->FindComponentByClass<UVeyraCastStateComponent>()->IsBusy();
		}

		static bool FragmentFlies(FState& State)
		{
			for (TActorIterator<AVeyraProjectile> It(State.World); It; ++It)
			{
				if (It->GetAbility() == ContentId(TEXT("oriel_gathering_light")))
				{
					return true;
				}
			}
			return false;
		}

		TEST_METHOD(SplinterLanceStrikesTheFirstEnemy)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Prepare the duel"), [this](FState& State) { PrepareDuel(State); })
				.ThenClient(TEXT("Throw the lance"), 0, [this](FState& State) { Duel.CastAtTheTarget(State.World, EVeyraAbilitySlot::Q); })
				.UntilServer(TEXT("It lands"), [](FState& State) { return HealthLost(ParticipantOf(State, 1)) > 0.0; })
				.ThenServer(TEXT("One stack of Gathering Light"), [this](FState& State) { ASSERT_THAT(AreEqual(1, GatheringLightOf(State)->GetStacks())); });
		}

		TEST_METHOD(ShatteredSkyFallsAfterItsDelayAndSlows)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Prepare the duel"), [this](FState& State) { PrepareDuel(State); })
				.ThenClient(TEXT("Call the shards down"), 0, [this](FState& State) { Duel.CastAtTheTarget(State.World, EVeyraAbilitySlot::W); })
				.UntilClients(TEXT("Every client sees the enemy slowed"), [this](FState& State) { return SeesStatus(State.World, Duel.TargetId, EVeyraStatusKind::Slow); })
				.ThenServer(TEXT("With the fall's damage"), [this](FState& State) { ASSERT_THAT(IsTrue(HealthLost(ParticipantOf(State, 1)) > 0.0)); });
		}

		TEST_METHOD(MirrorVeilShieldsAndQuickensOriel)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Prepare the duel"), [this](FState& State) { PrepareDuel(State); })
				.ThenClient(TEXT("Raise the veil"), 0, [this](FState& State) { Duel.CastAtTheTarget(State.World, EVeyraAbilitySlot::E); })
				.UntilClients(TEXT("Every client sees her quickened"), [this](FState& State) { return SeesStatus(State.World, Duel.CasterId, EVeyraStatusKind::MoveSpeed); })
				.ThenServer(TEXT("With the rank's shield"), [this](FState& State) {
					const AVeyraPlayerState* Caster = ParticipantOf(State, 0);
					const FVeyraShieldTuning& Shield = UVeyraAbilitiesTuningSubsystem::Get().SelfBuff.FindChecked(ContentId(TEXT("oriel_mirror_veil"))).Shields[0];
					const double Expected = VeyraEffectDelivery::ShieldGrant(*Caster->GetAbilitySystemComponent(), Shield, 1).Amount;
					const TArray<FVeyraShieldEntry>& Shields = ShieldsOf(Caster);
					ASSERT_THAT(IsTrue(Shields.Num() == 1 && FMath::IsNearlyEqual(Shields[0].Remaining, Expected, Tolerance)));
				});
		}

		TEST_METHOD(FinalRadianceBurnsAlongItsBeamTickByTick)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Prepare the duel"), [this](FState& State) { PrepareDuel(State); })
				.ThenClient(TEXT("Channel the beam"), 0, [this](FState& State) { Duel.CastAtTheTarget(State.World, EVeyraAbilitySlot::R); })
				.UntilServer(TEXT("The first tick burns"), [this](FState& State) {
					FirstTickLoss = HealthLost(ParticipantOf(State, 1));
					return FirstTickLoss > 0.0;
				})
				.UntilServer(TEXT("A later tick burns again"), [this](FState& State) { return HealthLost(ParticipantOf(State, 1)) > FirstTickLoss; })
				.UntilServer(TEXT("The channel ends"), [](FState& State) { return IsFree(State); })
				.ThenServer(TEXT("One stack for the whole channel"), [this](FState& State) { ASSERT_THAT(AreEqual(1, GatheringLightOf(State)->GetStacks())); });
		}

		TEST_METHOD(GatheringLightPrimesOverThreeCastsAndFiresWithTheFourth)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Prepare the duel"), [this](FState& State) { PrepareDuel(State); })
				.ThenClient(TEXT("Lance"), 0, [this](FState& State) { Duel.CastAtTheTarget(State.World, EVeyraAbilitySlot::Q); })
				.UntilServer(TEXT("One stack"), [](FState& State) { return GatheringLightOf(State)->GetStacks() == 1 && IsFree(State); })
				.ThenClient(TEXT("Shards"), 0, [this](FState& State) { Duel.CastAtTheTarget(State.World, EVeyraAbilitySlot::W); })
				.UntilServer(TEXT("Two stacks"), [](FState& State) { return GatheringLightOf(State)->GetStacks() == 2 && IsFree(State); })
				.ThenClient(TEXT("Beam"), 0, [this](FState& State) { Duel.CastAtTheTarget(State.World, EVeyraAbilitySlot::R); })
				.UntilServer(TEXT("Primed once the channel ends"), [](FState& State) { return GatheringLightOf(State)->IsPrimed() && IsFree(State); })
				.UntilServer(TEXT("The lance is ready again"), [](FState& State) {
					return ParticipantOf(State, 0)->FindComponentByClass<UVeyraCooldownComponent>()->GetRemainingSecondsNow(ContentId(TEXT("oriel_splinter_lance"))) <= 0.0;
				})
				.ThenClient(TEXT("Lance again"), 0, [this](FState& State) { Duel.CastAtTheTarget(State.World, EVeyraAbilitySlot::Q); })
				.UntilServer(TEXT("The stacks are spent on a fragment"), [](FState& State) { return GatheringLightOf(State)->GetStacks() == 0 && FragmentFlies(State); });
		}
	};
}

#endif // ENABLE_PIE_NETWORK_TEST
