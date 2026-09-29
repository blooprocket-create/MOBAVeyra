// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"

#if ENABLE_PIE_NETWORK_TEST

#include "Delivery/VeyraEffectDelivery.h"
#include "Movement/VeyraMovementComponent.h"
#include "Targeting/VeyraTargeting.h"
#include "Tests/Net/VeyraMatchNetTestHelpers.h"
#include "Tests/Net/VeyraNetTestHelpers.h"
#include "Tests/Net/VeyraVanguardNetTestHelpers.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"

namespace VeyraNetTests
{
	// Veyra.Net.Vanguards.Qazharr.*: Qazharr's kit in a match (Character Bible §13): Sea Dog building
	// over his attacks, Heavy Hand's empowered attack, Weather It, Boarding Rush's charge and No
	// Quarter's sweep and state. Both players are Qazharr at level 6 with a rank in each ability;
	// amounts come from the tuning.
	NETWORK_TEST_CLASS(Qazharr, "Veyra.Net.Vanguards")
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
		// apart the Vanguards stand for each ability, and allowances for float positions and for how
		// much of a knockback counts as having happened.
		static constexpr double ShortPreparationSeconds = 0.1;
		static constexpr int32 UltimateLevel = 6;
		static constexpr double MeleeDistance = 150.0;
		static constexpr double ChargeDistance = 400.0;
		static constexpr double SweepDistance = 250.0;
		static constexpr double PositionSlack = 5.0;
		static constexpr double KnockbackShare = 0.5;
		static constexpr double Tolerance = 1e-2;
		static constexpr int32 ChainHits = 2;

		BEFORE_EACH()
		{
			IgnoreKnownIrisWarnings(*TestRunner);
			ASSERT_THAT(IsTrue(VeyraGreybox::LoadLayout(Layout).IsEmpty()));
			Tuning = MakeUnique<FScopedMatchTuning>();
			Tuning->Tuning.Phases.PreparationSeconds = ShortPreparationSeconds;
			Tuning->Tuning.DeveloperMatch.Vanguards = { ContentId(TEXT("qazharr")) };
			Tuning->Tuning.DeveloperMatch.StartingRank = EVeyraDeveloperStartingRank::None;
			ExpectedPlayers = MakeUnique<FScopedExpectedPlayers>(MatchClientCount);
			BuildMatchNetwork(Network);
		}

		AFTER_EACH()
		{
			Tuning.Reset();
			ExpectedPlayers.Reset();
		}

		/** On the server: both reach the ultimate's level and rank every ability; the second stands Distance away. */
		void PrepareDuel(FState& State, double Distance)
		{
			ASSERT_THAT(IsTrue(Duel.Prepare(State, ContentId(TEXT("qazharr")), UltimateLevel, Distance)));
		}

		void AttackTheTarget(FState& State) const
		{
			LocalControllerOf(State.World)->IssueAttackOrder(FindVanguard(State.World, Duel.TargetId));
		}

		static FVeyraContentId SeaDog()
		{
			return UVeyraVanguardsTuningSubsystem::FindHitChain(ContentId(TEXT("qazharr_sea_dog")))->Status;
		}

