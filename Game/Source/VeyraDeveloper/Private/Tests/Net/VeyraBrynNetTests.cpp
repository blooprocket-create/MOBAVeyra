// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"

#if ENABLE_PIE_NETWORK_TEST

#include "Attacks/VeyraBasicAttackComponent.h"
#include "Cooldowns/VeyraCooldownComponent.h"
#include "Movement/VeyraMovementComponent.h"
#include "Tests/Net/VeyraMatchNetTestHelpers.h"
#include "Tests/Net/VeyraNetTestHelpers.h"
#include "Tests/Net/VeyraVanguardNetTestHelpers.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"

namespace VeyraNetTests
{
	// Veyra.Net.Vanguards.Bryn.*: Bryn's kit in a match (Character Bible §19): Breach on every third
	// shot, Breach Round's blast, Sounding Flare's slow, Kickback's blast and recoil, and Last
	// Broadside landing after its charge. Both players are Bryn at level 6 with a rank in each ability;
	// amounts come from the tuning.
	NETWORK_TEST_CLASS(Bryn, "Veyra.Net.Vanguards")
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
		// apart the Vanguards stand for her shots and for Kickback, and how much of the recoil counts.
		static constexpr double ShortPreparationSeconds = 0.1;
		static constexpr int32 UltimateLevel = 6;
		static constexpr double ShotDistance = 500.0;
		static constexpr double BlastDistance = 250.0;
		static constexpr double RecoilShare = 0.5;

		/** On the server: the Health the target had lost after each of the first player's hits. */
		TArray<double> LossAfterHit;

		BEFORE_EACH()
		{
			IgnoreKnownIrisWarnings(*TestRunner);
			ASSERT_THAT(IsTrue(VeyraGreybox::LoadLayout(Layout).IsEmpty()));
			Tuning = MakeUnique<FScopedMatchTuning>();
			Tuning->Tuning.Phases.PreparationSeconds = ShortPreparationSeconds;
			Tuning->Tuning.DeveloperMatch.Vanguards = { ContentId(TEXT("bryn")) };
			Tuning->Tuning.DeveloperMatch.StartingRank = EVeyraDeveloperStartingRank::None;
			ExpectedPlayers = MakeUnique<FScopedExpectedPlayers>(MatchClientCount);
			BuildMatchNetwork(Network);
		}

		AFTER_EACH()
		{
			Tuning.Reset();
			ExpectedPlayers.Reset();
		}

		void PrepareDuel(FState& State, double Distance)
		{
			ASSERT_THAT(IsTrue(Duel.Prepare(State, ContentId(TEXT("bryn")), UltimateLevel, Distance)));
		}

		/** On the server: records the target's lost Health as each of the first player's shots lands. */
		void WatchHits(FState& State)
		{
			UVeyraBasicAttackComponent* Attacks = ParticipantOf(State, 0)->FindComponentByClass<UVeyraBasicAttackComponent>();
			AVeyraPlayerState* Target = ParticipantOf(State, 1);
			Attacks->OnHit.AddLambda([this, Target](const FVeyraAttackEvent&) { LossAfterHit.Add(HealthLost(Target)); });
		}

		/** What shot Shot, from 1, dealt the target. */
		double DealtBy(int32 Shot) const
		{
			return LossAfterHit[Shot - 1] - (Shot > 1 ? LossAfterHit[Shot - 2] : 0.0);
		}

		void AttackTheTarget(FState& State) const
		{
			LocalControllerOf(State.World)->IssueAttackOrder(FindVanguard(State.World, Duel.TargetId));
		}

