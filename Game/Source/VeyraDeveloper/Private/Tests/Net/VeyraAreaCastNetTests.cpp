// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"

#if ENABLE_PIE_NETWORK_TEST

#include "AbilitySystemComponent.h"
#include "Attributes/VeyraResourceSet.h"
#include "Attributes/VeyraVitalsSet.h"
#include "Casting/VeyraCastStateComponent.h"
#include "Cooldowns/VeyraCooldownComponent.h"
#include "Delivery/VeyraDelayedArea.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"
#include "Loadout/VeyraAbilityLoadoutComponent.h"
#include "Movement/VeyraMovementComponent.h"
#include "Progression/VeyraProgressionComponent.h"
#include "Progression/VeyraProgressionTuningSubsystem.h"
#include "Statuses/VeyraStatusComponent.h"
#include "Tests/Net/VeyraMatchNetTestHelpers.h"
#include "Tests/Net/VeyraNetTestHelpers.h"
#include "VeyraCombatVerbs.h"
#include "VeyraPlayerState.h"

namespace VeyraNetTests
{
	// Veyra.Net.AreaCast.*: area casts run through their phases on the server, on world time, and
	// every client sees the telegraph and the result (Combat Bible §26, §48; ADR-008 §3, §4).
	NETWORK_TEST_CLASS(AreaCast, "Veyra.Net")
	{
		struct FState : public FBasePIENetworkComponentState
		{
		};

		FPIENetworkComponent<FState> Network{ TestRunner, TestCommandBuilder, bInitializing };
		TUniquePtr<FScopedExpectedPlayers> ExpectedPlayers;
		TUniquePtr<FScopedMatchTuning> Tuning;
		TUniquePtr<FScopedAbilitiesTuning> Abilities;
		FVeyraGreyboxLayout Layout;

		// Fixture values: a short preparation, and areas long enough to reach across the lane.
		static constexpr double ShortPreparationSeconds = 0.1;
		static constexpr double Reach = 6000.0;
		static constexpr double HitRadius = 200.0;
		static constexpr double Damage = 30.0;
		static constexpr double TickDamage = 10.0;
		static constexpr int32 Ticks = 3;
		static constexpr double PhaseSeconds = 0.5;
		static constexpr double CooldownSeconds = 10.0;
		static constexpr double ResourceCost = 40.0;
		static constexpr double StunSeconds = 2.0;

		int32 TargetId = INDEX_NONE;
		FVector TargetPoint = FVector::ZeroVector;
		double ResourceBefore = 0.0;
		double StunnedAt = 0.0;

		static FVeyraContentId Id(const TCHAR* Text)
		{
			return FVeyraContentId::FromText(Text).GetValue();
		}

		static FVeyraAreaAbilityTuning AreaAt(EVeyraAreaOrigin Origin, const FVeyraShape& Shape, double RawDamage)
		{
			FVeyraAreaAbilityTuning Area;
			Area.Cast.CooldownSecondsByRank = { CooldownSeconds };
			Area.Cast.ResourceCostByRank = { ResourceCost };
			Area.Cast.CastRange = Reach;
			Area.Origin = Origin;
			FVeyraAreaZoneTuning& Zone = Area.Zones.AddDefaulted_GetRef();
			Zone.Shape = Shape;
			Zone.Effects.Damage.Add(FVeyraDamageTuning{ EVeyraDamageType::TrueDamage, { RawDamage }, 0.0, 0.0 });
			return Area;
		}

		BEFORE_EACH()
		{
			IgnoreKnownIrisWarnings(*TestRunner);
			ASSERT_THAT(IsTrue(VeyraGreybox::LoadLayout(Layout).IsEmpty()));
			Tuning = MakeUnique<FScopedMatchTuning>();
			Tuning->Tuning.Phases.PreparationSeconds = ShortPreparationSeconds;

			// The committed abilities, so the developer loadout still has its own, plus the test areas.
			const FVeyraAbilitiesTuning Committed = UVeyraAbilitiesTuningSubsystem::Get();
			Abilities = MakeUnique<FScopedAbilitiesTuning>();
			Abilities->Tuning = Committed;
			FVeyraStatusTuning Stun;
			Stun.Kind = EVeyraStatusKind::Stun;
			Stun.DurationSeconds = StunSeconds;
			Abilities->Tuning.Statuses.Add(Id(TEXT("test_stun")), Stun);

			FVeyraShape Circle;
			Circle.Kind = EVeyraShapeKind::Circle;
			Circle.Radius = HitRadius;
			FVeyraAreaAbilityTuning Quake = AreaAt(EVeyraAreaOrigin::TargetPoint, Circle, Damage);
			Quake.Cast.WindupSeconds = PhaseSeconds;
			Quake.Cast.WindupMovement = EVeyraCastMovement::Locked;
			Quake.Zones[0].Effects.Statuses.Add(Id(TEXT("test_stun")));
			Abilities->Tuning.Area.Add(Id(TEXT("test_quake")), Quake);

			// The same, dearer and slower from its second rank on.
			FVeyraAreaAbilityTuning HeavyQuake = Quake;
			HeavyQuake.Cast.ResourceCostByRank = { ResourceCost, ResourceCost * 2.0, ResourceCost * 2.0, ResourceCost * 2.0, ResourceCost * 2.0 };
			HeavyQuake.Cast.CooldownSecondsByRank = { CooldownSeconds, CooldownSeconds * 2.0, CooldownSeconds * 2.0, CooldownSeconds * 2.0, CooldownSeconds * 2.0 };
			Abilities->Tuning.Area.Add(Id(TEXT("test_heavy_quake")), HeavyQuake);

			FVeyraAreaAbilityTuning Barrage = AreaAt(EVeyraAreaOrigin::TargetPoint, Circle, Damage);
			Barrage.DelaySeconds = PhaseSeconds;
			Abilities->Tuning.Area.Add(Id(TEXT("test_barrage")), Barrage);

			FVeyraShape Line;
			Line.Kind = EVeyraShapeKind::Rectangle;
			Line.Length = Reach;
			Line.Width = HitRadius;
			FVeyraAreaAbilityTuning Beam = AreaAt(EVeyraAreaOrigin::Caster, Line, TickDamage);
			Beam.ChannelTicks = Ticks;
			Beam.ChannelSeconds = PhaseSeconds;
			Abilities->Tuning.Area.Add(Id(TEXT("test_beam")), Beam);

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
			ASSERT_THAT(IsTrue(Caster && Target && Target->GetPawn()));
			UVeyraProgressionComponent* Progression = Caster->FindComponentByClass<UVeyraProgressionComponent>();
			ASSERT_THAT(IsTrue(Caster->FindComponentByClass<UVeyraAbilityLoadoutComponent>()->Grant(*Caster->GetAbilitySystemComponent(), EVeyraAbilitySlot::W, Id(Ability))));
			Progression->AddExperience(UVeyraProgressionTuningSubsystem::Get().Experience.ToNextLevel[0]);
			ASSERT_THAT(IsTrue(Progression->AllocateRank(EVeyraAbilitySlot::W) == EVeyraRankRefusal::None));
			TargetId = Target->GetPlayerId();
			TargetPoint = Target->GetPawn()->GetActorLocation();
			ResourceBefore = Caster->GetAbilitySystemComponent()->GetNumericAttribute(UVeyraResourceSet::GetResourceAttribute());
		}

		void CastWAtTheTarget(FState& State) const
		{
			FVeyraCastTarget Target;
			Target.bHasLocation = true;
			Target.Location = TargetPoint;
			LocalControllerOf(State.World)->IssueCastOrder(EVeyraAbilitySlot::W, Target);
		}

		static double HealthLost(const AVeyraPlayerState* Participant)
		{
			const UAbilitySystemComponent& Unit = *Participant->GetAbilitySystemComponent();
			return Unit.GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute()) - Unit.GetNumericAttribute(UVeyraVitalsSet::GetHealthAttribute());
		}

