// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Attacks/VeyraBasicAttackComponent.h"
#include "CQTest.h"
#include "Cooldowns/VeyraCooldownComponent.h"
#include "EngineUtils.h"
#include "Entities/VeyraPlacedMarker.h"
#include "Passives/VeyraQuarryPassive.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "Tuning/VeyraVanguardsTuningSubsystem.h"
#include "VeyraVanguards.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraVanguardsTests
{
	using VeyraAbilitiesTests::FArchetypeTestWorld;

	// Veyra.Vanguards.Quarry.*: Tavi's You're It! and her kit (Roster Bible §6; ADR-030 §10-§11), from the committed tuning.
	TEST_CLASS(Quarry, "Veyra.Vanguards")
	{
		// Fixture values: where enemies stand, a speed to move at, and a long cooldown to refund from.
		static constexpr double Near = 120.0;
		static constexpr double Moving = 300.0;
		static constexpr double LongCooldown = 20.0;
		static constexpr float Step = 0.05f;

		FActorTestSpawner Spawner;
		AVeyraVanguardCharacter* Tavi = nullptr;
		UVeyraQuarryPassive* Passive = nullptr;
		UVeyraBasicAttackComponent* Attacks = nullptr;

		BEFORE_EACH()
		{
			FArchetypeTestWorld World{ Spawner };
			Tavi = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			AVeyraPlayerState* Participant = Tavi->GetPlayerState<AVeyraPlayerState>();
			const FVeyraPreparedVanguard Prepared = VeyraVanguards::PrepareCombatant(*Participant->GetAbilitySystemComponent(), Id(TEXT("tavi")));
			ASSERT_THAT(IsTrue(Prepared.bPrepared));
			Participant->SetPassive(Prepared.Passive);
			Passive = Cast<UVeyraQuarryPassive>(Prepared.Passive);
			ASSERT_THAT(IsNotNull(Passive));
			Attacks = Participant->FindComponentByClass<UVeyraBasicAttackComponent>();
		}

		static FVeyraContentId Id(const TCHAR* Text)
		{
			return FVeyraContentId::FromText(Text).GetValue();
		}

		static const FVeyraQuarryTuning& Tuning()
		{
			return *UVeyraVanguardsTuningSubsystem::FindQuarry(Id(TEXT("tavi_youre_it")));
		}

		void MarkIt(AActor& Unit) const
		{
			const TOptional<FVeyraStatusSpec> Mark = UVeyraAbilitiesTuningSubsystem::FindStatus(Tuning().Mark);
			VeyraCombat::ApplyStatus(*Tavi->GetAbilitySystemComponent(), *UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Unit), Mark.GetValue());
		}

		bool IsIt(const AActor& Unit) const
		{
			return VeyraCombat::HasStatusFrom(&Unit, Tuning().Mark, *Tavi->GetAbilitySystemComponent());
		}

		void Wait(double Seconds)
		{
			UWorld& World = Spawner.GetWorld();
			const double Until = World.GetTimeSeconds() + Seconds;
			while (World.GetTimeSeconds() < Until)
			{
				World.Tick(LEVELTICK_TimeOnly, Step);
				++GFrameCounter;
				World.GetTimerManager().Tick(Step);
			}
		}

		EVeyraAttackRejection AttackNow(AActor& Target)
		{
			UWorld& World = Spawner.GetWorld();
			while (World.GetTimeSeconds() < Attacks->GetNextAttackAt())
			{
				World.Tick(LEVELTICK_TimeOnly, Step);
			}
			const EVeyraAttackRejection Rejection = Attacks->StartAttack(Target);
			if (Rejection == EVeyraAttackRejection::None)
			{
				Attacks->Commit();
			}
			return Rejection;
		}

		TEST_METHOD(OnlyOneEnemyIsItAtATime)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& First = World.Spawn(EVeyraTeam::B, FVector(Near, 0.0, 0.0));
			AVeyraVanguardCharacter& Second = World.Spawn(EVeyraTeam::B, FVector(0.0, Near, 0.0));
			MarkIt(First);
			ASSERT_THAT(IsTrue(Passive->GetQuarry() == &First));
			MarkIt(Second);
			ASSERT_THAT(IsTrue(Passive->GetQuarry() == &Second && !IsIt(First), TEXT("marking another takes it off the first")));
		}

		TEST_METHOD(SheSpeedsUpOnlyWhileClosingOnIt)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(Tuning().ChaseRange / 2.0, 0.0, 0.0));
			const FString Chase = Tuning().ChaseStatus.ToString();
			Tavi->GetVeyraMovement()->Velocity = FVector(Moving, 0.0, 0.0);
			Passive->Sample();
			ASSERT_THAT(IsFalse(FArchetypeTestWorld::Has(*Tavi, *Chase), TEXT("not without It")));
			MarkIt(Enemy);
			Passive->Sample();
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Has(*Tavi, *Chase), TEXT("closing on It")));
			Tavi->GetVeyraMovement()->Velocity = FVector(-Moving, 0.0, 0.0);
			Passive->Sample();
			ASSERT_THAT(IsFalse(FArchetypeTestWorld::Has(*Tavi, *Chase), TEXT("not moving away")));
		}

		TEST_METHOD(HerNextAttackOnItSpendsItForDamageAndHerCooldowns)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Plain = World.Spawn(EVeyraTeam::B, FVector(Near, 0.0, 0.0));
			AVeyraVanguardCharacter& Marked = World.Spawn(EVeyraTeam::B, FVector(0.0, Near, 0.0));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Learn(*Tavi, EVeyraAbilitySlot::Q, Id(TEXT("tavi_catch")))));
			UVeyraCooldownComponent& Cooldowns = *Tavi->GetPlayerState()->FindComponentByClass<UVeyraCooldownComponent>();
			Cooldowns.StartCooldown(Id(TEXT("tavi_catch")), LongCooldown);
			ASSERT_THAT(IsTrue(AttackNow(Plain) == EVeyraAttackRejection::None));
			MarkIt(Marked);
			const double Before = Cooldowns.GetRemainingSecondsNow(Id(TEXT("tavi_catch")));
			ASSERT_THAT(IsTrue(AttackNow(Marked) == EVeyraAttackRejection::None));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::HealthLost(Marked) > FArchetypeTestWorld::HealthLost(Plain),
				FString::Printf(TEXT("It takes more: %g against %g"), FArchetypeTestWorld::HealthLost(Marked), FArchetypeTestWorld::HealthLost(Plain))));
			ASSERT_THAT(IsFalse(IsIt(Marked), TEXT("spent")));
			const double After = Cooldowns.GetRemainingSecondsNow(Id(TEXT("tavi_catch")));
			ASSERT_THAT(IsTrue(After < Before * (1.0 - Tuning().CooldownRefund) + 0.5, FString::Printf(TEXT("Q back sooner: %g of %g"), After, Before)));
		}

		TEST_METHOD(ItJumpsToTheNearestEnemyWhenItFallsToHer)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Falling = World.Spawn(EVeyraTeam::B, FVector(Near, 0.0, 0.0));
			AVeyraVanguardCharacter& Nearby = World.Spawn(EVeyraTeam::B, FVector(Near, Tuning().JumpRadius / 2.0, 0.0));
			AVeyraVanguardCharacter& Distant = World.Spawn(EVeyraTeam::B, FVector(Near + Tuning().JumpRadius * 2.0, 0.0, 0.0));
			MarkIt(Falling);
			FVeyraRawDamageEvent Lethal;
			Lethal.Components.Add({ EVeyraDamageType::TrueDamage, 100000.0 });
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*Tavi->GetAbilitySystemComponent(), *Falling.GetAbilitySystemComponent(), Lethal)));
			ASSERT_THAT(IsTrue(IsIt(Nearby) && !IsIt(Distant) && Passive->GetQuarry() == &Nearby, TEXT("the game goes on")));
		}

		TEST_METHOD(HideLeavesHerIllusionAndReadyOrNotStrikesAPlaymate)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(Near * 4.0, 0.0, 0.0));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Learn(*Tavi, EVeyraAbilitySlot::W, Id(TEXT("tavi_hide")))));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Tavi, EVeyraAbilitySlot::W, Tavi->GetActorLocation()) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Has(*Tavi, TEXT("tavi_hidden"))));
			bool bIllusion = false;
			for (TActorIterator<AVeyraPlacedMarker> It(&Spawner.GetWorld()); It; ++It)
			{
				bIllusion |= It->GetPresentedAs() == Tavi->GetPlayerState();
			}
			ASSERT_THAT(IsTrue(bIllusion, TEXT("an illusion that looks like her")));

			// Ready or Not! in Q's slot: her ultimate's opens only at its level.
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Learn(*Tavi, EVeyraAbilitySlot::Q, Id(TEXT("tavi_ready_or_not")))));
			FVeyraCastTarget On;
			On.Actor = &Enemy;
			On.bHasLocation = true;
			On.Location = Enemy.GetActorLocation();
			ASSERT_THAT(IsTrue(VeyraAbilities::TryCast(*Tavi->GetAbilitySystemComponent(), EVeyraAbilitySlot::Q, On) == EVeyraCastRejection::InvalidTarget,
				TEXT("not a playmate yet")));
			FVeyraRawDamageEvent Hurt;
			Hurt.Components.Add({ EVeyraDamageType::TrueDamage, 50.0 });
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*Tavi->GetAbilitySystemComponent(), *Enemy.GetAbilitySystemComponent(), Hurt)));
			const double Wounded = FArchetypeTestWorld::HealthLost(Enemy);
			ASSERT_THAT(IsTrue(VeyraAbilities::TryCast(*Tavi->GetAbilitySystemComponent(), EVeyraAbilitySlot::Q, On) == EVeyraCastRejection::None));
			Wait(1.0);
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::HealthLost(Enemy) > Wounded && IsIt(Enemy), TEXT("struck, and It")));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