		TEST_METHOD(SeaDogBuildsWithEachAttackOnTheSameEnemy)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Prepare the duel"), [this](FState& State) { PrepareDuel(State, MeleeDistance); })
				.ThenClient(TEXT("Attack the enemy"), 0, [this](FState& State) { AttackTheTarget(State); })
				.UntilClients(TEXT("Every client sees Sea Dog's stacks grow"), [this](FState& State) {
					const FVeyraStatusEntry* Entry = SeenStatus(State.World, Duel.CasterId, SeaDog());
					return Entry && Entry->Stacks >= ChainHits;
				});
		}

		TEST_METHOD(HeavyHandEmpowersTheNextAttackToSlow)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Prepare the duel"), [this](FState& State) { PrepareDuel(State, MeleeDistance); })
				.ThenClient(TEXT("Empower the next attack"), 0, [this](FState& State) { Duel.CastAtTheTarget(State.World, EVeyraAbilitySlot::Q); })
				.ThenClient(TEXT("Attack the enemy"), 0, [this](FState& State) { AttackTheTarget(State); })
				.UntilClients(TEXT("Every client sees the enemy slowed"), [this](FState& State) { return SeesStatus(State.World, Duel.TargetId, EVeyraStatusKind::Slow); })
				.ThenServer(TEXT("The heavy hit landed"), [this](FState& State) { ASSERT_THAT(IsTrue(HealthLost(ParticipantOf(State, 1)) > 0.0)); });
		}

		TEST_METHOD(WeatherItShieldsAndSteadiesQazharr)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Prepare the duel"), [this](FState& State) { PrepareDuel(State, ChargeDistance); })
				.ThenClient(TEXT("Weather it"), 0, [this](FState& State) { Duel.CastAtTheTarget(State.World, EVeyraAbilitySlot::W); })
				.UntilClients(TEXT("Every client sees his Tenacity"), [this](FState& State) { return SeesStatus(State.World, Duel.CasterId, EVeyraStatusKind::Tenacity); })
				.ThenServer(TEXT("With the rank's shield"), [this](FState& State) {
					const AVeyraPlayerState* Caster = ParticipantOf(State, 0);
					const FVeyraShieldTuning& Shield = UVeyraAbilitiesTuningSubsystem::Get().SelfBuff.FindChecked(ContentId(TEXT("qazharr_weather_it"))).Shields[0];
					const double Expected = VeyraEffectDelivery::ShieldGrant(*Caster->GetAbilitySystemComponent(), Shield, 1).Amount;
					const TArray<FVeyraShieldEntry>& Shields = ShieldsOf(Caster);
					ASSERT_THAT(IsTrue(Shields.Num() == 1 && FMath::IsNearlyEqual(Shields[0].Remaining, Expected, Tolerance)));
				});
		}

		TEST_METHOD(BoardingRushChargesIntoTheEnemy)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Prepare the duel"), [this](FState& State) { PrepareDuel(State, ChargeDistance); })
				.ThenClient(TEXT("Charge"), 0, [this](FState& State) { Duel.CastAtTheTarget(State.World, EVeyraAbilitySlot::E); })
				.UntilServer(TEXT("The shoulder check lands"), [](FState& State) { return HealthLost(ParticipantOf(State, 1)) > 0.0; })
				.UntilClients(TEXT("Every client sees him against the enemy, quickened"), [this](FState& State) {
					const AVeyraVanguardCharacter* Caster = FindVanguard(State.World, Duel.CasterId);
					const AVeyraVanguardCharacter* Target = FindVanguard(State.World, Duel.TargetId);
					return Caster && Target && VeyraTargeting::EdgeToEdgeDistance(*Caster, *Target) <= PositionSlack
						&& SeesStatus(State.World, Duel.CasterId, EVeyraStatusKind::MoveSpeed);
				});
		}

		TEST_METHOD(NoQuarterSweepsTheEnemyAsideAndSendsHimIntoTheState)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Prepare the duel"), [this](FState& State) { PrepareDuel(State, SweepDistance); })
				.ThenClient(TEXT("Sweep"), 0, [this](FState& State) { Duel.CastAtTheTarget(State.World, EVeyraAbilitySlot::R); })
				.UntilClients(TEXT("Every client sees him in No Quarter"), [this](FState& State) {
					return SeesStatus(State.World, Duel.CasterId, EVeyraStatusKind::AttackSpeed) && SeesStatus(State.World, Duel.CasterId, EVeyraStatusKind::Tenacity)
						&& SeesStatus(State.World, Duel.CasterId, EVeyraStatusKind::AttackCleave)
						&& SeesStatus(State.World, Duel.CasterId, EVeyraStatusKind::MoveSpeedTowardEnemyVanguards);
				})
				.UntilServer(TEXT("The knockback ends"), [](FState& State) {
					return HealthLost(ParticipantOf(State, 1)) > 0.0 && !ParticipantOf(State, 1)->GetPawn()->FindComponentByClass<UVeyraMovementComponent>()->IsDisplaced();
				})
				.ThenServer(TEXT("The enemy was thrown sideways to the swing"), [this](FState& State) {
					const FVeyraAreaAbilityTuning& Sweep = *UVeyraAbilitiesTuningSubsystem::FindArea(ContentId(TEXT("qazharr_no_quarter")));
					const double Knockback = Sweep.Zones[0].Effects.Displacement[0].Distance;
					const FVector Cast = (Duel.TargetPoint - ParticipantOf(State, 0)->GetPawn()->GetActorLocation()).GetSafeNormal2D();
					const FVector Moved = ParticipantOf(State, 1)->GetPawn()->GetActorLocation() - Duel.TargetPoint;
					const double Sideways = FMath::Abs(FVector::CrossProduct(Cast, Moved).Z);
					ASSERT_THAT(IsTrue(Sideways >= Knockback * KnockbackShare, FString::Printf(TEXT("moved %.1f sideways of %.1f"), Sideways, Knockback)));
				});
		}
	};
}

#endif // ENABLE_PIE_NETWORK_TEST