		static EVeyraCastPhase CasterPhase(FState& State)
		{
			return ParticipantOf(State, 0)->FindComponentByClass<UVeyraCastStateComponent>()->GetState().Phase;
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

		TEST_METHOD(AWindupCastIsTelegraphedThenHits)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Learn the windup area"), [this](FState& State) { LearnW(State, TEXT("test_quake")); })
				.ThenClient(TEXT("Cast it at the enemy"), 0, [this](FState& State) { CastWAtTheTarget(State); })
				.UntilClients(TEXT("Every client sees the windup"), [](FState& State) {
					for (const APlayerState* Participant : GameStateOf(State.World)->PlayerArray)
					{
						const UVeyraCastStateComponent* CastState = Participant ? Participant->FindComponentByClass<UVeyraCastStateComponent>() : nullptr;
						if (CastState && CastState->GetState().Phase == EVeyraCastPhase::Windup)
						{
							return true;
						}
					}
					return false;
				})
				.UntilServer(TEXT("It hits after the windup"), [](FState& State) { return HealthLost(ParticipantOf(State, 1)) > 0.0; })
				.ThenServer(TEXT("With its damage, cost and cooldown"), [this](FState& State) {
					const AVeyraPlayerState* Caster = ParticipantOf(State, 0);
					ASSERT_THAT(IsTrue(HealthLost(ParticipantOf(State, 1)) == Damage));
					ASSERT_THAT(IsTrue(Caster->GetAbilitySystemComponent()->GetNumericAttribute(UVeyraResourceSet::GetResourceAttribute()) <= ResourceBefore - ResourceCost));
					ASSERT_THAT(IsTrue(Caster->FindComponentByClass<UVeyraCooldownComponent>()->GetRemainingSecondsNow(Id(TEXT("test_quake"))) > 0.0));
					ASSERT_THAT(IsTrue(CasterPhase(State) == EVeyraCastPhase::None));
				})
				.UntilClients(TEXT("Every client sees the Stun"), [this](FState& State) { return SeesTargetStunned(State); });
		}

