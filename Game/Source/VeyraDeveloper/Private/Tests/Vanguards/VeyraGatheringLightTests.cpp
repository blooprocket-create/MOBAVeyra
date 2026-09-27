// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Delivery/VeyraProjectile.h"
#include "EngineUtils.h"
#include "Events/VeyraAbilityEvents.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Passives/VeyraGatheringLightPassive.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "Tuning/VeyraVanguardsTuningSubsystem.h"
#include "VeyraVanguards.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraVanguardsTests
{
	// Veyra.Vanguards.GatheringLight.*: Oriel's passive, fed ability hits as her casts announce them
	// (Character Bible §20; ADR-008 §5, §9). Expectations come from the committed tuning.
	// Veyra.Net.Vanguards.Oriel primes it with real casts in a match.
	TEST_CLASS(GatheringLight, "Veyra.Vanguards")
	{
		// Fixture values: where the enemies stand, and how long the fragment gets to land.
		static constexpr double Apart = 400.0;
		static constexpr double FlightSeconds = 2.0;

		FActorTestSpawner Spawner;
		AVeyraVanguardCharacter* Body = nullptr;
		UVeyraGatheringLightPassive* Passive = nullptr;
		int32 NextCastId = 1;

		BEFORE_EACH()
		{
			VeyraAbilitiesTests::FArchetypeTestWorld World{ Spawner };
			Body = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			AVeyraPlayerState* Participant = Body->GetPlayerState<AVeyraPlayerState>();
			const FVeyraPreparedVanguard Prepared = VeyraVanguards::PrepareCombatant(*Participant->GetAbilitySystemComponent(), FVeyraContentId::FromText(TEXT("oriel")).GetValue());
			Passive = Cast<UVeyraGatheringLightPassive>(Prepared.Passive);
			ASSERT_THAT(IsTrue(Prepared.bPrepared && Passive));
			Participant->SetPassive(Prepared.Passive);
		}

		static int32 StacksToPrime()
		{
			return UVeyraVanguardsTuningSubsystem::FindGatheringLight(FVeyraContentId::FromText(TEXT("oriel_gathering_light")).GetValue())->StacksToPrime;
		}

		/** Announces a hit of one of Oriel's casts on Target, as its delivery would. */
		void Hit(AActor& Target, int32 CastId, bool bDamaging = true)
		{
			FVeyraAbilityHit AbilityHit;
			AbilityHit.Caster = Body->GetAbilitySystemComponent();
			AbilityHit.Target = &Target;
			AbilityHit.Ability = FVeyraContentId::FromText(TEXT("oriel_splinter_lance")).GetValue();
			AbilityHit.CastId = CastId;
			AbilityHit.bDamaging = bDamaging;
			UVeyraAbilityEventSubsystem::Announce(&Spawner.GetWorld(), AbilityHit);
		}

		/** Casts that each hit Target once, until the passive is primed. */
		void Prime(AActor& Target)
		{
			while (!Passive->IsPrimed())
			{
				Hit(Target, NextCastId++);
			}
		}

		/** The fragment in flight, if any. */
		AVeyraProjectile* Fragment()
		{
			for (TActorIterator<AVeyraProjectile> It(&Spawner.GetWorld()); It; ++It)
			{
				if (It->GetAbility() == FVeyraContentId::FromText(TEXT("oriel_gathering_light")).GetValue())
				{
					return *It;
				}
			}
			return nullptr;
		}

		TEST_METHOD(EachDamagingCastOnAnEnemyVanguardAddsOneStack)
		{
			VeyraAbilitiesTests::FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& First = World.Spawn(EVeyraTeam::B, FVector(Apart, 0.0, 0.0));
			AVeyraVanguardCharacter& Second = World.Spawn(EVeyraTeam::B, FVector(0.0, Apart, 0.0));
			AVeyraVanguardCharacter& Friend = World.Spawn(EVeyraTeam::A, FVector(-Apart, 0.0, 0.0));
			AVeyraTestFluxborn& Minion = World.SpawnFluxborn(EVeyraTeam::B, FVector(0.0, -Apart, 0.0));

			Hit(First, 1);
			Hit(Second, 1);
			ASSERT_THAT(IsTrue(Passive->GetStacks() == 1, TEXT("one cast, however many it hits")));
			Hit(First, 2, /*bDamaging*/ false);
			Hit(Minion, 3);
			Hit(Friend, 4);
			ASSERT_THAT(IsTrue(Passive->GetStacks() == 1, TEXT("no damage, no Vanguard, or no enemy")));
			Hit(Second, 5);
			ASSERT_THAT(AreEqual(2, Passive->GetStacks()));
		}

		TEST_METHOD(ThePrimedCastSendsAFragmentAndStartsAgain)
		{
			VeyraAbilitiesTests::FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(Apart, 0.0, 0.0));
			AVeyraVanguardCharacter& Other = World.Spawn(EVeyraTeam::B, FVector(0.0, Apart, 0.0));
			Prime(Enemy);
			ASSERT_THAT(AreEqual(StacksToPrime(), Passive->GetStacks()));
			ASSERT_THAT(IsTrue(Fragment() == nullptr, TEXT("priming sends nothing yet")));

			const int32 Consuming = NextCastId++;
			Hit(Enemy, Consuming);
			AVeyraProjectile* Sent = Fragment();
			ASSERT_THAT(IsTrue(Sent && Sent->GetHomingTarget() == &Enemy));
			ASSERT_THAT(AreEqual(0, Passive->GetStacks()));
			Hit(Other, Consuming);
			ASSERT_THAT(IsTrue(Passive->GetStacks() == 0, TEXT("the consuming cast rebuilds nothing")));

			Sent->AdvanceBy(FlightSeconds);
			ASSERT_THAT(IsTrue(VeyraAbilitiesTests::FArchetypeTestWorld::HealthLost(Enemy) > 0.0, TEXT("the fragment lands")));
			ASSERT_THAT(IsTrue(Passive->GetStacks() == 0, TEXT("its own hit is not a cast")));
		}

		TEST_METHOD(APrimedCastThatHitsNoEnemyVanguardStaysPrimed)
		{
			VeyraAbilitiesTests::FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(Apart, 0.0, 0.0));
			AVeyraTestFluxborn& Minion = World.SpawnFluxborn(EVeyraTeam::B, FVector(0.0, Apart, 0.0));
			Prime(Enemy);
			const int32 Cast = NextCastId++;
			Hit(Minion, Cast);
			ASSERT_THAT(IsTrue(Passive->IsPrimed() && !Fragment()));
			Hit(Enemy, Cast);
			ASSERT_THAT(IsTrue(Fragment() && Passive->GetStacks() == 0, TEXT("a Vanguard struck by the same cast takes the fragment")));
		}

		TEST_METHOD(DeathClearsTheStacks)
		{
			VeyraAbilitiesTests::FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(Apart, 0.0, 0.0));
			Hit(Enemy, NextCastId++);
			ASSERT_THAT(AreEqual(1, Passive->GetStacks()));
			FVeyraDeathEvent Death;
			Death.Victim = Body->GetAbilitySystemComponent();
			Spawner.GetWorld().GetSubsystem<UVeyraCombatEventSubsystem>()->OnDeath.Broadcast(Death);
			ASSERT_THAT(AreEqual(0, Passive->GetStacks()));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
