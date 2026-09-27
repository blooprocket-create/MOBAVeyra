// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"

#if ENABLE_PIE_NETWORK_TEST

#include "AbilitySystemComponent.h"
#include "Attributes/VeyraVitalsSet.h"
#include "Delivery/VeyraProjectile.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"
#include "Loadout/VeyraAbilityLoadoutComponent.h"
#include "Movement/VeyraMovementComponent.h"
#include "Progression/VeyraProgressionComponent.h"
#include "Progression/VeyraProgressionTuningSubsystem.h"
#include "Statuses/VeyraStatusComponent.h"
#include "Targeting/VeyraTargeting.h"
#include "Tests/Abilities/VeyraTestFluxborn.h"
#include "Tests/Combat/VeyraCombatTestHelpers.h"
#include "Tests/Net/VeyraMatchNetTestHelpers.h"
#include "Tests/Net/VeyraNetTestHelpers.h"
#include "VeyraCombatVerbs.h"
#include "VeyraPlayerState.h"

namespace VeyraNetTests
{
	// Veyra.Net.Skillshot.*: skillshots fly on the server's ticks and dashes run until they meet an
	// enemy; every client sees the projectile and what it did (ADR-008 §3, §9; ADR-009 §4).
	NETWORK_TEST_CLASS(Skillshot, "Veyra.Net")
	{
		struct FState : public FBasePIENetworkComponentState
		{
		};

		FPIENetworkComponent<FState> Network{ TestRunner, TestCommandBuilder, bInitializing };
		TUniquePtr<FScopedExpectedPlayers> ExpectedPlayers;
		TUniquePtr<FScopedMatchTuning> Tuning;
		TUniquePtr<FScopedAbilitiesTuning> Abilities;
		FVeyraGreyboxLayout Layout;

		// Fixture values: a short preparation; a hook and a charge that reach across the lane, the hook
		// slowly enough for every client to see it fly; a push; and allowances for float positions.
		static constexpr double ShortPreparationSeconds = 0.1;
		static constexpr double Reach = 6000.0;
		static constexpr double ShotSpeed = 3000.0;
		static constexpr double ShotRadius = 40.0;
		static constexpr double Damage = 30.0;
		static constexpr double PullSpeed = 4000.0;
		static constexpr double PushDistance = 300.0;
		static constexpr double PushSpeed = 1500.0;
		static constexpr double ChargeSpeed = 5000.0;
		static constexpr double StunSeconds = 3.0;
		static constexpr double CooldownSeconds = 10.0;
		static constexpr double OffPath = 40.0;
		static constexpr double PositionSlack = 5.0;
		static constexpr double SameDirection = 0.999;

		int32 CasterId = INDEX_NONE;
		int32 TargetId = INDEX_NONE;
		FVector TargetPoint = FVector::ZeroVector;
		TWeakObjectPtr<AVeyraTestFluxborn> InThePath;
		FVector InThePathStart = FVector::ZeroVector;

		static FVeyraContentId Id(const TCHAR* Text)
		{
			return FVeyraContentId::FromText(Text).GetValue();
		}

		static FVeyraCastTuning CastAcrossTheLane()
		{
			FVeyraCastTuning Cast;
			Cast.CooldownSecondsByRank = { CooldownSeconds };
			Cast.ResourceCostByRank = { 0.0 };
			Cast.CastRange = Reach;
			return Cast;
		}

		BEFORE_EACH()
		{
			IgnoreLoginViewTargetRpc(*TestRunner);
			ASSERT_THAT(IsTrue(VeyraGreybox::LoadLayout(Layout).IsEmpty()));
			Tuning = MakeUnique<FScopedMatchTuning>();
			Tuning->Tuning.Phases.PreparationSeconds = ShortPreparationSeconds;

			// The committed abilities, so the developer loadout still has its own, plus the test ones.
			const FVeyraAbilitiesTuning Committed = UVeyraAbilitiesTuningSubsystem::Get();
			Abilities = MakeUnique<FScopedAbilitiesTuning>();
			Abilities->Tuning = Committed;
			FVeyraStatusTuning Stun;
			Stun.Kind = EVeyraStatusKind::Stun;
			Stun.DurationSeconds = StunSeconds;
			Abilities->Tuning.Statuses.Add(Id(TEXT("test_stun")), Stun);

			FVeyraSkillshotAbilityTuning Hook;
			Hook.Cast = CastAcrossTheLane();
			Hook.Projectile = FVeyraProjectileTuning{ ShotSpeed, ShotRadius, Reach };
			Hook.Collision = EVeyraSkillshotCollision::FirstEnemyVanguard;
			Hook.Effects.Damage.Add(FVeyraDamageTuning{ EVeyraDamageType::TrueDamage, { Damage }, 0.0, 0.0 });
			Hook.Effects.Displacement.Add(FVeyraDisplacementTuning{ EVeyraDisplacementDirection::TowardOrigin, Reach, PullSpeed });
			Hook.PassThroughEffects.Displacement.Add(FVeyraDisplacementTuning{ EVeyraDisplacementDirection::AsideFromPath, PushDistance, PushSpeed });
			Abilities->Tuning.Skillshot.Add(Id(TEXT("test_hook")), Hook);

			FVeyraDashAbilityTuning Charge;
			Charge.Cast = CastAcrossTheLane();
			Charge.Direction = EVeyraDashDirection::TowardPoint;
			Charge.Distance = Reach;
			Charge.Speed = ChargeSpeed;
			Charge.Contact = EVeyraDashContact::StopAtFirstEnemy;
			Charge.ContactEffects.Damage.Add(FVeyraDamageTuning{ EVeyraDamageType::TrueDamage, { Damage }, 0.0, 0.0 });
			Charge.ContactEffects.Statuses.Add(Id(TEXT("test_stun")));
			Abilities->Tuning.Dash.Add(Id(TEXT("test_charge")), Charge);

			ExpectedPlayers = MakeUnique<FScopedExpectedPlayers>(MatchClientCount);
			BuildMatchNetwork(Network);
		}

		AFTER_EACH()
		{
			Abilities.Reset();
			Tuning.Reset();
			ExpectedPlayers.Reset();
		}

		static AVeyraPlayerState* ParticipantOf(FState& State, int32 ClientIndex)
		{
			const AVeyraPlayerController* Controller = ServerControllerOf(State, ClientIndex);
			return Controller ? Controller->GetPlayerState<AVeyraPlayerState>() : nullptr;
		}

		/** On the server: gives the first client's Vanguard Ability in W, with a level's skill point to learn it. */
		void LearnW(FState& State, const TCHAR* Ability)
		{
			AVeyraPlayerState* Caster = ParticipantOf(State, 0);
			AVeyraPlayerState* Target = ParticipantOf(State, 1);
			ASSERT_THAT(IsTrue(Caster && Target && Caster->GetPawn() && Target->GetPawn()));
			UVeyraProgressionComponent* Progression = Caster->FindComponentByClass<UVeyraProgressionComponent>();
			ASSERT_THAT(IsTrue(Caster->FindComponentByClass<UVeyraAbilityLoadoutComponent>()->Grant(*Caster->GetAbilitySystemComponent(), EVeyraAbilitySlot::W, Id(Ability))));
			Progression->AddExperience(UVeyraProgressionTuningSubsystem::Get().Experience.ToNextLevel[0]);
			ASSERT_THAT(IsTrue(Progression->AllocateRank(EVeyraAbilitySlot::W) == EVeyraRankRefusal::None));
			CasterId = Caster->GetPlayerId();
			TargetId = Target->GetPlayerId();
			TargetPoint = Target->GetPawn()->GetActorLocation();
		}

		/** On the server: puts a unit of the target's side just beside the path between the two Vanguards. */
		void PutAUnitInThePath(FState& State)
		{
			const FVector From = ParticipantOf(State, 0)->GetPawn()->GetActorLocation();
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
			AVeyraTestFluxborn* Unit = State.World->SpawnActor<AVeyraTestFluxborn>(AVeyraTestFluxborn::StaticClass(),
				FTransform((From + TargetPoint) / 2.0 + FVector(0.0, OffPath, 0.0)), Params);
			ASSERT_THAT(IsNotNull(Unit));
			Unit->SetVeyraTeam(VeyraTeams::TeamOf(ParticipantOf(State, 1)));
			ASSERT_THAT(IsTrue(VeyraCombat::InitializeStats(*Unit->GetAbilitySystemComponent(), VeyraCombatTests::ExampleStats())));
			Unit->GetVeyraMovement()->SetMovementMode(MOVE_Walking);
			InThePath = Unit;
			InThePathStart = Unit->GetActorLocation();
		}

		void CastWAtTheTarget(FState& State) const
		{
			FVeyraCastTarget Target;
			Target.bHasLocation = true;
			Target.Location = TargetPoint;
			LocalControllerOf(State.World)->IssueCastOrder(EVeyraAbilitySlot::W, Target);
		}

		static double HealthLost(const AActor* Unit)
		{
			const IAbilitySystemInterface* Owner = Cast<IAbilitySystemInterface>(Unit);
			const UAbilitySystemComponent& AbilitySystem = *Owner->GetAbilitySystemComponent();
			return AbilitySystem.GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute()) - AbilitySystem.GetNumericAttribute(UVeyraVitalsSet::GetHealthAttribute());
		}

