// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER && WITH_VEYRA_UI

#include "Components/ActorTestSpawner.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "Greybox/VeyraGreyboxSettings.h"
#include "Greybox/VeyraOrderMarks.h"

namespace VeyraOrderMarkTests
{
	// Veyra.UI.OrderMarks.*: a local order's mark (ADR-062 §6), from the committed grey-box settings. A ring closes on the
	// ground ordered, or round the unit an attack names, fading as it goes; Reduce Interface Animation keeps it still.
	TEST_CLASS(OrderMarks, "Veyra.UI")
	{
		// Fixture values: when the order was given, where, and an allowance for float sizes.
		static constexpr double GivenAt = 100.0;
		static constexpr double Slack = 0.01;
		inline static const FVector Where = FVector(300.0, -200.0, 40.0);

		FActorTestSpawner Spawner;

		static const UVeyraGreyboxSettings& Settings()
		{
			return *GetDefault<UVeyraGreyboxSettings>();
		}

		static FVeyraOrderMark MarkOf(EVeyraOrderMarkKind Kind, const AActor* Target = nullptr)
		{
			return FVeyraOrderMark{ Kind, Where, Target, GivenAt };
		}

		TEST_METHOD(AMoveMarkClosesAndFadesWhereItWent)
		{
			const FVeyraOrderMark Mark = MarkOf(EVeyraOrderMarkKind::Move);
			const TOptional<FVeyraOrderMarkRing> Start = VeyraOrderMarks::Describe(Mark, GivenAt, /*bStill*/ false, Settings());
			ASSERT_THAT(IsTrue(Start.IsSet()));
			ASSERT_THAT(IsTrue(Start->Centre.Equals(Where)));
			ASSERT_THAT(IsNear(Start->Radius, static_cast<double>(Settings().OrderMarkStartRadius), Slack));
			ASSERT_THAT(IsTrue(Start->Color.Equals(Settings().OrderMoveColor), TEXT("at full strength, in the move colour")));
			const TOptional<FVeyraOrderMarkRing> Halfway = VeyraOrderMarks::Describe(Mark, GivenAt + Settings().OrderMarkSeconds / 2.0, false, Settings());
			ASSERT_THAT(IsTrue(Halfway.IsSet()));
			ASSERT_THAT(IsTrue(Halfway->Radius < Start->Radius && Halfway->Radius > Settings().OrderMarkEndRadius, TEXT("closing")));
			ASSERT_THAT(IsTrue(Halfway->Color.A < Start->Color.A, TEXT("fading")));
			ASSERT_THAT(IsFalse(VeyraOrderMarks::Describe(Mark, GivenAt + Settings().OrderMarkSeconds, false, Settings()).IsSet(), TEXT("gone once its time is up")));
			ASSERT_THAT(IsFalse(VeyraOrderMarks::Describe(Mark, GivenAt - 1.0, false, Settings()).IsSet(), TEXT("nor shown before it was given")));
		}

		TEST_METHOD(AnAttackMoveMarkTakesTheAttackColour)
		{
			const TOptional<FVeyraOrderMarkRing> Ring = VeyraOrderMarks::Describe(MarkOf(EVeyraOrderMarkKind::AttackMove), GivenAt, false, Settings());
			ASSERT_THAT(IsTrue(Ring.IsSet() && Ring->Color.Equals(Settings().OrderAttackColor) && Ring->Centre.Equals(Where)));
		}

		TEST_METHOD(AnAttackMarkClosesOnItsUnitWhileItShows)
		{
			ACharacter& Unit = Spawner.SpawnActorAt<ACharacter>(Where + FVector(500.0, 0.0, 0.0), FRotator::ZeroRotator);
			const FVeyraOrderMark Mark = MarkOf(EVeyraOrderMarkKind::Attack, &Unit);
			const TOptional<FVeyraOrderMarkRing> Ring = VeyraOrderMarks::Describe(Mark, GivenAt, false, Settings());
			ASSERT_THAT(IsTrue(Ring.IsSet()));
			ASSERT_THAT(IsTrue(Ring->Centre.Equals(Unit.GetActorLocation()), TEXT("round the unit, not the ground")));
			const double Edge = Unit.GetCapsuleComponent()->GetScaledCapsuleRadius();
			ASSERT_THAT(IsNear(Ring->Radius, Edge + Settings().OrderMarkStartRadius, Slack));
			ASSERT_THAT(IsTrue(Ring->Color.Equals(Settings().OrderAttackColor)));
			Unit.SetActorHiddenInGame(true);
			ASSERT_THAT(IsFalse(VeyraOrderMarks::Describe(Mark, GivenAt, false, Settings()).IsSet(), TEXT("a hidden unit shows no mark")));
			ASSERT_THAT(IsFalse(VeyraOrderMarks::Describe(MarkOf(EVeyraOrderMarkKind::Attack), GivenAt, false, Settings()).IsSet(), TEXT("nor does a lost one")));
		}

		TEST_METHOD(AStillMarkKeepsItsEndRadiusAndOnlyFades)
		{
			const FVeyraOrderMark Mark = MarkOf(EVeyraOrderMarkKind::Move);
			const TOptional<FVeyraOrderMarkRing> Start = VeyraOrderMarks::Describe(Mark, GivenAt, /*bStill*/ true, Settings());
			const TOptional<FVeyraOrderMarkRing> Later = VeyraOrderMarks::Describe(Mark, GivenAt + Settings().OrderMarkSeconds / 2.0, true, Settings());
			ASSERT_THAT(IsTrue(Start.IsSet() && Later.IsSet()));
			ASSERT_THAT(IsNear(Start->Radius, static_cast<double>(Settings().OrderMarkEndRadius), Slack));
			ASSERT_THAT(IsNear(Later->Radius, Start->Radius, Slack));
			ASSERT_THAT(IsTrue(Later->Color.A < Start->Color.A));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER && WITH_VEYRA_UI
