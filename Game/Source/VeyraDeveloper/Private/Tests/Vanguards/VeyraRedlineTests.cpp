// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Attacks/VeyraBasicAttackComponent.h"
#include "CQTest.h"
#include "Engine/World.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Passives/VeyraMomentumPassive.h"
#include "Progression/VeyraProgressionComponent.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "Tuning/VeyraVanguardsTuningSubsystem.h"
#include "VeyraVanguards.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraVanguardsTests
{
	using VeyraAbilitiesTests::FArchetypeTestWorld;

	// Veyra.Vanguards.Redline.*: Raska's passive (Roster Bible §1), from the committed tuning. Momentum
	// by distance runs over real frames, in Veyra.Net.Vanguards.Raska.
	TEST_CLASS(Redline, "Veyra.Vanguards")
	{
		// Fixture values: where the enemy stands, and a small step of world time.
		static constexpr double Near = 120.0;
		static constexpr float WorldStep = 0.1f;

		FActorTestSpawner Spawner;
		AVeyraVanguardCharacter* Raska = nullptr;
		UVeyraMomentumPassive* Passive = nullptr;
		UVeyraAbilityLoadoutComponent* Loadout = nullptr;

		BEFORE_EACH()
		{
			FArchetypeTestWorld World{ Spawner };
			Raska = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			AVeyraPlayerState* Participant = Raska->GetPlayerState<AVeyraPlayerState>();
			const FVeyraPreparedVanguard Prepared = VeyraVanguards::PrepareCombatant(*Participant->GetAbilitySystemComponent(), FVeyraContentId::FromText(TEXT("raska")).GetValue());
			ASSERT_THAT(IsTrue(Prepared.bPrepared));
			Participant->SetPassive(Prepared.Passive);
			Passive = Cast<UVeyraMomentumPassive>(Prepared.Passive);
			ASSERT_THAT(IsNotNull(Passive));
			Loadout = Participant->FindComponentByClass<UVeyraAbilityLoadoutComponent>();
		}

		static const FVeyraMomentumTuning& Tuning()
		{
			return *UVeyraVanguardsTuningSubsystem::FindMomentum(FVeyraContentId::FromText(TEXT("raska_redline")).GetValue());
		}

		void CastEvent(const TCHAR* Ability)
		{
			Spawner.GetWorld().GetSubsystem<UVeyraCombatEventSubsystem>()->OnCastCommitted.Broadcast(
				FVeyraCastEvent{ Raska->GetAbilitySystemComponent(), FVeyraContentId::FromText(Ability).GetValue(), true });
		}

		/** Casts until the meter is full. */
		void FillUp()
		{
			while (Passive->GetMomentum() < 100)
			{
				CastEvent(TEXT("raska_breakneck"));
			}
		}

		bool Holds(EVeyraAbilitySlot Slot, const TCHAR* Ability) const
		{
			const FVeyraLoadoutEntry* Entry = Loadout->FindSlot(Slot);
			return Entry && Entry->Ability == FVeyraContentId::FromText(Ability).GetValue();
		}

		bool StrikeAt(AActor& Target)
		{
			UVeyraBasicAttackComponent* Attacks = Raska->GetPlayerState()->FindComponentByClass<UVeyraBasicAttackComponent>();
			UWorld& World = Spawner.GetWorld();
			while (World.GetTimeSeconds() < Attacks->GetNextAttackAt())
			{
				World.Tick(LEVELTICK_TimeOnly, WorldStep);
			}
			if (Attacks->StartAttack(Target) != EVeyraAttackRejection::None)
			{
				return false;
			}
			Attacks->Commit();
			return true;
		}

		TEST_METHOD(AttacksAndCastsBuildIt)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(Near, 0.0, 0.0));
			ASSERT_THAT(IsTrue(StrikeAt(Enemy)));
			ASSERT_THAT(AreEqual(Tuning().PointsPerAttack, Passive->GetMomentum()));
			CastEvent(TEXT("raska_breakneck"));
			ASSERT_THAT(AreEqual(Tuning().PointsPerAttack + Tuning().PointsPerCast, Passive->GetMomentum()));
		}

		TEST_METHOD(AtFullItsSlotsHoldTheirRedlinedFormsAndTheirCastSpendsIt)
		{
			UVeyraProgressionComponent* Progression = Raska->GetPlayerState()->FindComponentByClass<UVeyraProgressionComponent>();
			ASSERT_THAT(IsTrue(Progression->AllocateRank(EVeyraAbilitySlot::Q) == EVeyraRankRefusal::None));
			FillUp();
			ASSERT_THAT(IsTrue(Passive->IsRedlined() && Holds(EVeyraAbilitySlot::Q, TEXT("raska_breakneck_redlined")) && Holds(EVeyraAbilitySlot::W, TEXT("raska_countersteer_redlined"))));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Raska, EVeyraAbilitySlot::Q, FVector(400.0, 0.0, 0.0)) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(Passive->GetMomentum() == 0 && !Passive->IsRedlined() && Passive->IsRoadhouseReady(), TEXT("spent, and Roadhouse waits")));
			ASSERT_THAT(IsTrue(Holds(EVeyraAbilitySlot::Q, TEXT("raska_breakneck")) && Holds(EVeyraAbilitySlot::W, TEXT("raska_countersteer")), TEXT("both forms leave together")));
		}

		TEST_METHOD(NoBrakesHoldsItFullAndARideHoldsRedlineOff)
		{
			ASSERT_THAT(IsTrue(VeyraCombat::StartRide(*Raska->GetAbilitySystemComponent(), FVeyraRide{ 700.0, 180.0, 0.0 })));
			FVeyraStatusSpec Hold;
			Hold.Id = Tuning().HoldFullStatuses[0];
			Hold.Kind = EVeyraStatusKind::Unstoppable;
			Hold.DurationSeconds = 60.0;
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Raska->GetAbilitySystemComponent(), *Raska->GetAbilitySystemComponent(), Hold)));
			Passive->Sample();
			ASSERT_THAT(IsTrue(Passive->GetMomentum() == 100 && !Passive->IsRedlined(), TEXT("full, but no Redlined forms while mounted")));
			VeyraCombat::EndRide(*Raska->GetAbilitySystemComponent(), EVeyraRideEndReason::Expired);
			Passive->Sample();
			ASSERT_THAT(IsTrue(Passive->IsRedlined(), TEXT("off the ride, the full meter Redlines her")));
		}

		TEST_METHOD(RoadhouseLungesForMoreAgainstAVanguard)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Plain = World.Spawn(EVeyraTeam::B, FVector(Near, 0.0, 0.0));
			AVeyraVanguardCharacter& Marked = World.Spawn(EVeyraTeam::B, FVector(0.0, Near, 0.0));
			ASSERT_THAT(IsTrue(StrikeAt(Plain)));
			CastEvent(TEXT("raska_breakneck_redlined"));
			ASSERT_THAT(IsTrue(Passive->IsRoadhouseReady() && FArchetypeTestWorld::Has(*Raska, *Tuning().Roadhouse.ReachStatus.ToString())));
			ASSERT_THAT(IsTrue(StrikeAt(Marked)));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::HealthLost(Marked) > FArchetypeTestWorld::HealthLost(Plain),
				FString::Printf(TEXT("Roadhouse %g, plain %g"), FArchetypeTestWorld::HealthLost(Marked), FArchetypeTestWorld::HealthLost(Plain))));
			ASSERT_THAT(IsFalse(Passive->IsRoadhouseReady() || FArchetypeTestWorld::Has(*Raska, *Tuning().Roadhouse.ReachStatus.ToString())));
		}

		TEST_METHOD(RoadhouseStrikesWithoutItsLungeWhileGrounded)
		{
			// The lunge moves her by her own ability, which Grounded refuses (ADR-028 §2), as a Root's
			// movement lock does; the strike still lands.
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(Near, 0.0, 0.0));
			CastEvent(TEXT("raska_breakneck_redlined"));
			ASSERT_THAT(IsTrue(Passive->IsRoadhouseReady()));
			FVeyraStatusSpec Grounded;
			Grounded.Id = FVeyraContentId::FromText(TEXT("test_grounded")).GetValue();
			Grounded.Kind = EVeyraStatusKind::Grounded;
			Grounded.DurationSeconds = 60.0;
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Enemy.GetAbilitySystemComponent(), *Raska->GetAbilitySystemComponent(), Grounded)));
			ASSERT_THAT(IsTrue(StrikeAt(Enemy)));
			ASSERT_THAT(IsFalse(Raska->GetVeyraMovement()->IsDashing(), TEXT("no lunge")));
			ASSERT_THAT(IsFalse(Passive->IsRoadhouseReady(), TEXT("yet the strike spent Roadhouse")));
		}

		TEST_METHOD(DeathResetsIt)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(Near * 5.0, 0.0, 0.0));
			FillUp();
			ASSERT_THAT(IsTrue(Passive->IsRedlined()));
			FVeyraRawDamageEvent Lethal;
			Lethal.Components.Add({ EVeyraDamageType::TrueDamage, 100000.0 });
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*Enemy.GetAbilitySystemComponent(), *Raska->GetAbilitySystemComponent(), Lethal)));
			ASSERT_THAT(IsTrue(Passive->GetMomentum() == 0 && !Passive->IsRedlined() && Holds(EVeyraAbilitySlot::Q, TEXT("raska_breakneck"))));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
