// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Attributes/VeyraOffenceSet.h"
#include "CQTest.h"
#include "Echoes/VeyraEcho.h"
#include "Echoes/VeyraEchoRules.h"
#include "Echoes/VeyraEchoSubsystem.h"
#include "Progression/VeyraProgressionComponent.h"
#include "Targeting/VeyraTargeting.h"
#include "Teams/VeyraTeam.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "TimerManager.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "Units/VeyraUnit.h"
#include "VeyraCombatVerbs.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraAbilitiesTests
{
	namespace EchoFixture
	{
		// Fixture values.
		constexpr double Range = 1000.0;
		constexpr double EchoRange = 700.0;
		constexpr double Burst = 100.0;
		constexpr double BurstRadius = 150.0;
		constexpr double Coefficient = 0.5;
		constexpr double Window = 4.0;
		constexpr double Tolerance = 1e-3;
		constexpr float Step = 0.1f;
		// Enough experience for at least one level.
		constexpr double LevelsOfExperience = 100000.0;
		const FVector EchoAt(0.0, 500.0, 0.0);
		const FVector EnemyAt(400.0, 0.0, 0.0);
	}

	// Veyra.Abilities.EchoRules.*: where an Echo's repeat is aimed, its tether radius and what a hit costs it (ADR-050 §3–§5).
	TEST_CLASS(EchoRules, "Veyra.Abilities")
	{
		TEST_METHOD(ARepeatAimsAtTheSamePointWithinItsRange)
		{
			const FVector Caster(0.0, 0.0, 0.0);
			const FVector Echo(0.0, 500.0, 0.0);
			const FVeyraEchoAim Near = VeyraEchoRules::AimFrom(Echo, Caster, FVector(400.0, 0.0, 0.0), FVector::ForwardVector, 1000.0);
			ASSERT_THAT(IsTrue(Near.Point.Equals(FVector(400.0, 0.0, 0.0), 1e-3), TEXT("the same point, within range")));
			ASSERT_THAT(IsTrue(Near.Direction.Equals((FVector(400.0, -500.0, 0.0)).GetSafeNormal(), 1e-3), TEXT("aimed from the Echo")));

			const FVeyraEchoAim Far = VeyraEchoRules::AimFrom(Echo, Caster, FVector(400.0, 0.0, 0.0), FVector::ForwardVector, 320.0);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(FVector::Dist2D(Far.Point, Echo), 320.0, 1e-3), TEXT("brought within the range")));

			const FVeyraEchoAim Self = VeyraEchoRules::AimFrom(Echo, Caster, Caster, FVector::RightVector, 1000.0);
			ASSERT_THAT(IsTrue(Self.Point.Equals(Echo) && Self.Direction.Equals(FVector::RightVector), TEXT("a cast on its caster is repeated on the Echo")));
		}

		TEST_METHOD(TheTetherShrinksWithIntegrityToItsMinimum)
		{
			FVeyraEchoProjectionTuning Projection;
			Projection.Integrity = 100.0;
			Projection.MaxRadius = 1100.0;
			Projection.MinRadius = 450.0;
			Projection.RadiusExponent = 1.0;
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraEchoRules::TetherRadius(Projection, 100.0), 1100.0)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraEchoRules::TetherRadius(Projection, 50.0), 775.0)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraEchoRules::TetherRadius(Projection, 0.0), 450.0)));
			Projection.RadiusExponent = 2.0;
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraEchoRules::TetherRadius(Projection, 50.0), 612.5), TEXT("a curve narrows it sooner")));
		}

		TEST_METHOD(EachHitCostsItsKindsIntegrity)
		{
			FVeyraEchoIntegrityLossTuning Loss;
			Loss.VanguardBasicAttack = 20.0;
			Loss.UnitBasicAttack = 10.0;
			Loss.Ability = 25.0;
			Loss.StructureAttack = 50.0;
			Loss.Periodic = 5.0;
			Loss.Proc = 3.0;
			ASSERT_THAT(IsTrue(VeyraEchoRules::IntegrityLoss(Loss, EVeyraDamageDelivery::BasicAttack, true) == 20.0));
			ASSERT_THAT(IsTrue(VeyraEchoRules::IntegrityLoss(Loss, EVeyraDamageDelivery::BasicAttack, false) == 10.0));
			ASSERT_THAT(IsTrue(VeyraEchoRules::IntegrityLoss(Loss, EVeyraDamageDelivery::Ability, true) == 25.0));
			ASSERT_THAT(IsTrue(VeyraEchoRules::IntegrityLoss(Loss, EVeyraDamageDelivery::StructureAttack, false) == 50.0));
			ASSERT_THAT(IsTrue(VeyraEchoRules::IntegrityLoss(Loss, EVeyraDamageDelivery::Periodic, true) == 5.0));
			ASSERT_THAT(IsTrue(VeyraEchoRules::IntegrityLoss(Loss, EVeyraDamageDelivery::Proc, true) == 3.0));
			ASSERT_THAT(IsTrue(VeyraEchoRules::IntegrityLoss(Loss, EVeyraDamageDelivery::Presence, true) == 0.0));
		}
	};

	// Veyra.Abilities.Echoes.*: a manifest Echo repeats its holder's next eligible ability once, at its share of the
	// damage, and nothing it does repeats again (Item Bible §9; ADR-050 §2, §4, §5).
	TEST_CLASS(Echoes, "Veyra.Abilities")
	{
		FActorTestSpawner Spawner;
		FVeyraAbilitiesTuning Tuning;
		AVeyraVanguardCharacter* Holder = nullptr;
		AVeyraVanguardCharacter* Enemy = nullptr;

		static FVeyraAreaAbilityTuning BurstAtPoint()
		{
			using namespace EchoFixture;
			FVeyraAreaAbilityTuning Area;
			Area.Cast = InstantCast(Range, 0.0, 0.0);
			Area.Origin = EVeyraAreaOrigin::TargetPoint;
			FVeyraAreaZoneTuning& Zone = Area.Zones.AddDefaulted_GetRef();
			Zone.Shape = CircleOf(BurstRadius);
			FVeyraDamageTuning& Damage = Zone.Effects.Damage.AddDefaulted_GetRef();
			Damage.Type = EVeyraDamageType::TrueDamage;
			Damage.AmountByRank = { Burst };
			return Area;
		}

		BEFORE_EACH()
		{
			using namespace EchoFixture;
			Tuning.Area.Add(ArchetypeTestId(TEXT("test_burst")), BurstAtPoint());
			Tuning.Area.Add(ArchetypeTestId(TEXT("test_other_burst")), BurstAtPoint());
			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_veil")), StatusOf(EVeyraStatusKind::Untargetable, 0.0, Window));
			FVeyraEchoAbilityTuning Echo;
			Echo.Cast = InstantCast(EchoRange, 0.0, 0.0);
			Echo.DamageCoefficient = Coefficient;
			Echo.Slots = { EVeyraAbilitySlot::Q, EVeyraAbilitySlot::W };
			Echo.Repeats = 1;
			FVeyraEchoManifestTuning& Manifest = Echo.Manifest.AddDefaulted_GetRef();
			Manifest.WindowSeconds = Window;
			Manifest.Statuses = { ArchetypeTestId(TEXT("test_veil")) };
			Tuning.Echo.Add(ArchetypeTestId(TEXT("test_echo")), Echo);
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(&Tuning);
			const int32 Ranks[] = { 5, 3 };
			ASSERT_THAT(IsTrue(VeyraAbilityRules::Validate(Tuning, Ranks).IsEmpty(), FString::Join(VeyraAbilityRules::Validate(Tuning, Ranks), TEXT(" | "))));

			FArchetypeTestWorld World{ Spawner };
			Holder = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			Enemy = &World.Spawn(EVeyraTeam::B, EnemyAt);
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Learn(*Holder, EVeyraAbilitySlot::Q, ArchetypeTestId(TEXT("test_burst")))));
			// A second skill point for E, which takes its rank as Q took its own.
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Equip(*Holder, EVeyraAbilitySlot::E, ArchetypeTestId(TEXT("test_other_burst")))));
			UVeyraProgressionComponent* Progression = Holder->GetPlayerState()->FindComponentByClass<UVeyraProgressionComponent>();
			ASSERT_THAT(IsTrue(Progression->AddExperience(LevelsOfExperience) > 0, TEXT("a level up brings a skill point")));
			ASSERT_THAT(IsTrue(Progression->AllocateRank(EVeyraAbilitySlot::E) == EVeyraRankRefusal::None));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Equip(*Holder, EVeyraAbilitySlot::Item1, ArchetypeTestId(TEXT("test_echo")))));
		}

		AFTER_EACH()
		{
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(nullptr);
		}

		UAbilitySystemComponent& HolderAbilities() const
		{
			return *Holder->GetAbilitySystemComponent();
		}

		AVeyraEcho* Standing()
		{
			return Spawner.GetWorld().GetSubsystem<UVeyraEchoSubsystem>()->FindStanding(HolderAbilities());
		}

		void Wait(double Seconds)
		{
			UWorld& World = Spawner.GetWorld();
			const double Until = World.GetTimeSeconds() + Seconds;
			do
			{
				World.Tick(LEVELTICK_TimeOnly, EchoFixture::Step);
				++GFrameCounter;
				World.GetTimerManager().Tick(EchoFixture::Step);
			} while (World.GetTimeSeconds() < Until);
		}

		TEST_METHOD(ItFormsAtThePointAsItsHoldersProjection)
		{
			using namespace EchoFixture;
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Holder, EVeyraAbilitySlot::Item1, EchoAt) == EVeyraCastRejection::None));
			AVeyraEcho* Echo = Standing();
			ASSERT_THAT(IsNotNull(Echo));
			ASSERT_THAT(IsTrue(FVector::Dist2D(Echo->GetActorLocation(), EchoAt) <= 1.0, TEXT("at the point")));
			ASSERT_THAT(IsTrue(VeyraUnits::KindOf(Echo) == EVeyraUnitKind::Echo && VeyraTeams::TeamOf(Echo) == EVeyraTeam::A));
			ASSERT_THAT(IsTrue(VeyraCombat::ResponsibleFor(Echo->GetAbilitySystemComponent()) == &HolderAbilities(), TEXT("what it causes is its holder's")));
			ASSERT_THAT(IsTrue(VeyraTargeting::IsUntargetable(*Echo), TEXT("it waits Untargetable")));
			const double Share = Echo->GetAbilitySystemComponent()->GetNumericAttribute(UVeyraOffenceSet::GetDamageShareAttribute());
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Share, Coefficient, Tolerance), TEXT("its share of its holder's damage, True Damage included")));
		}

		TEST_METHOD(ItRepeatsTheNextEligibleCastOnceAtItsShare)
		{
			using namespace EchoFixture;
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Holder, EVeyraAbilitySlot::Item1, EchoAt) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Holder, EVeyraAbilitySlot::Q, EnemyAt) == EVeyraCastRejection::None));
			const double Lost = FArchetypeTestWorld::HealthLost(*Enemy);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Lost, Burst * (1.0 + Coefficient), Tolerance),
				FString::Printf(TEXT("the burst, then the Echo's repeat at its share: %g"), Lost)));

			Wait(Step);
			AVeyraEcho* Spent = Standing();
			ASSERT_THAT(IsNull(Spent, TEXT("its one repeat spent, it ends")));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Holder, EVeyraAbilitySlot::Q, EnemyAt) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(FArchetypeTestWorld::HealthLost(*Enemy), Lost + Burst, Tolerance), TEXT("nothing repeats it again")));
		}

		TEST_METHOD(AnIneligibleCastIsNotRepeatedAndTheWindowEnds)
		{
			using namespace EchoFixture;
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Holder, EVeyraAbilitySlot::Item1, EchoAt) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Holder, EVeyraAbilitySlot::E, EnemyAt) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(FArchetypeTestWorld::HealthLost(*Enemy), Burst, Tolerance), TEXT("E is not among the slots it repeats")));
			ASSERT_THAT(IsNotNull(Standing(), TEXT("so it waits on")));

			Wait(Window + Step);
			ASSERT_THAT(IsNull(Standing(), TEXT("its window ran out")));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Holder, EVeyraAbilitySlot::Q, EnemyAt) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(FArchetypeTestWorld::HealthLost(*Enemy), Burst * 2.0, Tolerance), TEXT("an ended Echo repeats nothing")));
		}

		TEST_METHOD(ANewEchoReplacesTheLast)
		{
			using namespace EchoFixture;
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Holder, EVeyraAbilitySlot::Item1, EchoAt) == EVeyraCastRejection::None));
			const TWeakObjectPtr<AVeyraEcho> First = Standing();
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Holder, EVeyraAbilitySlot::Item1, -EchoAt) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(Standing() && Standing() != First.Get(), TEXT("one Echo at a time")));
			ASSERT_THAT(IsTrue(!First.IsValid() || First->IsActorBeingDestroyed(), TEXT("the last goes")));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
