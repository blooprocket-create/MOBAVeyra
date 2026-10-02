// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"

#if ENABLE_PIE_NETWORK_TEST

#include "AbilitySystemComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "Attacks/VeyraBasicAttackComponent.h"
#include "Attributes/VeyraVitalsSet.h"
#include "GameFramework/PlayerState.h"
#include "Targeting/VeyraTargeting.h"
#include "Tests/Combat/VeyraCombatTestHelpers.h"
#include "Tests/Net/VeyraMatchNetTestHelpers.h"
#include "Tests/Net/VeyraNetTestHelpers.h"
#include "Tuning/VeyraCombatTuningSubsystem.h"
#include "VeyraCombatVerbs.h"
#include "VeyraPlayerState.h"
#include "VeyraVanguardController.h"

namespace VeyraNetTests
{
	// Veyra.Net.AttackOrders.*: attack and attack-move orders chase into range and attack on the
	// server's clock; every client sees the attack and its damage (Combat Bible §4, §48; ADR-009 §5).
	// Named for the orders, since test class names must be unique and Veyra.Abilities.BasicAttack exists.
	NETWORK_TEST_CLASS(AttackOrders, "Veyra.Net")
	{
		struct FState : public FBasePIENetworkComponentState
		{
		};

		FPIENetworkComponent<FState> Network{ TestRunner, TestCommandBuilder, bInitializing };
		TUniquePtr<FScopedExpectedPlayers> ExpectedPlayers;
		TUniquePtr<FScopedMatchTuning> Tuning;
		FVeyraGreyboxLayout Layout;

		// Fixture values: a short preparation, a melee attack, and how far apart the Vanguards start
		// the test, so the chase is short.
		static constexpr double ShortPreparationSeconds = 0.1;
		static constexpr double Range = 150.0;
		static constexpr double WindupFraction = 0.25;
		static constexpr double AcquisitionRadius = 800.0;
		static constexpr double Apart = 700.0;

		int32 AttackerId = INDEX_NONE;
		int32 TargetId = INDEX_NONE;
		/** A second enemy, for the attack-move preference. */
		AVeyraPlayerState* Decoy = nullptr;

		BEFORE_EACH()
		{
			IgnoreKnownIrisWarnings(*TestRunner);
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

		static AVeyraPlayerState* ParticipantOf(FState& State, int32 ClientIndex)
		{
			const AVeyraPlayerController* Controller = ServerControllerOf(State, ClientIndex);
			return Controller ? Controller->GetPlayerState<AVeyraPlayerState>() : nullptr;
		}

		static double BaseDamage()
		{
			return VeyraCombatTests::ExampleStats().PhysicalPower;
		}

		/**
		 * On the server: gives both Vanguards the example stats and a melee attack, and brings the
		 * second within a short walk of the first, toward the lane's centre.
		 */
		void PrepareDuel(FState& State)
		{
			AVeyraPlayerState* Attacker = ParticipantOf(State, 0);
			AVeyraPlayerState* Target = ParticipantOf(State, 1);
			ASSERT_THAT(IsTrue(Attacker && Target && Attacker->GetPawn() && Target->GetPawn()));
			FVeyraBasicAttackProfile Melee;
			Melee.Range = Range;
			// True damage, so every machine can compare exact amounts.
			Melee.DamageType = EVeyraDamageType::TrueDamage;
			Melee.PhysicalPowerRatio = 1.0;
			Melee.WindupFraction = WindupFraction;
			Melee.AcquisitionRadius = AcquisitionRadius;
			for (AVeyraPlayerState* Participant : { Attacker, Target })
			{
				ASSERT_THAT(IsTrue(VeyraCombat::InitializeStats(*Participant->GetAbilitySystemComponent(), VeyraCombatTests::ExampleStats())));
				ASSERT_THAT(IsTrue(Participant->FindComponentByClass<UVeyraBasicAttackComponent>()->SetProfile(Melee)));
			}
			const FVector From = Attacker->GetPawn()->GetActorLocation();
			const FVector TowardCentre = FVector(-From.X, 0.0, 0.0).GetSafeNormal();
			Target->GetPawn()->SetActorLocation(From + TowardCentre * Apart);
			AttackerId = Attacker->GetPlayerId();
			TargetId = Target->GetPlayerId();
		}

		/** The participant with PlayerId as this machine sees it. */
		static const APlayerState* SeenParticipant(FState& State, int32 PlayerId)
		{
			for (const APlayerState* Participant : GameStateOf(State.World)->PlayerArray)
			{
				if (Participant && Participant->GetPlayerId() == PlayerId)
				{
					return Participant;
				}
			}
			return nullptr;
		}

		static double HealthLost(const APlayerState* Participant)
		{
			const UAbilitySystemComponent& Unit = *CastChecked<AVeyraPlayerState>(Participant)->GetAbilitySystemComponent();
			return Unit.GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute()) - Unit.GetNumericAttribute(UVeyraVitalsSet::GetHealthAttribute());
		}

		TEST_METHOD(AnAttackOrderChasesIntoRangeAndHitsForEveryone)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Prepare the duel"), [this](FState& State) { PrepareDuel(State); })
				.UntilClient(TEXT("The attacker's client sees the target close by"), 0, [this](FState& State) {
					const AVeyraVanguardCharacter* Attacker = FindVanguard(State.World, AttackerId);
					const AVeyraVanguardCharacter* Target = FindVanguard(State.World, TargetId);
					return Attacker && Target && FVector::Dist2D(Attacker->GetActorLocation(), Target->GetActorLocation()) <= Apart + 1.0;
				})
				.ThenClient(TEXT("Order the attack"), 0, [this](FState& State) { LocalControllerOf(State.World)->IssueAttackOrder(FindVanguard(State.World, TargetId)); })
				.UntilServer(TEXT("The attack lands"), [](FState& State) { return HealthLost(ParticipantOf(State, 1)) > 0.0; })
				.ThenServer(TEXT("In range, with one attack's damage"), [this](FState& State) {
					const APawn* Attacker = ParticipantOf(State, 0)->GetPawn();
					const APawn* Target = ParticipantOf(State, 1)->GetPawn();
					ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(HealthLost(ParticipantOf(State, 1)), BaseDamage(), 1e-3)));
					ASSERT_THAT(IsTrue(VeyraTargeting::EdgeToEdgeDistance(*Attacker, *Target) <= Range + UVeyraCombatTuningSubsystem::Get().Targeting.ServerRangeTolerance));
					ASSERT_THAT(IsTrue(ParticipantOf(State, 0)->GetVanguardController()->GetAttackTarget() == Target, TEXT("the order keeps attacking")));
				})
				.UntilClients(TEXT("Every client sees the damage, the attack and its reach"), [this](FState& State) {
					const APlayerState* Attacker = SeenParticipant(State, AttackerId);
					const APlayerState* Target = SeenParticipant(State, TargetId);
					// The reach replicates, so Show Attack Range rings it on the client (ADR-052 §4).
					return Attacker && Target && HealthLost(Target) >= BaseDamage()
						&& Attacker->FindComponentByClass<UVeyraBasicAttackComponent>()->GetState().Target != nullptr
						&& FMath::IsNearlyEqual(Attacker->FindComponentByClass<UVeyraBasicAttackComponent>()->GetRange(nullptr), Range);
				})
				.UntilServer(TEXT("It attacks again after its interval"), [](FState& State) {
					return HealthLost(ParticipantOf(State, 1)) >= 2.0 * BaseDamage();
				})
				.ThenClient(TEXT("A move order ends the attack order"), 0, [](FState& State) { LocalControllerOf(State.World)->IssueMoveOrder(FVector::ZeroVector); })
				.UntilServer(TEXT("The server drops the attack order"), [](FState& State) {
					return ParticipantOf(State, 0)->GetVanguardController()->GetAttackTarget() == nullptr;
				});
		}

		TEST_METHOD(AChaseALockStopsGoesOnOnceItEnds)
		{
			// Fixture value: a stun shorter than the chase.
			static constexpr double BriefSeconds = 0.3;
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Prepare the duel"), [this](FState& State) { PrepareDuel(State); })
				.ThenServer(TEXT("Chase, and be stunned on the way"), [this](FState& State) {
					AVeyraPlayerState* Attacker = ParticipantOf(State, 0);
					AVeyraVanguardController* Controller = Attacker->GetVanguardController();
					ASSERT_THAT(IsTrue(Controller->AttackUnit(*ParticipantOf(State, 1)->GetPawn()) == EVeyraOrderRejection::None));
					ASSERT_THAT(IsTrue(Controller->GetPathFollowingComponent()->GetStatus() == EPathFollowingStatus::Moving, TEXT("it chases")));
					FVeyraStatusSpec Stun;
					Stun.Id = FVeyraContentId::FromText(TEXT("test_brief_stun")).GetValue();
					Stun.Kind = EVeyraStatusKind::Stun;
					Stun.DurationSeconds = BriefSeconds;
					ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*ParticipantOf(State, 1)->GetAbilitySystemComponent(), *Attacker->GetAbilitySystemComponent(), Stun)));
					ASSERT_THAT(IsTrue(Controller->GetPathFollowingComponent()->GetStatus() == EPathFollowingStatus::Idle, TEXT("the stun stops its path")));
				})
				.UntilServer(TEXT("Free again, it chases on and hits"), [](FState& State) { return HealthLost(ParticipantOf(State, 1)) > 0.0; });
		}

		TEST_METHOD(AMoveOrderEndsAWindupUnlessTheAttackerIsMobile)
		{
			// Fixture values: the target within reach, a step aside, and a passive's half of the speed.
			static constexpr double Aside = 100.0;
			static constexpr double Half = 0.5;
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Prepare the duel, within reach"), [this](FState& State) {
					PrepareDuel(State);
					const APawn* Body = ParticipantOf(State, 0)->GetPawn();
					ParticipantOf(State, 1)->GetPawn()->SetActorLocation(Body->GetActorLocation() + FVector(Range, 0.0, 0.0));
				})
				.ThenServer(TEXT("Attack, then walk aside"), [this](FState& State) {
					AVeyraPlayerState* Attacker = ParticipantOf(State, 0);
					AVeyraVanguardController* Controller = Attacker->GetVanguardController();
					UVeyraBasicAttackComponent* Attacks = Attacker->FindComponentByClass<UVeyraBasicAttackComponent>();
					APawn* Target = ParticipantOf(State, 1)->GetPawn();
					const FVector Step = Attacker->GetPawn()->GetActorLocation() + FVector(0.0, Aside, 0.0);
					ASSERT_THAT(IsTrue(Controller->AttackUnit(*Target) == EVeyraOrderRejection::None && Attacks->GetState().Phase == EVeyraAttackPhase::Windup));
					Controller->MoveToDestination(Step);
					ASSERT_THAT(IsTrue(Attacks->GetState().Phase == EVeyraAttackPhase::None, TEXT("a standing attacker's windup ends with the move (§48)")));
					Attacks->SetWindupMovement(Half);
					ASSERT_THAT(IsTrue(Controller->AttackUnit(*Target) == EVeyraOrderRejection::None && Attacks->GetState().Phase == EVeyraAttackPhase::Windup));
					Controller->MoveToDestination(Step);
					ASSERT_THAT(IsTrue(Attacks->GetState().Phase == EVeyraAttackPhase::Windup, TEXT("a mobile attacker's windup goes on (ADR-027 §1)")));
					// An attack-move walks on through it too, from a standstill.
					Controller->StopMovement();
					Controller->AttackMoveTo(Step + FVector(0.0, Aside, 0.0));
					ASSERT_THAT(IsTrue(Attacks->GetState().Phase == EVeyraAttackPhase::Windup
						&& Controller->GetPathFollowingComponent()->GetStatus() == EPathFollowingStatus::Moving, TEXT("an attack-move walks during a mobile windup")));
				})
				.UntilServer(TEXT("Its attack lands as it walks"), [](FState& State) { return HealthLost(ParticipantOf(State, 1)) > 0.0; });
		}

		TEST_METHOD(AttackMoveTakesTheEnemyItsPreferenceNames)
		{
			// Fixture value: a decoy enemy nearer the attacker than the target, off to one side.
			constexpr double DecoyAside = 400.0;
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Prepare the duel and a decoy nearer the attacker"), [this, DecoyAside](FState& State) {
					PrepareDuel(State);
					AVeyraPlayerState* Attacker = ParticipantOf(State, 0);
					Decoy = GameModeOf(State.World)->AddBotParticipant(TEXT("Decoy"), ParticipantOf(State, 1)->GetVeyraTeam());
					ASSERT_THAT(IsTrue(Decoy && Decoy->GetPawn()));
					Decoy->GetPawn()->SetActorLocation(Attacker->GetPawn()->GetActorLocation() + FVector(0.0, DecoyAside, 0.0));
				})
				.UntilServer(TEXT("The attacker can take on both"), [this](FState& State) {
					const APawn* Body = ParticipantOf(State, 0)->GetPawn();
					return VeyraTargeting::CanAcquire(Body, *Decoy->GetPawn()) && VeyraTargeting::CanAcquire(Body, *ParticipantOf(State, 1)->GetPawn());
				})
				.ThenServer(TEXT("Attack-move onto the target, closest to the Vanguard"), [this](FState& State) {
					// The click is on the target; the decoy stands nearer the attacker.
					ASSERT_THAT(IsTrue(ParticipantOf(State, 0)->GetVanguardController()->AttackMoveTo(ParticipantOf(State, 1)->GetPawn()->GetActorLocation(),
						EVeyraAttackMoveTarget::ClosestToVanguard) == EVeyraOrderRejection::None));
				})
				.UntilServer(TEXT("It takes the decoy first"), [this](FState& State) {
					return ParticipantOf(State, 0)->GetVanguardController()->GetAttackTarget() == Decoy->GetPawn();
				})
				.ThenServer(TEXT("The same order, closest to the cursor"), [this](FState& State) {
					AVeyraVanguardController* Controller = ParticipantOf(State, 0)->GetVanguardController();
					Controller->StopOrders();
					ASSERT_THAT(IsTrue(Controller->AttackMoveTo(ParticipantOf(State, 1)->GetPawn()->GetActorLocation(), EVeyraAttackMoveTarget::ClosestToCursor)
						== EVeyraOrderRejection::None));
				})
				.UntilServer(TEXT("It takes the one clicked on first"), [this](FState& State) {
					return ParticipantOf(State, 0)->GetVanguardController()->GetAttackTarget() == ParticipantOf(State, 1)->GetPawn();
				});
		}

		TEST_METHOD(AttackMoveTakesOnAnEnemyItMeets)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Prepare the duel"), [this](FState& State) { PrepareDuel(State); })
				.ThenClient(TEXT("Attack-move to the lane's centre"), 0, [](FState& State) {
					LocalControllerOf(State.World)->IssueAttackMoveOrder(FVector::ZeroVector);
				})
				.UntilServer(TEXT("It takes on the enemy and hits it"), [](FState& State) {
					const AVeyraVanguardController* Controller = ParticipantOf(State, 0)->GetVanguardController();
					return Controller->GetAttackTarget() == ParticipantOf(State, 1)->GetPawn() && HealthLost(ParticipantOf(State, 1)) > 0.0;
				})
				.ThenServer(TEXT("Still an attack-move"), [this](FState& State) {
					ASSERT_THAT(IsTrue(ParticipantOf(State, 0)->GetVanguardController()->GetAttackMoveDestination().IsSet()));
				})
				.UntilClients(TEXT("Every client sees the damage"), [this](FState& State) {
					const APlayerState* Target = SeenParticipant(State, TargetId);
					return Target && HealthLost(Target) >= BaseDamage();
				});
		}
	};
}

#endif // ENABLE_PIE_NETWORK_TEST
