// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Attributes/VeyraResourceSet.h"
#include "CQTest.h"
#include "Movement/VeyraMovementFields.h"
#include "Passives/VeyraChargerPassive.h"
#include "Progression/VeyraProgressionComponent.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "TimerManager.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "Tuning/VeyraVanguardsTuningSubsystem.h"
#include "VeyraVanguards.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraVanguardsTests
{
	using VeyraAbilitiesTests::FArchetypeTestWorld;

	// Veyra.Vanguards.Conductor.*: Relay's kit (Roster Bible §4; ADR-033), from the committed tuning.
	TEST_CLASS(Conductor, "Veyra.Vanguards")
	{
		// Fixture values: XP for many levels, a lethal blow, where units stand, and a step of time.
		static constexpr double ManyLevels = 50000.0;
		static constexpr double Lethal = 100000.0;
		static constexpr double Near = 200.0;
		static constexpr double Plenty = 60.0;
		static constexpr float Step = 0.1f;
		static constexpr double Tolerance = 1e-3;

		FActorTestSpawner Spawner;
		AVeyraVanguardCharacter* Relay = nullptr;

		BEFORE_EACH()
		{
			FArchetypeTestWorld World{ Spawner };
			Relay = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			AVeyraPlayerState* Participant = Relay->GetPlayerState<AVeyraPlayerState>();
			const FVeyraPreparedVanguard Prepared = VeyraVanguards::PrepareCombatant(*Participant->GetAbilitySystemComponent(), Id(TEXT("relay")));
			ASSERT_THAT(IsTrue(Prepared.bPrepared));
			Participant->SetPassive(Prepared.Passive);
			ASSERT_THAT(IsNotNull(Cast<UVeyraChargerPassive>(Prepared.Passive)));
			UVeyraProgressionComponent* Progression = Participant->FindComponentByClass<UVeyraProgressionComponent>();
			Progression->AddExperience(ManyLevels);
			for (const EVeyraAbilitySlot Slot : { EVeyraAbilitySlot::Q, EVeyraAbilitySlot::W, EVeyraAbilitySlot::E, EVeyraAbilitySlot::R })
			{
				ASSERT_THAT(IsTrue(Progression->AllocateRank(Slot) == EVeyraRankRefusal::None));
			}
		}

		static FVeyraContentId Id(const TCHAR* Text)
		{
			return FVeyraContentId::FromText(Text).GetValue();
		}

		double Charge() const
		{
			return Relay->GetAbilitySystemComponent()->GetNumericAttribute(UVeyraResourceSet::GetResourceAttribute());
		}

		bool Holds(const AActor& Unit, const TCHAR* Status) const
		{
			return VeyraCombat::HasStatusFrom(&Unit, Id(Status), *Relay->GetAbilitySystemComponent());
		}

		EVeyraCastRejection CastOn(EVeyraAbilitySlot Slot, AActor& Unit) const
		{
			FVeyraCastTarget Target;
			Target.Actor = &Unit;
			Target.bHasLocation = true;
			Target.Location = Unit.GetActorLocation();
			return VeyraAbilities::TryCast(*Relay->GetAbilitySystemComponent(), Slot, Target);
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

		TEST_METHOD(HeStartsWithoutChargeAndNearbyFluxbornDeathsFillIt)
		{
			ASSERT_THAT(IsTrue(FMath::IsNearlyZero(Charge()), TEXT("an empty battery")));
			ASSERT_THAT(IsTrue(Relay->GetAbilitySystemComponent()->GetNumericAttribute(UVeyraResourceSet::GetMaxResourceAttribute()) > 100.0,
				TEXT("whose most grows with his level")));
			FArchetypeTestWorld World{ Spawner };
			AVeyraTestFluxborn& Wave = World.SpawnFluxborn(EVeyraTeam::B, FVector(Near, 0.0, 0.0));
			FVeyraRawDamageEvent Blow;
			Blow.Components.Add({ EVeyraDamageType::TrueDamage, Lethal });
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*Relay->GetAbilitySystemComponent(), *Wave.GetAbilitySystemComponent(), Blow)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Charge(), UVeyraVanguardsTuningSubsystem::FindCharger(Id(TEXT("relay_charger")))->ChargePerDeath, Tolerance)));
		}

		TEST_METHOD(RapidDischargeSpendsHalfHisChargeToOverclockAnAlly)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Ally = World.Spawn(EVeyraTeam::A, FVector(Near, 0.0, 0.0));
			UAbilitySystemComponent& Abilities = *Relay->GetAbilitySystemComponent();
			ASSERT_THAT(IsTrue(CastOn(EVeyraAbilitySlot::Q, Ally) == EVeyraCastRejection::InsufficientResource, TEXT("not without Charge")));
			ASSERT_THAT(IsTrue(VeyraCombat::RestoreResource(Abilities, Plenty)));
			ASSERT_THAT(IsTrue(CastOn(EVeyraAbilitySlot::Q, Ally) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(Holds(Ally, TEXT("relay_overclocked")), TEXT("the ally is overclocked")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Charge(), Plenty / 2.0, Tolerance), TEXT("for half his Charge")));
		}

		TEST_METHOD(FullGridDrainsHisChargeAndEndsWithIt)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Ally = World.Spawn(EVeyraTeam::A, FVector(Near, 0.0, 0.0));
			AVeyraTestFluxborn& Minion = World.SpawnFluxborn(EVeyraTeam::A, FVector(0.0, Near, 0.0));
			ASSERT_THAT(IsTrue(VeyraCombat::RestoreResource(*Relay->GetAbilitySystemComponent(), Plenty)));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Relay, EVeyraAbilitySlot::R, Relay->GetActorLocation()) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(Holds(*Relay, TEXT("relay_grid_planted")) && Holds(*Relay, TEXT("relay_grid_discount")), TEXT("he anchors")));
			ASSERT_THAT(IsTrue(Holds(Ally, TEXT("relay_grid_link")), TEXT("an ally is connected")));
			ASSERT_THAT(IsTrue(Holds(Minion, TEXT("relay_fluxborn_overclock")), TEXT("an allied Fluxborn is overclocked")));
			const FVeyraDrainTuning& Drain = UVeyraAbilitiesTuningSubsystem::FindSelfBuff(Id(TEXT("relay_full_grid")))->Drain[0];
			Wait(Plenty / Drain.PerSecond + Drain.IntervalSeconds);
			ASSERT_THAT(IsTrue(FMath::IsNearlyZero(Charge()), TEXT("his Charge drains away")));
			ASSERT_THAT(IsFalse(Holds(*Relay, TEXT("relay_grid_planted")), TEXT("and the grid goes with it")));
		}

		TEST_METHOD(MagneticFieldBendsAnEnemyDashTowardItsCentre)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(0.0, Near, 0.0));
			const FVector Centre = Enemy.GetActorLocation() + FVector(Near * 1.5, Near, 0.0);
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Relay, EVeyraAbilitySlot::E, Centre) == EVeyraCastRejection::None));
			// A timer set this frame counts from the next tick, and fires once its time is strictly past.
			Wait(UVeyraAbilitiesTuningSubsystem::FindArea(Id(TEXT("relay_magnetic_field")))->Cast.WindupSeconds + 3.0 * Step);
			ASSERT_THAT(AreEqual(1, Spawner.GetWorld().GetSubsystem<UVeyraMovementFieldSubsystem>()->GetFieldCount()));
			FVeyraDash Dash;
			Dash.Direction = FVector::ForwardVector;
			Dash.Distance = Near * 3.0;
			Dash.Speed = Near * 6.0;
			ASSERT_THAT(IsTrue(VeyraCombat::Dash(*Enemy.GetAbilitySystemComponent(), Dash)));
			const TOptional<FVector> Lands = Enemy.GetVeyraMovement()->GetForcedMoveDestination();
			ASSERT_THAT(IsTrue(Lands.IsSet() && Lands->Y > Enemy.GetActorLocation().Y + 1.0, TEXT("dragged toward the field's centre")));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
