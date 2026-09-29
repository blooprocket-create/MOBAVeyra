// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Engine/World.h"
#include "Passives/VeyraHauntPassive.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "TimerManager.h"
#include "Tuning/VeyraVanguardsTuningSubsystem.h"
#include "VeyraVanguards.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraVanguardsTests
{
	using VeyraAbilitiesTests::FArchetypeTestWorld;

	// Veyra.Vanguards.HauntedAttachment.*: Patch's passive (Roster Bible §5), from the committed tuning.
	TEST_CLASS(HauntedAttachment, "Veyra.Vanguards")
	{
		// Fixture values: where the others stand, and a small hit.
		static constexpr double Near = 300.0;
		static constexpr double Hit = 10.0;
		static constexpr float WorldStep = 0.1f;

		FActorTestSpawner Spawner;
		AVeyraVanguardCharacter* Patch = nullptr;
		AVeyraVanguardCharacter* Enemy = nullptr;
		AVeyraVanguardCharacter* Ally = nullptr;

		BEFORE_EACH()
		{
			FArchetypeTestWorld World{ Spawner };
			Patch = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			AVeyraPlayerState* Participant = Patch->GetPlayerState<AVeyraPlayerState>();
			const FVeyraPreparedVanguard Prepared = VeyraVanguards::PrepareCombatant(*Participant->GetAbilitySystemComponent(), FVeyraContentId::FromText(TEXT("patch")).GetValue());
			ASSERT_THAT(IsTrue(Prepared.bPrepared && Cast<UVeyraHauntPassive>(Prepared.Passive) != nullptr));
			Participant->SetPassive(Prepared.Passive);
			Enemy = &World.Spawn(EVeyraTeam::B, FVector(Near, 0.0, 0.0));
			Ally = &World.Spawn(EVeyraTeam::A, FVector(0.0, Near, 0.0));
		}

		static const FVeyraHauntTuning& Tuning()
		{
			return *UVeyraVanguardsTuningSubsystem::FindHaunt(FVeyraContentId::FromText(TEXT("patch_haunted_attachment")).GetValue());
		}

		static void Strike(AActor& From, AActor& Whom)
		{
			FVeyraRawDamageEvent Damage;
			Damage.Components.Add({ EVeyraDamageType::Physical, Hit });
			VeyraCombat::DealDamage(*UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&From), *UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Whom), Damage);
		}

		bool IsHaunted(const AActor& Unit) const
		{
			const UAbilitySystemComponent* AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Unit);
			const UVeyraStatusComponent* Marks = AbilitySystem ? AbilitySystem->GetOwner()->FindComponentByClass<UVeyraStatusComponent>() : nullptr;
			return Marks && Marks->HasFrom(Tuning().HauntStatus, *Patch->GetAbilitySystemComponent());
		}

		void Wait(double Seconds)
		{
			UWorld& World = Spawner.GetWorld();
			const double Until = World.GetTimeSeconds() + Seconds;
			while (World.GetTimeSeconds() < Until)
			{
				World.Tick(LEVELTICK_TimeOnly, WorldStep);
			}
		}

		TEST_METHOD(WhoHitsPatchIsHauntedAndLashedForHittingAnAllyNearHim)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& FarAlly = World.Spawn(EVeyraTeam::A, FVector(Tuning().AllyRadius * 2.0, 0.0, 0.0));
			Strike(*Enemy, *Patch);
			ASSERT_THAT(IsTrue(IsHaunted(*Enemy)));
			Strike(*Enemy, FarAlly);
			ASSERT_THAT(IsTrue(IsHaunted(*Enemy) && FArchetypeTestWorld::HealthLost(*Enemy) == 0.0, TEXT("an ally far from Patch wakes nothing")));
			Strike(*Enemy, *Ally);
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::HealthLost(*Enemy) > 0.0, TEXT("the spirit lashes out")));
			ASSERT_THAT(IsTrue(!IsHaunted(*Enemy) && FArchetypeTestWorld::Has(*Enemy, *Tuning().Statuses[0].ToString()), TEXT("slowed, and the Haunt is spent")));
		}

		TEST_METHOD(TheSameEnemyIsHauntedOnlyOncePerItsCooldown)
		{
			Strike(*Enemy, *Patch);
			Strike(*Enemy, *Ally);
			ASSERT_THAT(IsFalse(IsHaunted(*Enemy)));
			Strike(*Enemy, *Patch);
			ASSERT_THAT(IsFalse(IsHaunted(*Enemy), TEXT("still on its cooldown")));
			Wait(Tuning().PerEnemyCooldownSeconds + WorldStep);
			Strike(*Enemy, *Patch);
			ASSERT_THAT(IsTrue(IsHaunted(*Enemy)));
		}

		TEST_METHOD(OnlyAnEnemyVanguardWakesTheSpirit)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraTestFluxborn& Minion = World.SpawnFluxborn(EVeyraTeam::B, FVector(-Near, 0.0, 0.0));
			Strike(Minion, *Patch);
			ASSERT_THAT(IsFalse(IsHaunted(Minion)));
			Strike(*Ally, *Patch);
			ASSERT_THAT(IsFalse(IsHaunted(*Ally)));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
