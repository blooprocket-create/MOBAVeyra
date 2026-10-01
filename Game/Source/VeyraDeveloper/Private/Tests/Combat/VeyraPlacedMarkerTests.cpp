// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "Entities/VeyraPlacedMarker.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Life/VeyraLifeComponent.h"
#include "Targeting/VeyraTargeting.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "TimerManager.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraCombatTests
{
	using VeyraAbilitiesTests::FArchetypeTestWorld;

	// Veyra.Combat.PlacedMarker.*: ADR-003's placed marker, as Tavi's illusion uses it (ADR-030 §5; Combat Bible §32).
	TEST_CLASS(PlacedMarker, "Veyra.Combat")
	{
		// Fixture values.
		static constexpr double Near = 200.0;
		static constexpr double LifetimeSeconds = 3.0;
		static constexpr double Blow = 50.0;
		static constexpr float Step = 0.1f;

		FActorTestSpawner Spawner;
		AVeyraVanguardCharacter* Owner = nullptr;
		AVeyraVanguardCharacter* Enemy = nullptr;
		TArray<FVeyraMarkerEnd> Ends;

		BEFORE_EACH()
		{
			FArchetypeTestWorld World{ Spawner };
			Owner = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			Enemy = &World.Spawn(EVeyraTeam::B, FVector(Near * 2.0, 0.0, 0.0));
			UVeyraCombatEventSubsystem* Events = Spawner.GetWorld().GetSubsystem<UVeyraCombatEventSubsystem>();
			ASSERT_THAT(IsNotNull(Events));
			Events->OnMarkerEnded.AddLambda([this](const FVeyraMarkerEnd& End) { Ends.Add(End); });
		}

		static FVeyraMarkerSpec Illusion(int32 Hits = 1)
		{
			FVeyraMarkerSpec Spec;
			Spec.Id = FVeyraContentId::FromText(TEXT("test_illusion")).GetValue();
			Spec.LifetimeSeconds = LifetimeSeconds;
			Spec.HitsToDestroy = Hits;
			Spec.bPresentsAsOwner = true;
			return Spec;
		}

		AVeyraPlacedMarker* Place(const FVeyraMarkerSpec& Spec)
		{
			return AVeyraPlacedMarker::Place(Spawner.GetWorld(), *Owner->GetAbilitySystemComponent(), Spec, FTransform(FVector(Near, 0.0, 0.0)));
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

		TEST_METHOD(ItStandsForItsOwnerOnItsOwnersSide)
		{
			AVeyraPlacedMarker* Marker = Place(Illusion());
			ASSERT_THAT(IsNotNull(Marker));
			ASSERT_THAT(IsTrue(Marker->GetVeyraTeam() == EVeyraTeam::A && VeyraUnits::IsMarker(Marker)));
			ASSERT_THAT(IsTrue(Marker->GetPresentedAs() == Owner->GetPlayerState(), TEXT("a decoy presents as its owner")));
			ASSERT_THAT(IsTrue(Marker->GetOwnerAbilities() == Owner->GetAbilitySystemComponent()));
			float OwnerRadius = 0.0f;
			float OwnerHalfHeight = 0.0f;
			Owner->GetSimpleCollisionCylinder(OwnerRadius, OwnerHalfHeight);
			float Radius = 0.0f;
			float HalfHeight = 0.0f;
			Marker->GetSimpleCollisionCylinder(Radius, HalfHeight);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Radius, OwnerRadius) && FMath::IsNearlyEqual(HalfHeight, OwnerHalfHeight), TEXT("its owner's size")));
			ASSERT_THAT(IsTrue(VeyraTargeting::CheckEnemyTarget(*Enemy, Marker, Near * 4.0) == EVeyraTargetValidity::Valid, TEXT("its enemies may target it")));
		}

		TEST_METHOD(OneHitOfAnythingDestroysItAndNamesTheDestroyer)
		{
			AVeyraPlacedMarker* Marker = Place(Illusion());
			ASSERT_THAT(IsNotNull(Marker));
			FVeyraStatusSpec Slow;
			Slow.Id = FVeyraContentId::FromText(TEXT("test_slow")).GetValue();
			Slow.Kind = EVeyraStatusKind::Slow;
			Slow.Magnitude = 0.5;
			Slow.DurationSeconds = LifetimeSeconds;
			ASSERT_THAT(IsFalse(VeyraCombat::ApplyStatus(*Enemy->GetAbilitySystemComponent(), *Marker->GetAbilitySystemComponent(), Slow), TEXT("no status affects it")));
			FVeyraRawDamageEvent Hit;
			Hit.Components.Add({ EVeyraDamageType::Magic, Blow });
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*Enemy->GetAbilitySystemComponent(), *Marker->GetAbilitySystemComponent(), Hit)));
			ASSERT_THAT(IsTrue(Ends.Num() == 1 && Ends[0].Reason == EVeyraMarkerEndReason::Destroyed, TEXT("a hit from an ability, not only a basic attack")));
			ASSERT_THAT(IsTrue(Ends[0].Destroyer.Get() == Enemy->GetAbilitySystemComponent() && Ends[0].Owner.Get() == Owner->GetAbilitySystemComponent()));
			ASSERT_THAT(IsTrue(Ends[0].Id == Illusion().Id && FVector::Dist2D(Ends[0].Location, FVector(Near, 0.0, 0.0)) < 1.0));
			ASSERT_THAT(IsTrue(!IsValid(Marker) || Marker->IsActorBeingDestroyed()));
		}

		TEST_METHOD(ItEndsWithItsTimeAndWithItsOwner)
		{
			ASSERT_THAT(IsNotNull(Place(Illusion())));
			Wait(LifetimeSeconds + Step * 2.0);
			ASSERT_THAT(IsTrue(Ends.Num() == 1 && Ends[0].Reason == EVeyraMarkerEndReason::Expired));

			ASSERT_THAT(IsNotNull(Place(Illusion())));
			FVeyraRawDamageEvent Lethal;
			Lethal.Components.Add({ EVeyraDamageType::TrueDamage, 100000.0 });
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*Enemy->GetAbilitySystemComponent(), *Owner->GetAbilitySystemComponent(), Lethal)));
			ASSERT_THAT(IsTrue(Ends.Num() == 2 && Ends[1].Reason == EVeyraMarkerEndReason::OwnerDied));
		}

		TEST_METHOD(OneThatTakesNoHitsCannotBeTargeted)
		{
			AVeyraPlacedMarker* Marker = Place(Illusion(0));
			ASSERT_THAT(IsNotNull(Marker));
			ASSERT_THAT(IsTrue(VeyraTargeting::IsUntargetable(*Marker) && !Marker->IsTargetable()));
			ASSERT_THAT(IsTrue(VeyraTargeting::CheckEnemyTarget(*Enemy, Marker, Near * 4.0) != EVeyraTargetValidity::Valid));
			ASSERT_THAT(IsFalse(VeyraTargeting::CanHitEnemy(Enemy, *Marker)));
		}

		TEST_METHOD(OnlyTheServerPlacesOneWithALifetime)
		{
			TestRunner->AddExpectedMessagePlain(TEXT("Refused a marker"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, 1);
			FVeyraMarkerSpec Timeless = Illusion();
			Timeless.LifetimeSeconds = 0.0;
			ASSERT_THAT(IsNull(Place(Timeless)));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
