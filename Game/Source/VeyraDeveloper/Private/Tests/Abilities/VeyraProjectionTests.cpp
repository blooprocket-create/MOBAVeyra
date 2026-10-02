// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "AbilitySystemGlobals.h"
#include "Attributes/VeyraVitalsSet.h"
#include "CQTest.h"
#include "Echoes/VeyraEcho.h"
#include "Echoes/VeyraEchoSubsystem.h"
#include "Statuses/VeyraStatusComponent.h"
#include "Targeting/VeyraTargeting.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "TimerManager.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "VeyraCombatVerbs.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraAbilitiesTests
{
	namespace ProjectionFixture
	{
		// Fixture values.
		constexpr double Range = 1000.0;
		constexpr double Burst = 100.0;
		constexpr double BurstRadius = 150.0;
		constexpr double Coefficient = 0.25;
		constexpr double Formation = 0.5;
		constexpr double Immunity = 2.0;
		constexpr double Integrity = 100.0;
		constexpr double Decay = 10.0;
		constexpr double MinRadius = 400.0;
		constexpr double AttackLoss = 20.0;
		constexpr double AbilityLoss = 25.0;
		constexpr double Update = 0.1;
		constexpr double StasisSeconds = 20.0;
		constexpr double Poke = 10.0;
		constexpr double Tolerance = 1e-3;
		constexpr float Step = 0.05f;
		const FVector EchoAt(500.0, 0.0, 0.0);
		const FVector EnemyAt(500.0, 400.0, 0.0);
	}

	// Veyra.Abilities.Projections.*: a projected Echo (Item Bible §11; ADR-050 §3–§5): its holder in Stasis, its formation
	// and control, its immunity, Integrity and tether, its one cast, and its end.
	TEST_CLASS(Projections, "Veyra.Abilities")
	{
		FActorTestSpawner Spawner;
		FVeyraAbilitiesTuning Tuning;
		AVeyraVanguardCharacter* Holder = nullptr;
		AVeyraVanguardCharacter* Enemy = nullptr;
		int32 Commanded = 0;
		TArray<EVeyraEchoEnd> Ends;

		BEFORE_EACH()
		{
			using namespace ProjectionFixture;
			FVeyraAreaAbilityTuning Area;
			Area.Cast = InstantCast(Range, 0.0, 0.0);
			Area.Origin = EVeyraAreaOrigin::TargetPoint;
			FVeyraAreaZoneTuning& Zone = Area.Zones.AddDefaulted_GetRef();
			Zone.Shape = CircleOf(BurstRadius);
			FVeyraDamageTuning& Damage = Zone.Effects.Damage.AddDefaulted_GetRef();
			Damage.Type = EVeyraDamageType::TrueDamage;
			Damage.AmountByRank = { Burst };
			Tuning.Area.Add(ArchetypeTestId(TEXT("test_burst")), Area);
			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_stasis")), StatusOf(EVeyraStatusKind::Stasis, 0.0, StasisSeconds));

			FVeyraEchoAbilityTuning Echo;
			Echo.Cast = InstantCast(Range, 0.0, 0.0);
			Echo.DamageCoefficient = Coefficient;
			Echo.Slots = { EVeyraAbilitySlot::Q };
			Echo.Repeats = 1;
			FVeyraEchoProjectionTuning& Projection = Echo.Projection.AddDefaulted_GetRef();
			Projection.Stasis = ArchetypeTestId(TEXT("test_stasis"));
			Projection.FormationSeconds = Formation;
			Projection.ImmunitySeconds = Immunity;
			Projection.Integrity = Integrity;
			Projection.DecayPerSecond = Decay;
			Projection.MaxRadius = Range;
			Projection.MinRadius = MinRadius;
			Projection.RadiusExponent = 1.0;
			Projection.UpdateSeconds = Update;
			Projection.IntegrityLoss.VanguardBasicAttack = AttackLoss;
			Projection.IntegrityLoss.UnitBasicAttack = AttackLoss;
			Projection.IntegrityLoss.Ability = AbilityLoss;
			Tuning.Echo.Add(ArchetypeTestId(TEXT("test_project")), Echo);
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(&Tuning);
			const int32 Ranks[] = { 5, 3 };
			ASSERT_THAT(IsTrue(VeyraAbilityRules::Validate(Tuning, Ranks).IsEmpty(), FString::Join(VeyraAbilityRules::Validate(Tuning, Ranks), TEXT(" | "))));

			FArchetypeTestWorld World{ Spawner };
			Holder = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			Enemy = &World.Spawn(EVeyraTeam::B, EnemyAt);
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Learn(*Holder, EVeyraAbilitySlot::Q, ArchetypeTestId(TEXT("test_burst")))));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Equip(*Holder, EVeyraAbilitySlot::Item1, ArchetypeTestId(TEXT("test_project")))));
			Echoes().OnEchoCommanded.AddLambda([this](UAbilitySystemComponent&, AVeyraEcho&) { ++Commanded; });
			Echoes().OnEchoEnded.AddLambda([this](AVeyraEcho&, EVeyraEchoEnd Why) { Ends.Add(Why); });
		}

		AFTER_EACH()
		{
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(nullptr);
		}

		UVeyraEchoSubsystem& Echoes()
		{
			return *Spawner.GetWorld().GetSubsystem<UVeyraEchoSubsystem>();
		}

		UAbilitySystemComponent& HolderAbilities() const
		{
			return *Holder->GetAbilitySystemComponent();
		}

		bool HolderInStasis() const
		{
			const UVeyraStatusComponent* Statuses = Holder->GetPlayerState()->FindComponentByClass<UVeyraStatusComponent>();
			return Statuses && Statuses->Has(EVeyraStatusKind::Stasis);
		}

		/** An Echo's Integrity is its sealed Health (ADR-050 §3). */
		static double IntegrityOf(const AVeyraEcho& Echo)
		{
			return Echo.GetAbilitySystemComponent()->GetNumericAttribute(UVeyraVitalsSet::GetHealthAttribute());
		}

		void Wait(double Seconds)
		{
			UWorld& World = Spawner.GetWorld();
			const double Until = World.GetTimeSeconds() + Seconds;
			do
			{
				World.Tick(LEVELTICK_TimeOnly, ProjectionFixture::Step);
				++GFrameCounter;
				World.GetTimerManager().Tick(ProjectionFixture::Step);
			} while (World.GetTimeSeconds() < Until);
		}

		bool Hit(EVeyraDamageDelivery Delivery, AActor& Target) const
		{
			FVeyraRawDamageEvent Damage;
			Damage.Components.Add({ EVeyraDamageType::TrueDamage, ProjectionFixture::Poke });
			Damage.Delivery = Delivery;
			return VeyraCombat::DealDamage(*Enemy->GetAbilitySystemComponent(), *UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Target), Damage);
		}

		AVeyraEcho* Project()
		{
			return FArchetypeTestWorld::CastAt(*Holder, EVeyraAbilitySlot::Item1, ProjectionFixture::EchoAt) == EVeyraCastRejection::None
				? Echoes().FindStanding(HolderAbilities()) : nullptr;
		}

		TEST_METHOD(ItsHolderWaitsInStasisWhileItFormsThenTakesControl)
		{
			using namespace ProjectionFixture;
			AVeyraEcho* Echo = Project();
			ASSERT_THAT(IsNotNull(Echo));
			ASSERT_THAT(IsTrue(HolderInStasis(), TEXT("its holder enters Stasis at once, where it stands")));
			ASSERT_THAT(IsTrue(Echo->GetAnchor().Equals(Holder->GetActorLocation(), 1.0), TEXT("the Stasis body anchors the tether")));
			ASSERT_THAT(IsTrue(FVector::Dist2D(Echo->GetActorLocation(), EchoAt) <= 1.0, TEXT("it forms at the point")));
			ASSERT_THAT(IsNull(Echoes().FindCommanded(HolderAbilities()), TEXT("forming, it takes no orders")));
			Wait(Formation + Step);
			ASSERT_THAT(IsTrue(Echoes().FindCommanded(HolderAbilities()) == Echo && Commanded == 1, TEXT("formed, control passes to it")));
		}

		TEST_METHOD(EnemyHitsCostIntegrityOnlyOnceItsImmunityEnds)
		{
			using namespace ProjectionFixture;
			AVeyraEcho* Echo = Project();
			ASSERT_THAT(IsNotNull(Echo));
			const double Before = IntegrityOf(*Echo);
			ASSERT_THAT(IsTrue(Hit(EVeyraDamageDelivery::BasicAttack, *Echo)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(IntegrityOf(*Echo), Before, Tolerance), TEXT("immune: a hit costs nothing")));
			Wait(Immunity + Step);
			const double Exposed = IntegrityOf(*Echo);
			ASSERT_THAT(IsTrue(Exposed < Before - Decay * Immunity * 0.9, TEXT("its Integrity decays through its immunity too")));
			ASSERT_THAT(IsTrue(Hit(EVeyraDamageDelivery::BasicAttack, *Echo)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(IntegrityOf(*Echo), Exposed - AttackLoss, Tolerance), TEXT("a basic attack costs its set amount, not its damage")));
			ASSERT_THAT(IsTrue(Hit(EVeyraDamageDelivery::Ability, *Echo)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(IntegrityOf(*Echo), Exposed - AttackLoss - AbilityLoss, Tolerance), TEXT("an ability's hit its own")));
			ASSERT_THAT(IsTrue(VeyraCombat::RestoreHealthFrom(HolderAbilities(), *Echo->GetAbilitySystemComponent(), Poke) == 0.0, TEXT("nothing restores it")));
		}

		TEST_METHOD(ItFadesAsItsIntegrityRunsOutAndTheStasisEnds)
		{
			using namespace ProjectionFixture;
			AVeyraEcho* Echo = Project();
			ASSERT_THAT(IsNotNull(Echo));
			// Within its least radius, the shrinking tether never reaches it: only its Integrity ends it.
			Echo->SetActorLocation(Echo->GetAnchor() + FVector(MinRadius / 2.0, 0.0, Echo->GetActorLocation().Z - Echo->GetAnchor().Z));
			Wait(Integrity / Decay + Update + Step);
			ASSERT_THAT(IsTrue(Echo->IsWithdrawn() && Ends.Num() == 1 && Ends[0] == EVeyraEchoEnd::Faded, TEXT("it fades at 0 Integrity")));
			ASSERT_THAT(IsFalse(HolderInStasis(), TEXT("its end ends its holder's Stasis")));
			ASSERT_THAT(IsNull(Echoes().FindCommanded(HolderAbilities())));
		}

		TEST_METHOD(LeavingItsTetherBreaksIt)
		{
			using namespace ProjectionFixture;
			AVeyraEcho* Echo = Project();
			ASSERT_THAT(IsNotNull(Echo));
			Wait(Formation + Step);
			Echo->SetActorLocation(Echo->GetAnchor() + FVector(Range + BurstRadius, 0.0, Echo->GetActorLocation().Z - Echo->GetAnchor().Z));
			Wait(Update + Step);
			ASSERT_THAT(IsTrue(Echo->IsWithdrawn() && Ends.Num() == 1 && Ends[0] == EVeyraEchoEnd::Strayed, TEXT("beyond the circle the tether snaps")));
			ASSERT_THAT(IsFalse(HolderInStasis()));
		}

		TEST_METHOD(TheShrinkingTetherCatchesAnEchoThatStays)
		{
			using namespace ProjectionFixture;
			AVeyraEcho* Echo = Project();
			ASSERT_THAT(IsNotNull(Echo));
			// Inside at full Integrity, but past where the circle will have shrunk to.
			const double Stand = (Range + MinRadius) / 2.0 + BurstRadius;
			Echo->SetActorLocation(Echo->GetAnchor() + FVector(Stand, 0.0, Echo->GetActorLocation().Z - Echo->GetAnchor().Z));
			Wait(Update + Step);
			ASSERT_THAT(IsFalse(Echo->IsWithdrawn(), TEXT("inside the circle at first")));
			Wait(Integrity / Decay);
			ASSERT_THAT(IsTrue(Ends.Num() == 1 && Ends[0] == EVeyraEchoEnd::Strayed, TEXT("the circle shrank past it before it faded")));
		}

		TEST_METHOD(ItCastsOneEligibleAbilityAtItsShareAndNoItem)
		{
			using namespace ProjectionFixture;
			AVeyraEcho* Echo = Project();
			ASSERT_THAT(IsNotNull(Echo));
			FVeyraCastTarget AtEnemy;
			AtEnemy.bHasLocation = true;
			AtEnemy.Location = EnemyAt;
			ASSERT_THAT(IsTrue(Echoes().CastFrom(HolderAbilities(), EVeyraAbilitySlot::Q, AtEnemy) == EVeyraCastRejection::Projected, TEXT("not before control")));
			Wait(Formation + Step);
			ASSERT_THAT(IsTrue(Echoes().CastFrom(HolderAbilities(), EVeyraAbilitySlot::Item1, AtEnemy) == EVeyraCastRejection::Projected, TEXT("never an item")));
			ASSERT_THAT(IsTrue(Echoes().CastFrom(HolderAbilities(), EVeyraAbilitySlot::Q, AtEnemy) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(FArchetypeTestWorld::HealthLost(*Enemy), Burst * Coefficient, Tolerance), TEXT("at its share, True Damage too")));
			ASSERT_THAT(IsTrue(Echoes().CastFrom(HolderAbilities(), EVeyraAbilitySlot::Q, AtEnemy) == EVeyraCastRejection::Projected, TEXT("once")));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Holder, EVeyraAbilitySlot::Q, EnemyAt) == EVeyraCastRejection::CrowdControlled,
				TEXT("the Stasis body casts nothing itself")));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
