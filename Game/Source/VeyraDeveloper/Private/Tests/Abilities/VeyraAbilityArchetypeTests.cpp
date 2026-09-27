// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Absorption/VeyraDamageAbsorptionComponent.h"
#include "Attributes/VeyraResourceSet.h"
#include "Casting/VeyraCastStateComponent.h"
#include "CQTest.h"
#include "Delivery/VeyraDelayedArea.h"
#include "EngineUtils.h"
#include "Life/VeyraLifeComponent.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"

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
		static constexpr double LongSeconds = 60.0;

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
			Broken.SelfBuff.Add(ArchetypeTestId(TEXT("test_slam")), FVeyraSelfBuffAbilityTuning());
			Broken.Statuses.Add(ArchetypeTestId(TEXT("bad_stun")), StatusOf(EVeyraStatusKind::Stun, 0.5, 1.0));
			const TArray<FString> Problems = VeyraAbilityRules::Validate(Broken, RankCounts);
			const FString All = FString::Join(Problems, TEXT(" | "));
			ASSERT_THAT(IsTrue(Mentions(Problems, TEXT("/area/test_slam/cast/cooldownSecondsByRank:")), All));
			ASSERT_THAT(IsTrue(Mentions(Problems, TEXT("no_such_status")), All));
			ASSERT_THAT(IsTrue(Mentions(Problems, TEXT("/area/test_slam/zones/0/shape:")), All));
			ASSERT_THAT(IsTrue(Mentions(Problems, TEXT("an ability has one archetype")), All));
			ASSERT_THAT(IsTrue(Mentions(Problems, TEXT("/statuses/bad_stun:")), All));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