		TEST_METHOD(BreachBurstsOnTheThirdShot)
		{
			const int32 HitsToBreach = UVeyraVanguardsTuningSubsystem::FindBreach(ContentId(TEXT("bryn_breach")))->HitsToBreach;
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Prepare the duel"), [this](FState& State) {
					PrepareDuel(State, ShotDistance);
					WatchHits(State);
				})
				.ThenClient(TEXT("Open fire"), 0, [this](FState& State) { AttackTheTarget(State); })
				.UntilServer(TEXT("Enough shots to breach"), [this, HitsToBreach](FState&) { return LossAfterHit.Num() >= HitsToBreach; })
				.ThenServer(TEXT("Only the breaching shot hits harder"), [this, HitsToBreach](FState&) {
					for (int32 Shot = 2; Shot < HitsToBreach; ++Shot)
					{
						ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(DealtBy(Shot), DealtBy(1), 1e-2), FString::Printf(TEXT("shot %d dealt %g"), Shot, DealtBy(Shot))));
					}
					ASSERT_THAT(IsTrue(DealtBy(HitsToBreach) > DealtBy(1), FString::Printf(TEXT("the breaching shot dealt %g, a plain one %g"), DealtBy(HitsToBreach), DealtBy(1))));
				});
		}

		TEST_METHOD(BreachRoundLoadsTheNextShot)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Prepare the duel"), [this](FState& State) {
					PrepareDuel(State, ShotDistance);
					WatchHits(State);
				})
				.ThenClient(TEXT("Load the round"), 0, [this](FState& State) { Duel.CastAtTheTarget(State.World, EVeyraAbilitySlot::Q); })
				.ThenClient(TEXT("Fire it, then keep firing"), 0, [this](FState& State) { AttackTheTarget(State); })
				.UntilServer(TEXT("Two shots land"), [this](FState&) { return LossAfterHit.Num() >= 2; })
				.ThenServer(TEXT("The loaded shot hit harder than the plain one after it"), [this](FState&) {
					ASSERT_THAT(IsTrue(DealtBy(1) > DealtBy(2), FString::Printf(TEXT("loaded %g, plain %g"), DealtBy(1), DealtBy(2))));
				});
		}

		TEST_METHOD(SoundingFlareSlowsWhereItBursts)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Prepare the duel"), [this](FState& State) { PrepareDuel(State, ShotDistance); })
				.ThenClient(TEXT("Fire the flare"), 0, [this](FState& State) { Duel.CastAtTheTarget(State.World, EVeyraAbilitySlot::W); })
				.UntilClients(TEXT("Every client sees the enemy slowed"), [this](FState& State) { return SeesStatus(State.World, Duel.TargetId, EVeyraStatusKind::Slow); });
		}

		TEST_METHOD(KickbackBlastsTheEnemyAndThrowsBrynBack)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Prepare the duel"), [this](FState& State) { PrepareDuel(State, BlastDistance); })
				.ThenClient(TEXT("Kick back"), 0, [this](FState& State) { Duel.CastAtTheTarget(State.World, EVeyraAbilitySlot::E); })
				.UntilClients(TEXT("Every client sees the enemy slowed"), [this](FState& State) { return SeesStatus(State.World, Duel.TargetId, EVeyraStatusKind::Slow); })
				.UntilServer(TEXT("The recoil ends"), [](FState& State) { return !ParticipantOf(State, 0)->GetPawn()->FindComponentByClass<UVeyraMovementComponent>()->IsDashing(); })
				.ThenServer(TEXT("The blast hurt, and she flew back"), [this](FState& State) {
					ASSERT_THAT(IsTrue(HealthLost(ParticipantOf(State, 1)) > 0.0));
					const double Recoil = UVeyraAbilitiesTuningSubsystem::Get().Dash.FindChecked(ContentId(TEXT("bryn_kickback"))).Distance;
					const double Apart = FVector::Dist2D(ParticipantOf(State, 0)->GetPawn()->GetActorLocation(), ParticipantOf(State, 1)->GetPawn()->GetActorLocation());
					ASSERT_THAT(IsTrue(Apart >= BlastDistance + Recoil * RecoilShare, FString::Printf(TEXT("%.1f apart"), Apart)));
				});
		}

		TEST_METHOD(LastBroadsideLandsAfterItsCharge)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Prepare the duel"), [this](FState& State) { PrepareDuel(State, ShotDistance); })
				.ThenClient(TEXT("Plant and fire"), 0, [this](FState& State) { Duel.CastAtTheTarget(State.World, EVeyraAbilitySlot::R); })
				.UntilServer(TEXT("The shell lands"), [](FState& State) { return HealthLost(ParticipantOf(State, 1)) > 0.0; })
				.ThenServer(TEXT("On cooldown once fired"), [this](FState& State) {
					ASSERT_THAT(IsTrue(ParticipantOf(State, 0)->FindComponentByClass<UVeyraCooldownComponent>()->GetRemainingSecondsNow(ContentId(TEXT("bryn_last_broadside"))) > 0.0));
				});
		}
	};
}

#endif // ENABLE_PIE_NETWORK_TEST
