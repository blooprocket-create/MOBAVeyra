// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Absorption/VeyraDamageAbsorptionComponent.h"
#include "Attributes/VeyraOffenceSet.h"
#include "Attributes/VeyraResourceSet.h"
#include "Casting/VeyraCastStateComponent.h"
#include "Cooldowns/VeyraCooldownComponent.h"
#include "CQTest.h"
#include "Delivery/VeyraDelayedArea.h"
#include "EngineUtils.h"
#include "Life/VeyraLifeComponent.h"
#include "Progression/VeyraProgressionTuningSubsystem.h"
#include "Targeting/VeyraTargeting.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "VeyraCombatVerbs.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraAbilitiesTests
{
	// Veyra.Abilities.Shapes.*: hit shapes on the ground, edges included (Combat Bible §40; ADR-009 §4).
	TEST_CLASS(Shapes, "Veyra.Abilities")
	{
		// Fixture values: a body radius and shape sizes.
		static constexpr double Body = 40.0;
		static constexpr double Radius = 300.0;
		static constexpr double Length = 500.0;
		static constexpr double Width = 200.0;
		static constexpr double Arc = 90.0;
		static constexpr double Epsilon = 0.01;

		FActorTestSpawner Spawner;

		static FVeyraPlacedShape Placed(const FVeyraShape& Shape)
		{
			return FVeyraPlacedShape{ Shape, FVector::ZeroVector, FVector::ForwardVector };
		}

		TEST_METHOD(ACircleTouchesABodyAtItsEdge)
		{
			const FVeyraPlacedShape Circle = Placed(CircleOf(Radius));
			ASSERT_THAT(IsTrue(VeyraShapes::Touches(Circle, FVector(Radius + Body, 0.0, 0.0), Body)));
			ASSERT_THAT(IsFalse(VeyraShapes::Touches(Circle, FVector(Radius + Body + Epsilon, 0.0, 0.0), Body)));
		}

		TEST_METHOD(ARectangleRunsAlongTheDirectionFromTheOrigin)
		{
			FVeyraShape Shape;
			Shape.Kind = EVeyraShapeKind::Rectangle;
			Shape.Length = Length;
			Shape.Width = Width;
			const FVeyraPlacedShape Rectangle = Placed(Shape);
			ASSERT_THAT(IsTrue(VeyraShapes::Touches(Rectangle, FVector(Length / 2.0, Width / 2.0 + Body, 0.0), Body)));
			ASSERT_THAT(IsFalse(VeyraShapes::Touches(Rectangle, FVector(Length / 2.0, Width / 2.0 + Body + Epsilon, 0.0), Body)));
			ASSERT_THAT(IsFalse(VeyraShapes::Touches(Rectangle, FVector(-(Body + Epsilon), 0.0, 0.0), Body), TEXT("behind the origin")));
			ASSERT_THAT(IsTrue(VeyraShapes::Touches(Rectangle, FVector(Length + Body, 0.0, 0.0), Body)));
		}

		TEST_METHOD(ASectorCoversItsArcAndTouchesAlongItsEdges)
		{
			FVeyraShape Shape;
			Shape.Kind = EVeyraShapeKind::Sector;
			Shape.Radius = Radius;
			Shape.ArcDegrees = Arc;
			const FVeyraPlacedShape Sector = Placed(Shape);
			ASSERT_THAT(IsTrue(VeyraShapes::Touches(Sector, FVector(Radius / 2.0, Radius / 4.0, 0.0), Body)));
			// Just outside the arc's edge at 45 degrees, but near enough to overlap it.
			const FVector BesideEdge = FVector(1.0, 1.0, 0.0).GetSafeNormal() * (Radius / 2.0) + FVector(-1.0, 1.0, 0.0).GetSafeNormal() * (Body - 1.0);
			ASSERT_THAT(IsTrue(VeyraShapes::Touches(Sector, BesideEdge, Body)));
			ASSERT_THAT(IsFalse(VeyraShapes::Touches(Sector, FVector(-Radius / 2.0, 0.0, 0.0), Body), TEXT("behind the sector")));
		}

		TEST_METHOD(ValidationKeepsEachKindToItsOwnSizes)
		{
			FVeyraShape Circle = CircleOf(Radius);
			ASSERT_THAT(IsTrue(VeyraShapes::Validate(Circle).IsEmpty()));
			Circle.Width = Width;
			ASSERT_THAT(IsFalse(VeyraShapes::Validate(Circle).IsEmpty()));
			FVeyraShape Sector;
			Sector.Kind = EVeyraShapeKind::Sector;
			Sector.Radius = Radius;
			ASSERT_THAT(IsFalse(VeyraShapes::Validate(Sector).IsEmpty(), TEXT("a sector needs its arc")));
		}

		TEST_METHOD(GatheringOrdersUnitsByDistanceAndSkipsTheDead)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Far = World.Spawn(EVeyraTeam::B, FVector(250.0, 0.0, 0.0));
			AVeyraVanguardCharacter& Near = World.Spawn(EVeyraTeam::B, FVector(100.0, 0.0, 0.0));
			AVeyraVanguardCharacter& Outside = World.Spawn(EVeyraTeam::B, FVector(Radius * 3.0, 0.0, 0.0));
			AVeyraVanguardCharacter& Dead = World.Spawn(EVeyraTeam::B, FVector(0.0, 150.0, 0.0));
			Dead.GetPlayerState()->FindComponentByClass<UVeyraLifeComponent>()->SetState(EVeyraLifeState::Dead);
			const TArray<AActor*> Units = VeyraShapes::GatherUnits(Spawner.GetWorld(), Placed(CircleOf(Radius)), [](const AActor&) { return true; });
			ASSERT_THAT(IsTrue(Units == TArray<AActor*>{ &Near, &Far }, FString::Printf(TEXT("gathered %d unit(s)"), Units.Num())));
			ASSERT_THAT(IsFalse(Units.Contains(&Outside)));
		}

		TEST_METHOD(APathMeetsBodiesInTheOrderItReachesThem)
		{
			// Fixture values: a path along +X and the radius swept along it.
			constexpr double PathLength = 1000.0;
			constexpr double SweepRadius = 30.0;
			const FVector End(PathLength, 0.0, 0.0);
			const double Touching = SweepRadius + Body;
			const auto ContactWith = [&End](const FVector& Center) { return VeyraShapes::FirstContactAlong(FVector::ZeroVector, End, SweepRadius, Center, Body); };

			const TOptional<double> Ahead = ContactWith(FVector(Length, 0.0, 0.0));
			ASSERT_THAT(IsTrue(Ahead.IsSet() && FMath::IsNearlyEqual(Ahead.GetValue(), Length - Touching, Epsilon)));
			ASSERT_THAT(IsTrue(ContactWith(FVector(Length, Touching - Epsilon, 0.0)).IsSet()));
			ASSERT_THAT(IsFalse(ContactWith(FVector(Length, Touching + Epsilon, 0.0)).IsSet()));
			ASSERT_THAT(IsFalse(ContactWith(FVector(-(Touching + Epsilon), 0.0, 0.0)).IsSet(), TEXT("behind the start")));
			ASSERT_THAT(IsFalse(ContactWith(FVector(PathLength + Touching + Epsilon, 0.0, 0.0)).IsSet(), TEXT("past the end")));
			const TOptional<double> AtStart = ContactWith(FVector(0.0, Touching, 0.0));
			ASSERT_THAT(IsTrue(AtStart.IsSet() && AtStart.GetValue() == 0.0, TEXT("touching at the start, edges included")));

			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Second = World.Spawn(EVeyraTeam::B, FVector(Length, 0.0, 0.0));
			AVeyraTestFluxborn& First = World.SpawnFluxborn(EVeyraTeam::B, FVector(Length / 2.0, SweepRadius, 0.0));
			World.Spawn(EVeyraTeam::B, FVector(Length, Radius * 2.0, 0.0));
			const TArray<FVeyraPathHit> Hits = VeyraShapes::GatherUnitsAlong(Spawner.GetWorld(), FVector::ZeroVector, End, SweepRadius, [](const AActor&) { return true; });
			ASSERT_THAT(AreEqual(2, Hits.Num()));
			ASSERT_THAT(IsTrue(Hits[0].Unit == &First && Hits[1].Unit == &Second));
		}
	};

	// Veyra.Abilities.Area.*: the area archetype hits enemies in zones, innermost first (ADR-008 §3).
	TEST_CLASS(Area, "Veyra.Abilities")
	{
		// Fixture values, independent of the committed Abilities.json.
		static constexpr double InnerRadius = 150.0;
		static constexpr double OuterRadius = 400.0;
		static constexpr double CastRange = 600.0;
		static constexpr double InnerDamage = 50.0;
		static constexpr double PowerRatio = 0.5;
		static constexpr double MissingHealthRatio = 0.25;
		static constexpr double LongSeconds = 60.0;
		static constexpr double TakedownExtension = 5.0;

		FActorTestSpawner Spawner;
		FVeyraAbilitiesTuning Tuning;
		AVeyraVanguardCharacter* Caster = nullptr;

		BEFORE_EACH()
		{
			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_stun")), StatusOf(EVeyraStatusKind::Stun, 0.0, LongSeconds));
			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_slow")), StatusOf(EVeyraStatusKind::Slow, 0.3, LongSeconds));

			FVeyraAreaAbilityTuning Slam;
			Slam.Cast = InstantCast(CastRange, LongSeconds, 0.0);
			Slam.Origin = EVeyraAreaOrigin::Caster;
			FVeyraAreaZoneTuning& Inner = Slam.Zones.AddDefaulted_GetRef();
			Inner.Shape = CircleOf(InnerRadius);
			Inner.Effects.Damage.Add(FVeyraDamageTuning{ EVeyraDamageType::TrueDamage, { InnerDamage }, PowerRatio, 0.0 });
			Inner.Effects.Statuses.Add(ArchetypeTestId(TEXT("test_stun")));
			FVeyraAreaZoneTuning& Outer = Slam.Zones.AddDefaulted_GetRef();
			Outer.Shape = CircleOf(OuterRadius);
			Outer.Effects.Statuses.Add(ArchetypeTestId(TEXT("test_slow")));
			Tuning.Area.Add(ArchetypeTestId(TEXT("test_slam")), Slam);

			FVeyraAreaAbilityTuning Mortar = Slam;
			Mortar.Origin = EVeyraAreaOrigin::TargetPoint;
			Mortar.Zones.SetNum(1);
			Tuning.Area.Add(ArchetypeTestId(TEXT("test_mortar")), Mortar);

			FVeyraAreaAbilityTuning Flare = Mortar;
			Flare.DelaySeconds = LongSeconds;
			Tuning.Area.Add(ArchetypeTestId(TEXT("test_flare")), Flare);

			FVeyraAreaAbilityTuning Grasp = Slam;
			Grasp.Zones.SetNum(1);
			Grasp.Zones[0].Shape = CircleOf(OuterRadius);
			Grasp.Zones[0].Effects = FVeyraEffectBundleTuning();
			Grasp.Zones[0].Effects.Displacement.Add(FVeyraDisplacementTuning{ EVeyraDisplacementDirection::TowardOrigin, OuterRadius, OuterRadius });
			Tuning.Area.Add(ArchetypeTestId(TEXT("test_grasp")), Grasp);

			// A sweep whose caster enters a frenzy when it catches an enemy Vanguard (No Quarter).
			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_frenzy")), StatusOf(EVeyraStatusKind::AttackSpeed, 0.5, LongSeconds));
			FVeyraAreaAbilityTuning Sweep = Slam;
			Sweep.Cast = InstantCast(CastRange, 0.0, 0.0);
			Sweep.Zones.SetNum(1);
			Sweep.Zones[0].Shape = CircleOf(OuterRadius);
			Sweep.Zones[0].Effects = FVeyraEffectBundleTuning();
			Sweep.Zones[0].CasterStatusesPerVanguard.Add(ArchetypeTestId(TEXT("test_frenzy")));
			Tuning.Area.Add(ArchetypeTestId(TEXT("test_sweep")), Sweep);

			// A sweep that also deals damage, into a frenzy that takedowns extend (No Quarter).
			FVeyraStatusTuning Rampage = StatusOf(EVeyraStatusKind::AttackSpeed, 0.5, LongSeconds);
			Rampage.TakedownExtensionSeconds = TakedownExtension;
			Rampage.TakedownExtensionMaxSeconds = TakedownExtension;
			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_rampage_frenzy")), Rampage);
			FVeyraAreaAbilityTuning RampageSweep = Sweep;
			RampageSweep.Zones[0].CasterStatusesPerVanguard = { ArchetypeTestId(TEXT("test_rampage_frenzy")) };
			RampageSweep.Zones[0].Effects.Damage.Add(FVeyraDamageTuning{ EVeyraDamageType::TrueDamage, { InnerDamage }, 0.0, 0.0 });
			Tuning.Area.Add(ArchetypeTestId(TEXT("test_rampage")), RampageSweep);

			// A shell that hits harder the more Health its target already lacks.
			FVeyraAreaAbilityTuning Shell = Sweep;
			Shell.Zones[0].CasterStatusesPerVanguard.Reset();
			Shell.Zones[0].Effects.Damage.Add(FVeyraDamageTuning{ EVeyraDamageType::TrueDamage, { InnerDamage }, 0.0, 0.0 });
			Shell.Zones[0].Effects.MissingHealthDamage.Add(FVeyraMissingHealthDamageTuning{ EVeyraDamageType::TrueDamage, MissingHealthRatio });
			Tuning.Area.Add(ArchetypeTestId(TEXT("test_shell")), Shell);
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(&Tuning);

			FArchetypeTestWorld World{ Spawner };
			Caster = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
		}

		AFTER_EACH()
		{
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(nullptr);
		}

		TEST_METHOD(EachUnitTakesTheInnermostZoneThatTouchesIt)
		{
			FArchetypeTestWorld World{ Spawner };
			ASSERT_THAT(IsTrue(World.Learn(*Caster, EVeyraAbilitySlot::W, ArchetypeTestId(TEXT("test_slam")))));
			AVeyraVanguardCharacter& Near = World.Spawn(EVeyraTeam::B, FVector(InnerRadius / 2.0, 0.0, 0.0));
			AVeyraVanguardCharacter& Middle = World.Spawn(EVeyraTeam::B, FVector(0.0, (InnerRadius + OuterRadius) / 2.0, 0.0));
			AVeyraVanguardCharacter& Away = World.Spawn(EVeyraTeam::B, FVector(-OuterRadius * 2.0, 0.0, 0.0));
			AVeyraVanguardCharacter& Friend = World.Spawn(EVeyraTeam::A, FVector(0.0, -InnerRadius / 2.0, 0.0));

			ASSERT_THAT(IsTrue(World.CastAt(*Caster, EVeyraAbilitySlot::W, FVector(100.0, 0.0, 0.0)) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(World.Has(Near, TEXT("test_stun")) && !World.Has(Near, TEXT("test_slow"))));
			const double Expected = InnerDamage + VeyraCombatTests::ExampleStats().PhysicalPower * PowerRatio;
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(World.HealthLost(Near), Expected, 1e-3), TEXT("damage is the rank's amount plus power times the ratio")));
			ASSERT_THAT(IsTrue(World.Has(Middle, TEXT("test_slow")) && !World.Has(Middle, TEXT("test_stun"))));
			ASSERT_THAT(IsTrue(World.HealthLost(Middle) == 0.0));
			ASSERT_THAT(IsFalse(World.Has(Away, TEXT("test_slow")) || World.Has(Friend, TEXT("test_slow")) || World.Has(Friend, TEXT("test_stun"))));
		}

		TEST_METHOD(ASpellShieldBlocksTheWholeHitAndTheNextLands)
		{
			FArchetypeTestWorld World{ Spawner };
			ASSERT_THAT(IsTrue(World.Learn(*Caster, EVeyraAbilitySlot::W, ArchetypeTestId(TEXT("test_slam")))));
			// A second caster's hit follows the first.
			AVeyraVanguardCharacter& Second = World.Spawn(EVeyraTeam::A, FVector(0.0, -InnerRadius, 0.0));
			ASSERT_THAT(IsTrue(World.Learn(Second, EVeyraAbilitySlot::W, ArchetypeTestId(TEXT("test_mortar")))));
			AVeyraVanguardCharacter& Shielded = World.Spawn(EVeyraTeam::B, FVector(InnerRadius / 2.0, 0.0, 0.0));
			FVeyraStatusSpec Ward;
			Ward.Id = ArchetypeTestId(TEXT("test_ward"));
			Ward.Kind = EVeyraStatusKind::SpellShield;
			Ward.DurationSeconds = LongSeconds;
			UAbilitySystemComponent& Target = *Shielded.GetAbilitySystemComponent();
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(Target, Target, Ward)));

			ASSERT_THAT(IsTrue(World.CastAt(*Caster, EVeyraAbilitySlot::W, Shielded.GetActorLocation()) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(World.HealthLost(Shielded) == 0.0 && !World.Has(Shielded, TEXT("test_stun")), TEXT("no damage and no status (Combat Bible §19)")));
			ASSERT_THAT(IsFalse(World.Has(Shielded, TEXT("test_ward")), TEXT("the shield is spent")));
			ASSERT_THAT(IsTrue(World.CastAt(Second, EVeyraAbilitySlot::W, Shielded.GetActorLocation()) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(World.HealthLost(Shielded) > 0.0 && World.Has(Shielded, TEXT("test_stun")), TEXT("the next hit lands")));
		}

		TEST_METHOD(APointBeyondRangeIsBroughtWithinIt)
		{
			FArchetypeTestWorld World{ Spawner };
			ASSERT_THAT(IsTrue(World.Learn(*Caster, EVeyraAbilitySlot::W, ArchetypeTestId(TEXT("test_mortar")))));
			AVeyraVanguardCharacter& AtRange = World.Spawn(EVeyraTeam::B, FVector(CastRange, 0.0, 0.0));
			ASSERT_THAT(IsTrue(World.CastAt(*Caster, EVeyraAbilitySlot::W, FVector(CastRange * 3.0, 0.0, 0.0)) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(World.Has(AtRange, TEXT("test_stun"))));
		}

		TEST_METHOD(AGroundPointAreaNeedsAPoint)
		{
			FArchetypeTestWorld World{ Spawner };
			ASSERT_THAT(IsTrue(World.Learn(*Caster, EVeyraAbilitySlot::W, ArchetypeTestId(TEXT("test_mortar")))));
			ASSERT_THAT(IsTrue(VeyraAbilities::TryCast(*Caster->GetAbilitySystemComponent(), EVeyraAbilitySlot::W, FVeyraCastTarget()) == EVeyraCastRejection::InvalidLocation));
		}

		TEST_METHOD(ADelayedAreaWaitsAndShowsWhereItWillHit)
		{
			FArchetypeTestWorld World{ Spawner };
			ASSERT_THAT(IsTrue(World.Learn(*Caster, EVeyraAbilitySlot::W, ArchetypeTestId(TEXT("test_flare")))));
			const FVector Point(CastRange / 2.0, 0.0, 0.0);
			AVeyraVanguardCharacter& Target = World.Spawn(EVeyraTeam::B, Point);
			ASSERT_THAT(IsTrue(World.CastAt(*Caster, EVeyraAbilitySlot::W, Point) == EVeyraCastRejection::None));
			ASSERT_THAT(IsFalse(World.Has(Target, TEXT("test_stun")), TEXT("the area hit before its delay")));
			TActorIterator<AVeyraDelayedArea> Delayed(&Spawner.GetWorld());
			ASSERT_THAT(IsTrue(Delayed && FVector::Dist2D(Delayed->GetActorLocation(), Point) < 1.0));
			ASSERT_THAT(IsTrue(Delayed->GetShapes().Num() == 1 && Delayed->GetVeyraTeam() == EVeyraTeam::A));
		}

		TEST_METHOD(ACatchOnAnEnemyVanguardGivesTheCasterItsZoneStatuses)
		{
			FArchetypeTestWorld World{ Spawner };
			ASSERT_THAT(IsTrue(World.Learn(*Caster, EVeyraAbilitySlot::E, ArchetypeTestId(TEXT("test_sweep")))));
			World.SpawnFluxborn(EVeyraTeam::B, FVector(InnerRadius, 0.0, 0.0));
			ASSERT_THAT(IsTrue(World.CastAt(*Caster, EVeyraAbilitySlot::E, FVector(100.0, 0.0, 0.0)) == EVeyraCastRejection::None));
			ASSERT_THAT(IsFalse(World.Has(*Caster, TEXT("test_frenzy")), TEXT("a unit that is not a Vanguard gives nothing")));

			World.Spawn(EVeyraTeam::B, FVector(0.0, InnerRadius, 0.0));
			ASSERT_THAT(IsTrue(World.CastAt(*Caster, EVeyraAbilitySlot::E, FVector(100.0, 0.0, 0.0)) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(World.Has(*Caster, TEXT("test_frenzy"))));
		}

		TEST_METHOD(ATakedownKeepsItsExtensionThroughTheSameCastsOtherCatches)
		{
			FArchetypeTestWorld World{ Spawner };
			ASSERT_THAT(IsTrue(World.Learn(*Caster, EVeyraAbilitySlot::E, ArchetypeTestId(TEXT("test_rampage")))));
			// The nearer is resolved first, with less Health left than the sweep deals; the farther survives it.
			AVeyraVanguardCharacter& Doomed = World.Spawn(EVeyraTeam::B, FVector(InnerRadius / 2.0, 0.0, 0.0));
			AVeyraVanguardCharacter& Survivor = World.Spawn(EVeyraTeam::B, FVector(0.0, InnerRadius, 0.0));
			FVeyraRawDamageEvent Earlier;
			Earlier.Components.Add({ EVeyraDamageType::TrueDamage, VeyraCombatTests::ExampleStats().MaxHealth - InnerDamage / 2.0 });
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*Caster->GetAbilitySystemComponent(), *Doomed.GetAbilitySystemComponent(), Earlier)));

			ASSERT_THAT(IsTrue(World.CastAt(*Caster, EVeyraAbilitySlot::E, FVector(100.0, 0.0, 0.0)) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(!VeyraTargeting::IsAlive(&Doomed) && VeyraTargeting::IsAlive(&Survivor)));
			const UVeyraStatusComponent* Statuses = Caster->GetPlayerState()->FindComponentByClass<UVeyraStatusComponent>();
			const FVeyraContentId FrenzyId = ArchetypeTestId(TEXT("test_rampage_frenzy"));
			const FVeyraStatusEntry* Frenzy = Statuses->GetLedger().Entries.FindByPredicate([&FrenzyId](const FVeyraStatusEntry& Entry) { return Entry.Id == FrenzyId; });
			ASSERT_THAT(IsNotNull(Frenzy));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Frenzy->EndsAt - Frenzy->StartedAt, LongSeconds + TakedownExtension, 1e-3),
				FString::Printf(TEXT("the frenzy lasts %g s"), Frenzy->EndsAt - Frenzy->StartedAt)));
		}

		TEST_METHOD(AHitGrowsWithTheHealthItsTargetLacks)
		{
			// Fixture value: the Health the target has already lost.
			constexpr double Wound = 200.0;
			FArchetypeTestWorld World{ Spawner };
			ASSERT_THAT(IsTrue(World.Learn(*Caster, EVeyraAbilitySlot::E, ArchetypeTestId(TEXT("test_shell")))));
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(InnerRadius, 0.0, 0.0));
			FVeyraRawDamageEvent Earlier;
			Earlier.Components.Add({ EVeyraDamageType::TrueDamage, Wound });
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*Caster->GetAbilitySystemComponent(), *Enemy.GetAbilitySystemComponent(), Earlier)));

			ASSERT_THAT(IsTrue(World.CastAt(*Caster, EVeyraAbilitySlot::E, FVector(100.0, 0.0, 0.0)) == EVeyraCastRejection::None));
			// One hit: its amount, and the ratio of what was missing when it landed.
			const double Expected = Wound + InnerDamage + MissingHealthRatio * Wound;
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(World.HealthLost(Enemy), Expected, 1e-3), FString::Printf(TEXT("lost %g, expected %g"), World.HealthLost(Enemy), Expected)));
		}

		TEST_METHOD(APullStopsAtTheCastersEdge)
		{
			FArchetypeTestWorld World{ Spawner };
			ASSERT_THAT(IsTrue(World.Learn(*Caster, EVeyraAbilitySlot::W, ArchetypeTestId(TEXT("test_grasp")))));
			AVeyraVanguardCharacter& Target = World.Spawn(EVeyraTeam::B, FVector(OuterRadius / 2.0, 0.0, 0.0));
			ASSERT_THAT(IsTrue(World.CastAt(*Caster, EVeyraAbilitySlot::W, FVector(100.0, 0.0, 0.0)) == EVeyraCastRejection::None));
			const UVeyraMovementComponent* Movement = Target.GetVeyraMovement();
			ASSERT_THAT(IsTrue(Movement->IsDisplaced()));
			const double Touching = Caster->GetSimpleCollisionRadius() + Target.GetSimpleCollisionRadius();
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Movement->GetForcedMoveDestination()->X, Touching, 1.0)));
		}
	};

	// Veyra.Abilities.SelfBuff.*: the self-buff archetype's statuses, shield and ally aura, and ending
	// it early by casting again (ADR-008 §3, §9).
	TEST_CLASS(SelfBuff, "Veyra.Abilities")
	{
		static constexpr double ShieldBase = 50.0;
		static constexpr double MaxHealthRatio = 0.1;
		static constexpr double AuraRadius = 300.0;
		static constexpr double LongSeconds = 60.0;
		static constexpr double ResourceCost = 20.0;

		FActorTestSpawner Spawner;
		FVeyraAbilitiesTuning Tuning;
		AVeyraVanguardCharacter* Caster = nullptr;

		BEFORE_EACH()
		{
			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_bulwark")), StatusOf(EVeyraStatusKind::DamageReduction, 0.3, LongSeconds));
			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_heavy")), StatusOf(EVeyraStatusKind::MoveSpeed, -0.3, LongSeconds));
			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_cover")), StatusOf(EVeyraStatusKind::DamageReduction, 0.1, LongSeconds));

			FVeyraSelfBuffAbilityTuning Brace;
			Brace.Cast = InstantCast(0.0, LongSeconds, ResourceCost);
			Brace.Statuses = { ArchetypeTestId(TEXT("test_bulwark")), ArchetypeTestId(TEXT("test_heavy")) };
			FVeyraShieldTuning& Shield = Brace.Shields.AddDefaulted_GetRef();
			Shield.Id = ArchetypeTestId(TEXT("test_brace_shield"));
			Shield.Category = EVeyraShieldCategory::Universal;
			Shield.AmountByRank = { ShieldBase };
			Shield.MaxHealthRatio = MaxHealthRatio;
			Shield.DurationSeconds = LongSeconds;
			Brace.Aura.Add(FVeyraAuraTuning{ AuraRadius, LongSeconds, LongSeconds / 2.0, { ArchetypeTestId(TEXT("test_cover")) } });
			Brace.Recast = EVeyraRecast::EndsEarly;
			Tuning.SelfBuff.Add(ArchetypeTestId(TEXT("test_brace")), Brace);
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(&Tuning);

			FArchetypeTestWorld World{ Spawner };
			Caster = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			ASSERT_THAT(IsTrue(World.Learn(*Caster, EVeyraAbilitySlot::E, ArchetypeTestId(TEXT("test_brace")))));
		}

		AFTER_EACH()
		{
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(nullptr);
		}

		EVeyraCastRejection Cast() const
		{
			return VeyraAbilities::TryCast(*Caster->GetAbilitySystemComponent(), EVeyraAbilitySlot::E, FVeyraCastTarget());
		}

		TEST_METHOD(TheCasterGetsItsStatusesAndShield)
		{
			ASSERT_THAT(IsTrue(Cast() == EVeyraCastRejection::None));
			FArchetypeTestWorld World{ Spawner };
			ASSERT_THAT(IsTrue(World.Has(*Caster, TEXT("test_bulwark")) && World.Has(*Caster, TEXT("test_heavy"))));
			const TArray<FVeyraShieldEntry>& Shields = Caster->GetPlayerState()->FindComponentByClass<UVeyraDamageAbsorptionComponent>()->GetLedger().Shields;
			ASSERT_THAT(AreEqual(1, Shields.Num()));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Shields[0].Remaining, ShieldBase + VeyraCombatTests::ExampleStats().MaxHealth * MaxHealthRatio, 1e-3)));
		}

		TEST_METHOD(NearbyAlliedVanguardsShareTheAura)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Near = World.Spawn(EVeyraTeam::A, FVector(AuraRadius / 2.0, 0.0, 0.0));
			AVeyraVanguardCharacter& Far = World.Spawn(EVeyraTeam::A, FVector(AuraRadius * 3.0, 0.0, 0.0));
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(0.0, AuraRadius / 2.0, 0.0));
			ASSERT_THAT(IsTrue(Cast() == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(World.Has(Near, TEXT("test_cover"))));
			ASSERT_THAT(IsFalse(World.Has(Far, TEXT("test_cover")) || World.Has(Enemy, TEXT("test_cover")) || World.Has(*Caster, TEXT("test_cover"))));
		}

		TEST_METHOD(CastingAgainEndsItEarlyForFree)
		{
			ASSERT_THAT(IsTrue(Cast() == EVeyraCastRejection::None));
			const UAbilitySystemComponent& Unit = *Caster->GetAbilitySystemComponent();
			const double ResourceAfterFirst = Unit.GetNumericAttribute(UVeyraResourceSet::GetResourceAttribute());
			ASSERT_THAT(IsTrue(Cast() == EVeyraCastRejection::None, TEXT("the recast should pass the cooldown")));
			FArchetypeTestWorld World{ Spawner };
			ASSERT_THAT(IsFalse(World.Has(*Caster, TEXT("test_bulwark")) || World.Has(*Caster, TEXT("test_heavy"))));
			ASSERT_THAT(IsTrue(Unit.GetNumericAttribute(UVeyraResourceSet::GetResourceAttribute()) == ResourceAfterFirst));
			ASSERT_THAT(IsTrue(Cast() == EVeyraCastRejection::OnCooldown, TEXT("once ended, the cooldown applies again")));
		}
	};

	// Veyra.Abilities.SpellArchetypes.*: what the Flux Spells ask of the archetypes: a spell slot's rank
	// and fixed cooldown, a targeted ability's statuses and unit kinds, Level-scaled amounts, and the
	// heal (ADR-015 §1–§3).
	TEST_CLASS(SpellArchetypes, "Veyra.Abilities")
	{
		// Fixture values, independent of the committed Abilities.json.
		static constexpr double CastRange = 600.0;
		static constexpr double Near = 200.0;
		static constexpr double Cooldown = 90.0;
		static constexpr double SmiteDamage = 300.0;
		static constexpr double BurnPerTick = 10.0;
		static constexpr double BurnPerLevel = 2.0;
		static constexpr double HealAmount = 100.0;
		static constexpr double HealPerLevel = 10.0;
		static constexpr double AllyRange = 500.0;
		static constexpr double LongSeconds = 60.0;
		static constexpr double AbilityHaste = 100.0;

		FActorTestSpawner Spawner;
		FVeyraAbilitiesTuning Tuning;
		AVeyraVanguardCharacter* Caster = nullptr;

		BEFORE_EACH()
		{
			FVeyraStatusTuning Burn = StatusOf(EVeyraStatusKind::DamageOverTime, BurnPerTick, LongSeconds);
			Burn.DamageOverTime.Add(FVeyraDamageOverTimeTuning{ EVeyraDamageType::TrueDamage, 1.0, BurnPerLevel });
			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_burn")), Burn);
			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_rush")), StatusOf(EVeyraStatusKind::MoveSpeed, 0.3, LongSeconds));

			FVeyraTargetedDamageAbilityTuning Smite;
			Smite.CastRange = CastRange;
			Smite.CooldownSeconds = Cooldown;
			Smite.DamageType = EVeyraDamageType::TrueDamage;
			Smite.DamageAmount = SmiteDamage;
			Smite.TargetKinds = { EVeyraUnitKind::Wildlife, EVeyraUnitKind::Objective, EVeyraUnitKind::Fluxborn };
			Tuning.TargetedDamage.Add(ArchetypeTestId(TEXT("test_smite")), Smite);

			FVeyraTargetedDamageAbilityTuning Ignite;
			Ignite.CastRange = CastRange;
			Ignite.CooldownSeconds = Cooldown;
			Ignite.DamageType = EVeyraDamageType::TrueDamage;
			Ignite.Statuses = { ArchetypeTestId(TEXT("test_burn")) };
			Ignite.TargetKinds = { EVeyraUnitKind::Vanguard };
			Tuning.TargetedDamage.Add(ArchetypeTestId(TEXT("test_ignite")), Ignite);

			FVeyraSelfBuffAbilityTuning Mend;
			Mend.Cast = InstantCast(0.0, Cooldown, 0.0);
			Mend.Heal.Add(FVeyraHealTuning{ HealAmount, HealPerLevel, AllyRange, { ArchetypeTestId(TEXT("test_rush")) } });
			Tuning.SelfBuff.Add(ArchetypeTestId(TEXT("test_mend")), Mend);
			Tuning.FluxSpells.Roster = { ArchetypeTestId(TEXT("test_smite")), ArchetypeTestId(TEXT("test_ignite")), ArchetypeTestId(TEXT("test_mend")) };
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(&Tuning);

			FArchetypeTestWorld World{ Spawner };
			Caster = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			// Its team's permanent Flux has opened both slots, as Match would set it (ADR-015 §4).
			LoadoutOf(*Caster).SetUnlockedSpellSlots(UE_ARRAY_COUNT(VeyraAbilitySlots::Spells));
		}

		AFTER_EACH()
		{
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(nullptr);
		}

		static UVeyraAbilityLoadoutComponent& LoadoutOf(AVeyraVanguardCharacter& Vanguard)
		{
			return *Vanguard.GetPlayerState()->FindComponentByClass<UVeyraAbilityLoadoutComponent>();
		}

		EVeyraCastRejection CastAtUnit(EVeyraAbilitySlot Slot, AActor& Unit) const
		{
			FVeyraCastTarget Target;
			Target.Actor = &Unit;
			Target.bHasLocation = true;
			Target.Location = Unit.GetActorLocation();
			return VeyraAbilities::TryCast(*Caster->GetAbilitySystemComponent(), Slot, Target);
		}

		/** Takes Amount of Unit's Health, as a hazard on no side would. */
		void Hurt(AActor& Unit, double Amount)
		{
			UAbilitySystemComponent& Hazard = VeyraCombatTests::SpawnCombatant(Spawner);
			VeyraCombat::InitializeStats(Hazard, VeyraCombatTests::ExampleStats());
			FVeyraRawDamageEvent Damage;
			Damage.Components.Add({ EVeyraDamageType::TrueDamage, Amount });
			VeyraCombat::DealDamage(Hazard, *UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Unit), Damage);
		}

		TEST_METHOD(ASpellSlotCastsAtRankOneOnAFixedCooldown)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraTestFluxborn& Minion = World.SpawnFluxborn(EVeyraTeam::B, FVector(Near, 0.0, 0.0));
			ASSERT_THAT(IsTrue(World.Equip(*Caster, EVeyraAbilitySlot::Spell1, ArchetypeTestId(TEXT("test_smite")))));
			// Haste that would halve an ability's cooldown leaves a spell's alone (Combat Bible §21).
			Caster->GetAbilitySystemComponent()->SetNumericAttributeBase(UVeyraOffenceSet::GetAbilityHasteAttribute(), static_cast<float>(AbilityHaste));
			ASSERT_THAT(IsTrue(CastAtUnit(EVeyraAbilitySlot::Spell1, Minion) == EVeyraCastRejection::None, TEXT("a spell needs no rank")));
			const UVeyraCooldownComponent& Cooldowns = *Caster->GetPlayerState()->FindComponentByClass<UVeyraCooldownComponent>();
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Cooldowns.GetDurationSeconds(ArchetypeTestId(TEXT("test_smite"))), Cooldown),
				FString::Printf(TEXT("cooldown %.1f s"), Cooldowns.GetDurationSeconds(ArchetypeTestId(TEXT("test_smite"))))));
		}

		TEST_METHOD(ALockedSpellSlotRefusesWhateverItHolds)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Newcomer = World.Spawn(EVeyraTeam::A, FVector(0.0, Near, 0.0));
			ASSERT_THAT(IsTrue(World.Equip(Newcomer, EVeyraAbilitySlot::Spell1, ArchetypeTestId(TEXT("test_mend")))));
			ASSERT_THAT(IsTrue(World.Equip(Newcomer, EVeyraAbilitySlot::Spell2, ArchetypeTestId(TEXT("test_smite")))));
			const auto CastMend = [&Newcomer](EVeyraAbilitySlot Slot) { return VeyraAbilities::TryCast(*Newcomer.GetAbilitySystemComponent(), Slot, FVeyraCastTarget()); };
			ASSERT_THAT(IsTrue(CastMend(EVeyraAbilitySlot::Spell1) == EVeyraCastRejection::Locked, TEXT("no permanent Flux yet")));
			UVeyraAbilityLoadoutComponent& Loadout = LoadoutOf(Newcomer);
			Loadout.SetUnlockedSpellSlots(1);
			ASSERT_THAT(IsTrue(CastMend(EVeyraAbilitySlot::Spell1) == EVeyraCastRejection::None, TEXT("the first slot opens first")));
			ASSERT_THAT(IsTrue(CastMend(EVeyraAbilitySlot::Spell2) == EVeyraCastRejection::Locked, TEXT("locked whatever it holds, target or none")));
			Loadout.SetUnlockedSpellSlots(0);
			ASSERT_THAT(AreEqual(1, Loadout.GetUnlockedSpellSlots(), TEXT("an unlock lasts the match")));
		}

		TEST_METHOD(ATargetedSpellHitsOnlyTheKindsItNames)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(Near, 0.0, 0.0));
			AVeyraTestFluxborn& Minion = World.SpawnFluxborn(EVeyraTeam::B, FVector(0.0, Near, 0.0));
			ASSERT_THAT(IsTrue(World.Equip(*Caster, EVeyraAbilitySlot::Spell1, ArchetypeTestId(TEXT("test_smite")))));
			ASSERT_THAT(IsTrue(World.Equip(*Caster, EVeyraAbilitySlot::Spell2, ArchetypeTestId(TEXT("test_ignite")))));
			ASSERT_THAT(IsTrue(CastAtUnit(EVeyraAbilitySlot::Spell1, Enemy) == EVeyraCastRejection::InvalidTarget, TEXT("a smite spares Vanguards")));
			ASSERT_THAT(IsTrue(CastAtUnit(EVeyraAbilitySlot::Spell2, Minion) == EVeyraCastRejection::InvalidTarget, TEXT("an ignite is for Vanguards")));
			ASSERT_THAT(IsTrue(CastAtUnit(EVeyraAbilitySlot::Spell1, Minion) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(World.HealthLost(Minion), SmiteDamage, 1e-3)));
		}

		TEST_METHOD(ASpellShieldBlocksATargetedSpellWhichStaysSpent)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(Near, 0.0, 0.0));
			ASSERT_THAT(IsTrue(World.Equip(*Caster, EVeyraAbilitySlot::Spell2, ArchetypeTestId(TEXT("test_ignite")))));
			FVeyraStatusSpec Ward;
			Ward.Id = ArchetypeTestId(TEXT("test_ward"));
			Ward.Kind = EVeyraStatusKind::SpellShield;
			Ward.DurationSeconds = LongSeconds;
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Enemy.GetAbilitySystemComponent(), *Enemy.GetAbilitySystemComponent(), Ward)));
			ASSERT_THAT(IsTrue(CastAtUnit(EVeyraAbilitySlot::Spell2, Enemy) == EVeyraCastRejection::None));
			ASSERT_THAT(IsFalse(World.Has(Enemy, TEXT("test_burn")) || World.Has(Enemy, TEXT("test_ward")), TEXT("blocked, and the shield spent")));
			const UVeyraCooldownComponent& Cooldowns = *Caster->GetPlayerState()->FindComponentByClass<UVeyraCooldownComponent>();
			ASSERT_THAT(IsTrue(Cooldowns.GetRemainingSecondsNow(ArchetypeTestId(TEXT("test_ignite"))) > 0.0, TEXT("the cast stays spent (Combat Bible §54)")));
		}

		TEST_METHOD(AnIgniteBurnsByItsCastersLevel)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(Near, 0.0, 0.0));
			ASSERT_THAT(IsTrue(World.Equip(*Caster, EVeyraAbilitySlot::Spell2, ArchetypeTestId(TEXT("test_ignite")))));
			UVeyraProgressionComponent& Progression = *Caster->GetPlayerState()->FindComponentByClass<UVeyraProgressionComponent>();
			Progression.AddExperience(UVeyraProgressionTuningSubsystem::Get().Experience.ToNextLevel[0]);
			const int32 Level = Progression.GetLevel();
			ASSERT_THAT(IsTrue(Level > 1));
			ASSERT_THAT(IsTrue(CastAtUnit(EVeyraAbilitySlot::Spell2, Enemy) == EVeyraCastRejection::None));
			const UVeyraStatusComponent& Statuses = *Enemy.GetPlayerState()->FindComponentByClass<UVeyraStatusComponent>();
			const FVeyraStatusEntry* Burning = Statuses.GetLedger().Entries.FindByPredicate([](const FVeyraStatusEntry& Entry) { return Entry.Kind == EVeyraStatusKind::DamageOverTime; });
			ASSERT_THAT(IsNotNull(Burning));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Burning->Magnitude, BurnPerTick + BurnPerLevel * (Level - 1)), TEXT("its ticks are fixed at the caster's Level")));
			ASSERT_THAT(IsTrue(World.HealthLost(Enemy) == 0.0, TEXT("nothing lands at once")));
		}

		TEST_METHOD(AMendHealsTheCasterAndTheMostWoundedAllyInRange)
		{
			// Fixture values: the Health each lacks.
			constexpr double CasterLacks = 50.0;
			constexpr double MostWoundedLacks = 300.0;
			constexpr double LessWoundedLacks = 150.0;
			constexpr double FarLacks = 400.0;
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& MostWounded = World.Spawn(EVeyraTeam::A, FVector(AllyRange / 2.0, 0.0, 0.0));
			AVeyraVanguardCharacter& LessWounded = World.Spawn(EVeyraTeam::A, FVector(0.0, AllyRange / 4.0, 0.0));
			AVeyraVanguardCharacter& Far = World.Spawn(EVeyraTeam::A, FVector(AllyRange * 3.0, 0.0, 0.0));
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(0.0, -AllyRange / 4.0, 0.0));
			Hurt(*Caster, CasterLacks);
			Hurt(MostWounded, MostWoundedLacks);
			Hurt(LessWounded, LessWoundedLacks);
			Hurt(Far, FarLacks);
			Hurt(Enemy, FarLacks);
			ASSERT_THAT(IsTrue(World.Equip(*Caster, EVeyraAbilitySlot::Spell1, ArchetypeTestId(TEXT("test_mend")))));
			ASSERT_THAT(IsTrue(VeyraAbilities::TryCast(*Caster->GetAbilitySystemComponent(), EVeyraAbilitySlot::Spell1, FVeyraCastTarget()) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(World.HealthLost(*Caster) == 0.0, TEXT("healed to full, and no further")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(World.HealthLost(MostWounded), MostWoundedLacks - HealAmount, 1e-3)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(World.HealthLost(LessWounded), LessWoundedLacks, 1e-3), TEXT("one ally only")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(World.HealthLost(Far), FarLacks, 1e-3) && FMath::IsNearlyEqual(World.HealthLost(Enemy), FarLacks, 1e-3)));
			ASSERT_THAT(IsTrue(World.Has(*Caster, TEXT("test_rush")) && World.Has(MostWounded, TEXT("test_rush"))));
			ASSERT_THAT(IsFalse(World.Has(LessWounded, TEXT("test_rush"))));
		}

		TEST_METHOD(TheRosterNamesSingleRankAbilitiesThatExist)
		{
			const int32 BasicRanks[] = { 5 };
			ASSERT_THAT(IsTrue(VeyraAbilityRules::Validate(Tuning, BasicRanks).IsEmpty(), FString::Join(VeyraAbilityRules::Validate(Tuning, BasicRanks), TEXT(" | "))));
			FVeyraAbilitiesTuning Broken = Tuning;
			Broken.SelfBuff[ArchetypeTestId(TEXT("test_mend"))].Cast.CooldownSecondsByRank = { 5.0, 4.0, 3.0, 2.0, 1.0 };
			Broken.FluxSpells.Roster.Add(ArchetypeTestId(TEXT("test_nothing")));
			Broken.Statuses[ArchetypeTestId(TEXT("test_burn"))].DamageOverTime.Reset();
			Broken.TargetedDamage[ArchetypeTestId(TEXT("test_ignite"))].Statuses.Reset();
			const TArray<FString> Problems = VeyraAbilityRules::Validate(Broken, BasicRanks);
			const FString All = FString::Join(Problems, TEXT(" | "));
			const auto Mentions = [&Problems](const TCHAR* Text) { return Problems.ContainsByPredicate([Text](const FString& Problem) { return Problem.Contains(Text); }); };
			ASSERT_THAT(IsTrue(Mentions(TEXT("/fluxSpells/roster/2: \"test_mend\" has ranks")), All));
			ASSERT_THAT(IsTrue(Mentions(TEXT("/fluxSpells/roster/3: names \"test_nothing\"")), All));
			ASSERT_THAT(IsTrue(Mentions(TEXT("/statuses/test_burn/damageOverTime")), All));
			ASSERT_THAT(IsTrue(Mentions(TEXT("/targetedDamage/test_ignite/damageAmount: deals no damage and applies no status")), All));
		}
	};

	// Veyra.Abilities.Casting.*: rules every cast shares, and the checks on Abilities tuning (ADR-008 §3, §4, §7).
	TEST_CLASS(Casting, "Veyra.Abilities")
	{
		FActorTestSpawner Spawner;
		FVeyraAbilitiesTuning Tuning;

		BEFORE_EACH()
		{
			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_slow")), StatusOf(EVeyraStatusKind::Slow, 0.3, 1.0));
			FVeyraAreaAbilityTuning Slam;
			Slam.Cast = InstantCast(0.0, 1.0, 0.0);
			FVeyraAreaZoneTuning& Zone = Slam.Zones.AddDefaulted_GetRef();
			Zone.Shape = CircleOf(100.0);
			Zone.Effects.Statuses.Add(ArchetypeTestId(TEXT("test_slow")));
			Tuning.Area.Add(ArchetypeTestId(TEXT("test_slam")), Slam);
		}

		AFTER_EACH()
		{
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(nullptr);
		}

		static bool Mentions(const TArray<FString>& Problems, const TCHAR* Fragment)
		{
			return Problems.ContainsByPredicate([Fragment](const FString& Problem) { return Problem.Contains(Fragment); });
		}

		TEST_METHOD(ACastThatHoldsTheCasterRefusesAnother)
		{
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(&Tuning);
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Caster = World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			ASSERT_THAT(IsTrue(World.Learn(Caster, EVeyraAbilitySlot::W, ArchetypeTestId(TEXT("test_slam")))));
			FVeyraCastState Winding;
			Winding.Phase = EVeyraCastPhase::Windup;
			Caster.GetPlayerState()->FindComponentByClass<UVeyraCastStateComponent>()->SetState(Winding);
			ASSERT_THAT(IsTrue(World.CastAt(Caster, EVeyraAbilitySlot::W, FVector::ZeroVector) == EVeyraCastRejection::Busy));
		}

		TEST_METHOD(ValueAtRankReadsOneForAllOrOnePerRank)
		{
			ASSERT_THAT(IsTrue(VeyraAbilityRules::ValueAtRank({ 7.0 }, 3) == 7.0));
			ASSERT_THAT(IsTrue(VeyraAbilityRules::ValueAtRank({ 1.0, 2.0, 3.0 }, 2) == 2.0));
			ASSERT_THAT(IsTrue(VeyraAbilityRules::ValueAtRank({ 1.0, 2.0, 3.0 }, 4) == 0.0));
		}

		TEST_METHOD(ValidationCatchesWhatTheSchemaCannot)
		{
			constexpr int32 RankCounts[] = { 5, 3 };
			ASSERT_THAT(IsTrue(VeyraAbilityRules::Validate(Tuning, RankCounts).IsEmpty(), FString::Join(VeyraAbilityRules::Validate(Tuning, RankCounts), TEXT(" | "))));

			FVeyraAbilitiesTuning Broken = Tuning;
			FVeyraAreaAbilityTuning& Slam = Broken.Area.FindChecked(ArchetypeTestId(TEXT("test_slam")));
			Slam.Cast.CooldownSecondsByRank = { 1.0, 2.0 };
			Slam.Zones[0].Effects.Statuses.Add(ArchetypeTestId(TEXT("no_such_status")));
			Slam.Zones[0].Shape.Radius = 0.0;
			// A reveal needs both a radius and a time (ADR-016 §5).
			Slam.Reveal.Radius = 400.0;
			Broken.SelfBuff.Add(ArchetypeTestId(TEXT("test_slam")), FVeyraSelfBuffAbilityTuning());
			Broken.Statuses.Add(ArchetypeTestId(TEXT("bad_stun")), StatusOf(EVeyraStatusKind::Stun, 0.5, 1.0));
			const TArray<FString> Problems = VeyraAbilityRules::Validate(Broken, RankCounts);
			const FString All = FString::Join(Problems, TEXT(" | "));
			ASSERT_THAT(IsTrue(Mentions(Problems, TEXT("/area/test_slam/cast/cooldownSecondsByRank:")), All));
			ASSERT_THAT(IsTrue(Mentions(Problems, TEXT("no_such_status")), All));
			ASSERT_THAT(IsTrue(Mentions(Problems, TEXT("/area/test_slam/zones/0/shape:")), All));
			ASSERT_THAT(IsTrue(Mentions(Problems, TEXT("/area/test_slam/reveal:")), All));
			ASSERT_THAT(IsTrue(Mentions(Problems, TEXT("an ability has one archetype")), All));
			ASSERT_THAT(IsTrue(Mentions(Problems, TEXT("/statuses/bad_stun:")), All));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
