// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER && WITH_VEYRA_UI

#include "Algo/Count.h"
#include "Components/ActorTestSpawner.h"
#include "Cues/VeyraCombatCues.h"
#include "GameFramework/Character.h"

namespace VeyraCombatCueTests
{
	// Veyra.UI.CombatCues.*: a fight's moments read from two sightings of a unit (ADR-063 §1).
	TEST_CLASS(CombatCues, "Veyra.UI")
	{
		// Fixture values: phase ends in server seconds, Health, and a damage amount.
		static constexpr double FirstEnd = 10.0;
		static constexpr double SecondEnd = 11.0;
		static constexpr double FullHealth = 500.0;
		static constexpr double Damage = 40.0;

		FActorTestSpawner Spawner;

		ACharacter& SpawnUnit()
		{
			return Spawner.SpawnActor<ACharacter>();
		}

		static FVeyraUnitSighting Standing()
		{
			FVeyraUnitSighting Sighting;
			Sighting.bAlive = true;
			Sighting.Vitality = FullHealth;
			Sighting.MaxHealth = FullHealth;
			return Sighting;
		}

		static FVeyraUnitSighting Attacking(EVeyraAttackPhase Phase, double EndsAt, const AActor* Target)
		{
			FVeyraUnitSighting Sighting = Standing();
			Sighting.AttackPhase = Phase;
			Sighting.AttackPhaseEndsAt = EndsAt;
			Sighting.AttackTarget = Target;
			return Sighting;
		}

		static int32 CountOf(const TArray<FVeyraCombatCue>& Cues, EVeyraCombatCueKind Kind)
		{
			return Algo::CountIf(Cues, [Kind](const FVeyraCombatCue& Cue) { return Cue.Kind == Kind; });
		}

		TEST_METHOD(AnAttacksWindupAndCommitAreCuesAndACancelIsNone)
		{
			const ACharacter& Unit = SpawnUnit();
			const ACharacter& Target = SpawnUnit();
			const FVeyraUnitSighting Idle = Standing();
			const FVeyraUnitSighting Windup = Attacking(EVeyraAttackPhase::Windup, FirstEnd, &Target);
			const FVeyraUnitSighting Backswing = Attacking(EVeyraAttackPhase::Backswing, FirstEnd, &Target);
			const TArray<FVeyraCombatCue> Began = VeyraCombatCues::Between(Unit, Idle, Windup);
			ASSERT_THAT(IsTrue(Began.Num() == 1 && Began[0].Kind == EVeyraCombatCueKind::AttackWindup && Began[0].Target.Get() == &Target));
			const TArray<FVeyraCombatCue> Committed = VeyraCombatCues::Between(Unit, Windup, Backswing);
			ASSERT_THAT(IsTrue(Committed.Num() == 1 && Committed[0].Kind == EVeyraCombatCueKind::AttackCommit && Committed[0].Unit.Get() == &Unit));
			ASSERT_THAT(IsTrue(VeyraCombatCues::Between(Unit, Backswing, Backswing).IsEmpty(), TEXT("nothing new, nothing raised")));
			ASSERT_THAT(IsTrue(VeyraCombatCues::Between(Unit, Windup, Idle).IsEmpty(), TEXT("a cancelled windup lands nothing")));
		}

		TEST_METHOD(ARisingLevelIsALevelUpAndALevelFirstSeenIsNot)
		{
			const ACharacter& Unit = SpawnUnit();
			FVeyraUnitSighting Before = Standing();
			Before.Level = 3;
			FVeyraUnitSighting After = Before;
			After.Level = 4;
			const TArray<FVeyraCombatCue> Cues = VeyraCombatCues::Between(Unit, Before, After);
			ASSERT_THAT(IsTrue(Cues.Num() == 1 && Cues[0].Kind == EVeyraCombatCueKind::LevelUp && Cues[0].Amount == 4.0 && Cues[0].Unit.Get() == &Unit));
			FVeyraUnitSighting Unseen = Standing();
			ASSERT_THAT(IsTrue(VeyraCombatCues::Between(Unit, Unseen, After).IsEmpty(), TEXT("a Level first seen is no level-up")));
			ASSERT_THAT(IsTrue(VeyraCombatCues::Between(Unit, After, After).IsEmpty()));
		}

		TEST_METHOD(AnAttackSeenOnlyInItsBackswingStillCommits)
		{
			// A fast attack's windup can fall between two updates: its new backswing ends at a new time.
			const ACharacter& Unit = SpawnUnit();
			const TArray<FVeyraCombatCue> Cues = VeyraCombatCues::Between(Unit, Attacking(EVeyraAttackPhase::Backswing, FirstEnd, nullptr),
				Attacking(EVeyraAttackPhase::Backswing, SecondEnd, nullptr));
			ASSERT_THAT(IsTrue(Cues.Num() == 1 && Cues[0].Kind == EVeyraCombatCueKind::AttackCommit));
		}

		TEST_METHOD(AHitIsHealthOrShieldsLost)
		{
			const ACharacter& Unit = SpawnUnit();
			const FVeyraUnitSighting Full = Standing();
			FVeyraUnitSighting Hurt = Full;
			Hurt.Vitality -= Damage;
			const TArray<FVeyraCombatCue> Hit = VeyraCombatCues::Between(Unit, Full, Hurt);
			ASSERT_THAT(IsTrue(Hit.Num() == 1 && Hit[0].Kind == EVeyraCombatCueKind::Hit));
			ASSERT_THAT(IsNear(Hit[0].Amount, Damage, static_cast<double>(UE_KINDA_SMALL_NUMBER)));
			ASSERT_THAT(IsTrue(VeyraCombatCues::Between(Unit, Hurt, Full).IsEmpty(), TEXT("healing is no hit")));
			FVeyraUnitSighting Smaller = Hurt;
			Smaller.MaxHealth -= Damage;
			ASSERT_THAT(IsTrue(VeyraCombatCues::Between(Unit, Full, Smaller).IsEmpty(), TEXT("Max Health lost with it is no hit")));
		}

		TEST_METHOD(ADeathEndsTheUnitsCues)
		{
			const ACharacter& Unit = SpawnUnit();
			FVeyraUnitSighting Dead = Attacking(EVeyraAttackPhase::Windup, FirstEnd, nullptr);
			Dead.bAlive = false;
			Dead.Vitality = 0.0;
			const TArray<FVeyraCombatCue> Cues = VeyraCombatCues::Between(Unit, Standing(), Dead);
			ASSERT_THAT(IsTrue(Cues.Num() == 2 && Cues[0].Kind == EVeyraCombatCueKind::Hit && Cues[1].Kind == EVeyraCombatCueKind::Death,
				TEXT("the fatal hit, then the death, and no attack")));
			ASSERT_THAT(IsTrue(VeyraCombatCues::Between(Unit, Dead, Dead).IsEmpty(), TEXT("the dead die once")));
		}

		TEST_METHOD(ACastsWindupAndEveryCommitAreCues)
		{
			const ACharacter& Unit = SpawnUnit();
			const FVeyraContentId Ability = FVeyraContentId::FromText(TEXT("a_cast")).GetValue();
			const FVector Aim(300.0, 0.0, 0.0);
			constexpr double WindupEndsAt = 12.5;
			const FVeyraUnitSighting Idle = Standing();
			FVeyraUnitSighting Windup = Idle;
			Windup.CastPhase = EVeyraCastPhase::Windup;
			Windup.CastId = 1;
			Windup.CastAbility = Ability;
			Windup.CastLocation = Aim;
			Windup.CastPhaseEndsAt = WindupEndsAt;
			const TArray<FVeyraCombatCue> Began = VeyraCombatCues::Between(Unit, Idle, Windup);
			ASSERT_THAT(IsTrue(Began.Num() == 1 && Began[0].Kind == EVeyraCombatCueKind::CastWindup && Began[0].Ability == Ability && Began[0].Location.Equals(Aim)));
			ASSERT_THAT(IsNear(Began[0].EndsAt, WindupEndsAt, 1e-6, TEXT("a cast's windup cue says when it commits (ADR-072 §3)")));
			ASSERT_THAT(IsTrue(VeyraCombatCues::Between(Unit, Windup, Windup).IsEmpty()));
			// A commit with no phase after it, as a cast with no windup, channel or recovery.
			FVeyraUnitSighting Committed = Idle;
			Committed.CommitSerial = 1;
			Committed.CommitAbility = Ability;
			Committed.CommitLocation = Aim;
			const TArray<FVeyraCombatCue> Cast = VeyraCombatCues::Between(Unit, Idle, Committed);
			ASSERT_THAT(IsTrue(Cast.Num() == 1 && Cast[0].Kind == EVeyraCombatCueKind::CastCommit && Cast[0].Ability == Ability && Cast[0].Location.Equals(Aim)));
			FVeyraUnitSighting Again = Committed;
			Again.CommitSerial = 2;
			ASSERT_THAT(IsTrue(CountOf(VeyraCombatCues::Between(Unit, Committed, Again), EVeyraCombatCueKind::CastCommit) == 1, TEXT("each commit, the same ability again too")));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER && WITH_VEYRA_UI