		TEST_METHOD(AnInterruptedWindupCostsNothingAndStartsPartOfTheCooldown)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Learn the windup area"), [this](FState& State) { LearnW(State, TEXT("test_quake")); })
				.ThenClient(TEXT("Cast it at the enemy"), 0, [this](FState& State) { CastWAtTheTarget(State); })
				.UntilServer(TEXT("The windup begins"), [](FState& State) { return CasterPhase(State) == EVeyraCastPhase::Windup; })
				.ThenServer(TEXT("Stun the caster"), [this](FState& State) {
					FVeyraStatusSpec Stun = UVeyraAbilitiesTuningSubsystem::FindStatus(Id(TEXT("test_stun"))).GetValue();
					UAbilitySystemComponent& Caster = *ParticipantOf(State, 0)->GetAbilitySystemComponent();
					ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*ParticipantOf(State, 1)->GetAbilitySystemComponent(), Caster, Stun)));
					ASSERT_THAT(IsTrue(CasterPhase(State) == EVeyraCastPhase::None, TEXT("the stun should end the cast at once")));
					StunnedAt = State.World->GetTimeSeconds();
				})
				.UntilServer(TEXT("Past when the windup would have ended"), [this](FState& State) {
					return State.World->GetTimeSeconds() >= StunnedAt + PhaseSeconds;
				})
				.ThenServer(TEXT("Nothing was paid or hit, and part of the cooldown runs"), [this](FState& State) {
					const AVeyraPlayerState* Caster = ParticipantOf(State, 0);
					ASSERT_THAT(IsTrue(HealthLost(ParticipantOf(State, 1)) == 0.0));
					ASSERT_THAT(IsTrue(Caster->GetAbilitySystemComponent()->GetNumericAttribute(UVeyraResourceSet::GetResourceAttribute()) >= ResourceBefore));
					const double Fraction = UVeyraAbilitiesTuningSubsystem::Get().Casting.InterruptedCooldownFraction;
					ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Caster->FindComponentByClass<UVeyraCooldownComponent>()->GetDurationSeconds(Id(TEXT("test_quake"))),
						CooldownSeconds * Fraction)));
				});
		}

		TEST_METHOD(ARankTakenDuringTheWindupCountsFromTheNextCast)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Learn the heavy windup area, with a level's point to spare"), [this](FState& State) {
					LearnW(State, TEXT("test_heavy_quake"));
					AVeyraPlayerState* Caster = ParticipantOf(State, 0);
					UVeyraProgressionComponent* Progression = Caster->FindComponentByClass<UVeyraProgressionComponent>();
					Progression->AddExperience(UVeyraProgressionTuningSubsystem::Get().Experience.ToNextLevel[Progression->GetLevel() - 1]);
					ASSERT_THAT(IsTrue(Progression->GetUnspentSkillPoints() > 0));
					ResourceBefore = Caster->GetAbilitySystemComponent()->GetNumericAttribute(UVeyraResourceSet::GetResourceAttribute());
				})
				.ThenClient(TEXT("Cast it at the enemy"), 0, [this](FState& State) { CastWAtTheTarget(State); })
				.UntilServer(TEXT("The windup begins"), [](FState& State) { return CasterPhase(State) == EVeyraCastPhase::Windup; })
				.ThenServer(TEXT("Take the second rank during it"), [this](FState& State) {
					UVeyraProgressionComponent* Progression = ParticipantOf(State, 0)->FindComponentByClass<UVeyraProgressionComponent>();
					ASSERT_THAT(IsTrue(Progression->AllocateRank(EVeyraAbilitySlot::W) == EVeyraRankRefusal::None));
					ASSERT_THAT(AreEqual(2, Progression->GetRank(EVeyraAbilitySlot::W)));
				})
				.UntilServer(TEXT("It hits after the windup"), [](FState& State) { return HealthLost(ParticipantOf(State, 1)) > 0.0; })
				.ThenServer(TEXT("Commit charged and cooled down at the rank the cast began with"), [this](FState& State) {
					// ADR-008 §6: a cast reads its rank when it begins. Resource may only have regenerated since.
					const AVeyraPlayerState* Caster = ParticipantOf(State, 0);
					const double Resource = Caster->GetAbilitySystemComponent()->GetNumericAttribute(UVeyraResourceSet::GetResourceAttribute());
					ASSERT_THAT(IsTrue(Resource >= ResourceBefore - ResourceCost - 1e-3, FString::Printf(TEXT("spent %g"), ResourceBefore - Resource)));
					ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Caster->FindComponentByClass<UVeyraCooldownComponent>()->GetDurationSeconds(Id(TEXT("test_heavy_quake"))),
						CooldownSeconds)));
				});
		}

		TEST_METHOD(ADelayedAreaIsTelegraphedThenHits)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Learn the delayed area"), [this](FState& State) { LearnW(State, TEXT("test_barrage")); })
				.ThenClient(TEXT("Cast it at the enemy"), 0, [this](FState& State) { CastWAtTheTarget(State); })
				.UntilClients(TEXT("Every client sees where it will land"), [this](FState& State) {
					TActorIterator<AVeyraDelayedArea> Delayed(State.World);
					return Delayed && Delayed->GetShapes().Num() == 1 && FVector::Dist2D(Delayed->GetActorLocation(), TargetPoint) < 1.0;
				})
				.UntilServer(TEXT("It lands"), [](FState& State) { return HealthLost(ParticipantOf(State, 1)) == Damage; })
				.UntilClients(TEXT("The telegraph goes"), [](FState& State) { return !TActorIterator<AVeyraDelayedArea>(State.World); });
		}

		TEST_METHOD(AChannelHitsOnEveryTickWithTheCasterHeld)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Learn the channelled beam"), [this](FState& State) { LearnW(State, TEXT("test_beam")); })
				.ThenClient(TEXT("Aim it at the enemy"), 0, [this](FState& State) { CastWAtTheTarget(State); })
				.UntilServer(TEXT("The channel runs"), [](FState& State) { return CasterPhase(State) == EVeyraCastPhase::Channel; })
				.ThenServer(TEXT("The caster is held in place"), [this](FState& State) {
					ASSERT_THAT(IsTrue(ParticipantOf(State, 0)->GetPawn()->FindComponentByClass<UVeyraMovementComponent>()->IsMovementLocked()));
				})
				.UntilServer(TEXT("The channel ends"), [](FState& State) { return CasterPhase(State) == EVeyraCastPhase::None; })
				.ThenServer(TEXT("Every tick hit"), [this](FState& State) {
					ASSERT_THAT(IsTrue(HealthLost(ParticipantOf(State, 1)) == TickDamage * Ticks));
					ASSERT_THAT(IsFalse(ParticipantOf(State, 0)->GetPawn()->FindComponentByClass<UVeyraMovementComponent>()->IsMovementLocked()));
				});
		}
	};
}

#endif // ENABLE_PIE_NETWORK_TEST