		/** Whether the two Vanguards, as this machine sees them, are touching. */
		bool SeesVanguardsTouching(FState& State) const
		{
			const AVeyraVanguardCharacter* Caster = FindVanguard(State.World, CasterId);
			const AVeyraVanguardCharacter* Target = FindVanguard(State.World, TargetId);
			return Caster && Target && VeyraTargeting::EdgeToEdgeDistance(*Caster, *Target) <= PositionSlack;
		}

		/** The target participant's statuses as this machine sees them include a Stun. */
		bool SeesTargetStunned(FState& State) const
		{
			for (const APlayerState* Participant : GameStateOf(State.World)->PlayerArray)
			{
				if (Participant && Participant->GetPlayerId() == TargetId)
				{
					return Participant->FindComponentByClass<UVeyraStatusComponent>()->GetLedger().Entries.ContainsByPredicate(
						[](const FVeyraStatusEntry& Entry) { return Entry.Kind == EVeyraStatusKind::Stun; });
				}
			}
			return false;
		}

		TEST_METHOD(AHookPushesAUnitAsideAndPullsTheVanguardBehindIt)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Learn the hook, with a unit in its path"), [this](FState& State) {
					LearnW(State, TEXT("test_hook"));
					PutAUnitInThePath(State);
				})
				.ThenClient(TEXT("Cast it at the enemy"), 0, [this](FState& State) { CastWAtTheTarget(State); })
				.UntilClients(TEXT("Every client sees it launched at the enemy"), [this](FState& State) {
					for (TActorIterator<AVeyraProjectile> It(State.World); It; ++It)
					{
						const FVector TowardTarget = (TargetPoint - It->GetLaunchedFrom()).GetSafeNormal2D();
						if (It->GetFlight() == EVeyraProjectileFlight::Line && It->GetRadius() == ShotRadius && It->GetSpeed() == ShotSpeed
							&& It->GetRange() == Reach && FVector::DotProduct(It->GetDirection(), TowardTarget) >= SameDirection)
						{
							return true;
						}
					}
					return false;
				})
				.UntilServer(TEXT("It hits the enemy Vanguard"), [](FState& State) { return HealthLost(ParticipantOf(State, 1)) == Damage; })
				.ThenServer(TEXT("It passed through the unit without harming it"), [this](FState&) {
					ASSERT_THAT(IsTrue(InThePath.IsValid() && HealthLost(InThePath.Get()) == 0.0));
				})
				.UntilServer(TEXT("The unit lands out of the path"), [this](FState&) {
					const AVeyraTestFluxborn* Unit = InThePath.Get();
					return Unit && !Unit->GetVeyraMovement()->IsDisplaced() && Unit->GetActorLocation().Y >= InThePathStart.Y + PushDistance - PositionSlack;
				})
				.UntilClients(TEXT("Every client sees the enemy pulled to the caster"), [this](FState& State) { return SeesVanguardsTouching(State); });
		}

		TEST_METHOD(AChargeStopsAtTheFirstEnemyAndHitsIt)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Learn the charge"), [this](FState& State) { LearnW(State, TEXT("test_charge")); })
				.ThenClient(TEXT("Charge at the enemy"), 0, [this](FState& State) { CastWAtTheTarget(State); })
				.UntilServer(TEXT("It reaches the enemy"), [](FState& State) { return HealthLost(ParticipantOf(State, 1)) == Damage; })
				.ThenServer(TEXT("And stops there"), [this](FState& State) {
					ASSERT_THAT(IsTrue(SeesVanguardsTouching(State)));
					ASSERT_THAT(IsFalse(ParticipantOf(State, 0)->GetPawn()->FindComponentByClass<UVeyraMovementComponent>()->IsDashing()));
				})
				.UntilClients(TEXT("Every client sees the enemy stunned"), [this](FState& State) { return SeesTargetStunned(State); });
		}
	};
}

#endif // ENABLE_PIE_NETWORK_TEST
