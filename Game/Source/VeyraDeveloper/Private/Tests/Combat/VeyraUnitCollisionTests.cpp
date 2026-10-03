// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Components/ActorTestSpawner.h"
#include "Components/CapsuleComponent.h"
#include "CQTest.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Movement/VeyraUnitCollision.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraCombatTests
{
	// Veyra.Combat.UnitCollision.*: units collide by side (Combat Bible §24; ADR-062 §1). Enemies block each
	// other, allies pass through, neutral units block both sides, and a ghost passes everyone.
	TEST_CLASS(UnitCollision, "Veyra.Combat")
	{
		// Fixture values: two bodies this far apart along +X, high enough off any floor, and a step that
		// would carry the first through where the second stands.
		static constexpr double Apart = 300.0;
		static constexpr double Height = 200.0;
		static constexpr double Step = 600.0;
		static constexpr double QueryRadius = 1000.0;

		FActorTestSpawner Spawner;

		ACharacter& SpawnUnit(EVeyraTeam Team, double X)
		{
			ACharacter& Body = Spawner.SpawnActorAt<ACharacter>(FVector(X, 0.0, Height), FRotator::ZeroRotator);
			VeyraUnitCollision::ApplySide(*Body.GetCapsuleComponent(), Team);
			return Body;
		}

		/** Whether Mover, stepping along +X toward Other, is stopped by it. */
		static bool IsStoppedBy(ACharacter& Mover, const ACharacter& Other)
		{
			FHitResult Hit;
			Mover.GetCapsuleComponent()->MoveComponent(FVector(Step, 0.0, 0.0), FQuat::Identity, /*bSweep*/ true, &Hit);
			return Hit.bBlockingHit && Hit.GetActor() == &Other;
		}

		TEST_METHOD(EachSideHasItsChannelAndNeutralUnitsThePawns)
		{
			ASSERT_THAT(IsTrue(VeyraUnitCollision::ChannelOf(EVeyraTeam::A) == VeyraUnitCollision::SideA));
			ASSERT_THAT(IsTrue(VeyraUnitCollision::ChannelOf(EVeyraTeam::B) == VeyraUnitCollision::SideB));
			ASSERT_THAT(IsTrue(VeyraUnitCollision::ChannelOf(EVeyraTeam::None) == ECC_Pawn));
			UCapsuleComponent* Body = SpawnUnit(EVeyraTeam::A, 0.0).GetCapsuleComponent();
			ASSERT_THAT(IsTrue(Body->GetCollisionObjectType() == VeyraUnitCollision::SideA));
			ASSERT_THAT(IsTrue(Body->GetCollisionResponseToChannel(VeyraUnitCollision::SideA) == ECR_Ignore, TEXT("allies pass")));
			ASSERT_THAT(IsTrue(Body->GetCollisionResponseToChannel(VeyraUnitCollision::SideB) == ECR_Block, TEXT("enemies block")));
			ASSERT_THAT(IsTrue(Body->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block, TEXT("neutral units block, and the cursor still finds it")));
		}

		TEST_METHOD(AnAllyIsPassedThrough)
		{
			ACharacter& Mover = SpawnUnit(EVeyraTeam::A, 0.0);
			ACharacter& Ally = SpawnUnit(EVeyraTeam::A, Apart);
			ASSERT_THAT(IsFalse(IsStoppedBy(Mover, Ally)));
		}

		TEST_METHOD(AnEnemyBlocks)
		{
			ACharacter& Mover = SpawnUnit(EVeyraTeam::A, 0.0);
			ACharacter& Enemy = SpawnUnit(EVeyraTeam::B, Apart);
			ASSERT_THAT(IsTrue(IsStoppedBy(Mover, Enemy)));
		}

		TEST_METHOD(ANeutralUnitBlocksBothSidesAndEachOther)
		{
			// One row of mover and neutral unit per side, far enough apart that the rows never meet.
			double Row = 0.0;
			for (const EVeyraTeam Team : { EVeyraTeam::A, EVeyraTeam::B, EVeyraTeam::None })
			{
				ACharacter& Mover = Spawner.SpawnActorAt<ACharacter>(FVector(0.0, Row, Height), FRotator::ZeroRotator);
				VeyraUnitCollision::ApplySide(*Mover.GetCapsuleComponent(), Team);
				ACharacter& Neutral = Spawner.SpawnActorAt<ACharacter>(FVector(Apart, Row, Height), FRotator::ZeroRotator);
				VeyraUnitCollision::ApplySide(*Neutral.GetCapsuleComponent(), EVeyraTeam::None);
				ASSERT_THAT(IsTrue(IsStoppedBy(Mover, Neutral), FString::Printf(TEXT("a mover of side %d"), static_cast<int32>(Team))));
				Row += QueryRadius;
			}
		}

		TEST_METHOD(AGhostPassesEveryone)
		{
			ACharacter& Mover = SpawnUnit(EVeyraTeam::A, 0.0);
			ACharacter& Enemy = SpawnUnit(EVeyraTeam::B, Apart);
			VeyraUnitCollision::SetResponseToUnits(*Mover.GetCapsuleComponent(), ECR_Ignore);
			ASSERT_THAT(IsFalse(IsStoppedBy(Mover, Enemy)));
		}

		TEST_METHOD(AllUnitsFindsEverySide)
		{
			SpawnUnit(EVeyraTeam::A, 0.0);
			SpawnUnit(EVeyraTeam::B, Apart);
			SpawnUnit(EVeyraTeam::None, -Apart);
			TArray<FOverlapResult> Overlaps;
			Spawner.GetWorld().OverlapMultiByObjectType(Overlaps, FVector(0.0, 0.0, Height), FQuat::Identity, VeyraUnitCollision::AllUnits(),
				FCollisionShape::MakeSphere(QueryRadius));
			TSet<const AActor*> Found;
			for (const FOverlapResult& Overlap : Overlaps)
			{
				Found.Add(Overlap.GetActor());
			}
			ASSERT_THAT(AreEqual(Found.Num(), 3));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
