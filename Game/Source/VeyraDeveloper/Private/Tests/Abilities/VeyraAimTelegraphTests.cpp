// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER

#include "Casting/VeyraCastTelegraphs.h"
#include "Delivery/VeyraAreaDelivery.h"
#include "Tuning/VeyraAbilitiesTuning.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"

namespace VeyraAimTelegraphTests
{
	// Fixture values: a caster's body and an aim ahead of it.
	constexpr double CasterRadius = 40.0;
	const FVector Caster = FVector::ZeroVector;
	const FVector Aim(300.0, 0.0, 0.0);

	bool IsRing(const FVeyraPlacedShape& Placed, double Radius)
	{
		return Placed.Shape.Kind == EVeyraShapeKind::Circle && FMath::IsNearlyEqual(Placed.Shape.Radius, Radius) && Placed.Origin.Equals(Caster);
	}

	// Veyra.Abilities.AimTelegraphs.*: the indicator a player sees before casting shows the ability's range and where it
	// would land, as its delivery places it (ADR-040 §2).
	TEST_CLASS(AimTelegraphs, "Veyra.Abilities")
	{
		TEST_METHOD(ASkillshotShowsItsRangeThenItsPathTowardTheAim)
		{
			const FVeyraAbilitiesTuning& Tuning = UVeyraAbilitiesTuningSubsystem::Get();
			ASSERT_THAT(IsFalse(Tuning.Skillshot.IsEmpty()));
			for (const TPair<FVeyraContentId, FVeyraSkillshotAbilityTuning>& Skillshot : Tuning.Skillshot)
			{
				const TArray<FVeyraPlacedShape> Shapes = VeyraCastTelegraphs::ForAim(Tuning, Skillshot.Key, Caster, CasterRadius, Aim);
				ASSERT_THAT(IsFalse(Shapes.IsEmpty(), Skillshot.Key.ToString()));
				const double Range = Skillshot.Value.Cast.CastRange;
				ASSERT_THAT(IsTrue(!(Range > 0.0) || IsRing(Shapes[0], Range), Skillshot.Key.ToString()));
				const FVeyraPlacedShape& Path = Shapes.Last();
				ASSERT_THAT(IsTrue(Path.Shape.Kind == EVeyraShapeKind::Rectangle && FMath::IsNearlyEqual(Path.Shape.Length, Skillshot.Value.Projectile.Range)));
				ASSERT_THAT(IsTrue(Path.Direction.Equals(FVector::ForwardVector), TEXT("toward the aim")));
			}
		}

		TEST_METHOD(ATargetedSpellShowsOnlyItsRange)
		{
			const FVeyraAbilitiesTuning& Tuning = UVeyraAbilitiesTuningSubsystem::Get();
			ASSERT_THAT(IsFalse(Tuning.TargetedDamage.IsEmpty()));
			for (const TPair<FVeyraContentId, FVeyraTargetedDamageAbilityTuning>& Targeted : Tuning.TargetedDamage)
			{
				const TArray<FVeyraPlacedShape> Shapes = VeyraCastTelegraphs::ForAim(Tuning, Targeted.Key, Caster, CasterRadius, Aim);
				ASSERT_THAT(AreEqual(1, Shapes.Num(), Targeted.Key.ToString()));
				ASSERT_THAT(IsTrue(IsRing(Shapes[0], Targeted.Value.CastRange)));
			}
		}

		TEST_METHOD(AnAreaLandsWhereItsDeliveryPlacesIt)
		{
			const FVeyraAbilitiesTuning& Tuning = UVeyraAbilitiesTuningSubsystem::Get();
			ASSERT_THAT(IsFalse(Tuning.Area.IsEmpty()));
			for (const TPair<FVeyraContentId, FVeyraAreaAbilityTuning>& Area : Tuning.Area)
			{
				const TArray<FVeyraPlacedShape> Shapes = VeyraCastTelegraphs::ForAim(Tuning, Area.Key, Caster, CasterRadius, Aim);
				const FVeyraEffectFrame Placed = VeyraAreaDelivery::Place(Area.Value, Caster, Aim, FVector::ForwardVector);
				ASSERT_THAT(IsFalse(Shapes.IsEmpty(), Area.Key.ToString()));
				ASSERT_THAT(IsTrue(Shapes.Last().Origin.Equals(Placed.Origin), Area.Key.ToString()));
			}
		}

		TEST_METHOD(AnAimOnTheCasterStillFacesSomewhere)
		{
			const FVeyraAbilitiesTuning& Tuning = UVeyraAbilitiesTuningSubsystem::Get();
			const TPair<FVeyraContentId, FVeyraSkillshotAbilityTuning>& Skillshot = *Tuning.Skillshot.CreateConstIterator();
			const TArray<FVeyraPlacedShape> Shapes = VeyraCastTelegraphs::ForAim(Tuning, Skillshot.Key, Caster, CasterRadius, Caster);
			ASSERT_THAT(IsTrue(Shapes.Last().Direction.IsNormalized()));
		}

		TEST_METHOD(AnAbilityTheTuningLacksShowsNothing)
		{
			const FVeyraContentId Unknown = FVeyraContentId::FromText(TEXT("no_such_ability")).GetValue();
			ASSERT_THAT(IsTrue(VeyraCastTelegraphs::ForAim(UVeyraAbilitiesTuningSubsystem::Get(), Unknown, Caster, CasterRadius, Aim).IsEmpty()));
		}
	};
}

#endif
