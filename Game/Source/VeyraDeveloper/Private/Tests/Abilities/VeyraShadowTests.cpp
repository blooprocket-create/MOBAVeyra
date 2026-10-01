// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Cooldowns/VeyraCooldownComponent.h"
#include "Delivery/VeyraProjectile.h"
#include "EngineUtils.h"
#include "Entities/VeyraPlacedMarker.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "TimerManager.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraAbilitiesTests
{
	/** Fixture values for the shadow tests: a placed marker, blinks to it and beside enemies, a shot it throws too. */
	namespace ShadowFixture
	{
		constexpr double PlaceRange = 600.0;
		constexpr double Lifetime = 3.0;
		constexpr double BlinkRange = 700.0;
		constexpr double Beside = 50.0;
		constexpr double Slash = 40.0;
		constexpr double Needle = 100.0;
		constexpr double Repeat = 30.0;
		constexpr double ShotSpeed = 2000.0;
		constexpr double ShotRadius = 50.0;
		constexpr double ShotRange = 1200.0;
		constexpr double Cooldown = 10.0;
		constexpr float Step = 0.05f;

		inline FVeyraEffectBundleTuning TrueDamage(double Amount)
		{
			FVeyraEffectBundleTuning Effects;
			FVeyraDamageTuning& Damage = Effects.Damage.AddDefaulted_GetRef();
			Damage.Type = EVeyraDamageType::TrueDamage;
			Damage.AmountByRank = { Amount };
			return Effects;
		}

		inline void Wait(FActorTestSpawner& Spawner, double Seconds)
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
	}

	// Veyra.Abilities.Shadow.*: a marker placed at a point, whose follow-up ends with it; blinks to it,
	// swapping places, or beside an enemy; and a shot it throws too, sharing what it strikes (ADR-031
	// §4–§6).
	TEST_CLASS(Shadow, "Veyra.Abilities")
	{
		FActorTestSpawner Spawner;
		FVeyraAbilitiesTuning Tuning;
		AVeyraVanguardCharacter* Caster = nullptr;
		AVeyraVanguardCharacter* Enemy = nullptr;
		UVeyraAbilityLoadoutComponent* Loadout = nullptr;

		BEFORE_EACH()
		{
			using namespace ShadowFixture;
			FVeyraPlacementAbilityTuning Place;
			Place.Cast = InstantCast(PlaceRange, Cooldown, 0.0);
			Place.Marker.LifetimeSeconds = Lifetime;
			FVeyraRecastTuning& Window = Place.Cast.RecastWindow.AddDefaulted_GetRef();
			Window.Ability = ArchetypeTestId(TEXT("test_swap"));
			Window.WindowSeconds = Lifetime * 2.0;
			Tuning.Placement.Add(ArchetypeTestId(TEXT("test_shadow")), Place);

			FVeyraBlinkAbilityTuning Swap;
			Swap.Cast = InstantCast(0.0, 0.0, 0.0);
			Swap.To = EVeyraBlinkTo::OwnMarker;
			Swap.MarkerAbility = { ArchetypeTestId(TEXT("test_shadow")) };
			Swap.Swap = EVeyraBlinkSwap::Swap;
			Tuning.Blink.Add(ArchetypeTestId(TEXT("test_swap")), Swap);

			FVeyraBlinkAbilityTuning BlackStep;
			BlackStep.Cast = InstantCast(BlinkRange, Cooldown, 0.0);
			BlackStep.To = EVeyraBlinkTo::EnemyUnitOrOwnMarker;
			BlackStep.MarkerAbility = { ArchetypeTestId(TEXT("test_shadow")) };
			BlackStep.BesideDistance = Beside;
			BlackStep.Effects = TrueDamage(Slash);
			Tuning.Blink.Add(ArchetypeTestId(TEXT("test_step")), BlackStep);

			FVeyraSkillshotAbilityTuning Throw;
			Throw.Cast = InstantCast(0.0, Cooldown, 0.0);
			Throw.Projectile.Speed = ShotSpeed;
			Throw.Projectile.Radius = ShotRadius;
			Throw.Projectile.Range = ShotRange;
			Throw.Collision = EVeyraSkillshotCollision::Pierce;
			Throw.Effects = TrueDamage(Needle);
			FVeyraSkillshotMimicTuning& Mimic = Throw.Mimic.AddDefaulted_GetRef();
			Mimic.MarkerAbility = ArchetypeTestId(TEXT("test_shadow"));
			Mimic.RepeatEffects = TrueDamage(Repeat);
			Tuning.Skillshot.Add(ArchetypeTestId(TEXT("test_needle")), Throw);
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(&Tuning);
			const int32 Ranks[] = { 5, 3 };
			ASSERT_THAT(IsTrue(VeyraAbilityRules::Validate(Tuning, Ranks).IsEmpty(), FString::Join(VeyraAbilityRules::Validate(Tuning, Ranks), TEXT(" | "))));

			FArchetypeTestWorld World{ Spawner };
			Caster = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			Enemy = &World.Spawn(EVeyraTeam::B, FVector(BlinkRange / 2.0, 0.0, 0.0));
			Loadout = Caster->GetPlayerState()->FindComponentByClass<UVeyraAbilityLoadoutComponent>();
		}

		AFTER_EACH()
		{
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(nullptr);
		}

		AVeyraPlacedMarker* StandingShadow() const
		{
			return AVeyraPlacedMarker::FindStanding(*Caster->GetAbilitySystemComponent(), ArchetypeTestId(TEXT("test_shadow")));
		}

		/** Places the shadow at Point, learning it in W first. */
		void PlaceAt(const FVector& Point)
		{
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Learn(*Caster, EVeyraAbilitySlot::W, ArchetypeTestId(TEXT("test_shadow")))));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Caster, EVeyraAbilitySlot::W, Point) == EVeyraCastRejection::None));
			ASSERT_THAT(IsNotNull(StandingShadow()));
		}

		EVeyraCastRejection CastOn(EVeyraAbilitySlot Slot, AActor* Unit) const
		{
			FVeyraCastTarget Target;
			Target.Actor = Unit;
			Target.bHasLocation = Unit != nullptr;
			Target.Location = Unit ? Unit->GetActorLocation() : FVector::ZeroVector;
			return VeyraAbilities::TryCast(*Caster->GetAbilitySystemComponent(), Slot, Target);
		}

		TEST_METHOD(ItPlacesItsMarkerWithinRangeAndItsFollowUpEndsWithIt)
		{
			PlaceAt(FVector(0.0, ShadowFixture::PlaceRange * 2.0, 0.0));
			ASSERT_THAT(IsTrue(FVector::Dist2D(StandingShadow()->GetActorLocation(), Caster->GetActorLocation()) <= ShadowFixture::PlaceRange + 1.0, TEXT("brought within range")));
			ASSERT_THAT(IsTrue(Loadout->FindSlot(EVeyraAbilitySlot::W)->Ability == ArchetypeTestId(TEXT("test_swap")), TEXT("the swap is offered")));
			ShadowFixture::Wait(Spawner, ShadowFixture::Lifetime + 0.5);
			ASSERT_THAT(IsNull(StandingShadow()));
			ASSERT_THAT(IsTrue(Loadout->FindSlot(EVeyraAbilitySlot::W)->Ability == ArchetypeTestId(TEXT("test_shadow")), TEXT("with its marker gone, so is the swap")));
		}

		TEST_METHOD(PlacingAgainWhileTheLastStandsKeepsTheNewSwap)
		{
			PlaceAt(FVector(0.0, ShadowFixture::PlaceRange / 2.0, 0.0));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Caster, EVeyraAbilitySlot::W, Caster->GetActorLocation()) == EVeyraCastRejection::None, TEXT("the swap")));
			ASSERT_THAT(IsNotNull(StandingShadow(), TEXT("the shadow still stands")));
			Caster->GetPlayerState()->FindComponentByClass<UVeyraCooldownComponent>()->ClearCooldown(ArchetypeTestId(TEXT("test_shadow")));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Caster, EVeyraAbilitySlot::W, FVector(ShadowFixture::PlaceRange / 2.0, 0.0, 0.0)) == EVeyraCastRejection::None));
			ASSERT_THAT(IsNotNull(StandingShadow(), TEXT("a new shadow stands")));
			ASSERT_THAT(IsTrue(Loadout->FindSlot(EVeyraAbilitySlot::W)->Ability == ArchetypeTestId(TEXT("test_swap")), TEXT("and offers its swap")));
		}

		TEST_METHOD(TheSwapExchangesPlacesWithTheMarker)
		{
			const FVector Stood = Caster->GetActorLocation();
			const FVector There(0.0, ShadowFixture::PlaceRange / 2.0, 0.0);
			PlaceAt(There);
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Caster, EVeyraAbilitySlot::W, Stood) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(FVector::Dist2D(Caster->GetActorLocation(), There) < 1.0, TEXT("the caster stands where the shadow stood")));
			ASSERT_THAT(IsTrue(FVector::Dist2D(StandingShadow()->GetActorLocation(), Stood) < 1.0, TEXT("and the shadow where the caster stood")));
		}

		TEST_METHOD(ABlinkLandsBesideAnEnemyAndStrikesOrGoesToItsOwnMarker)
		{
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Learn(*Caster, EVeyraAbilitySlot::E, ArchetypeTestId(TEXT("test_step")))));
			ASSERT_THAT(IsTrue(CastOn(EVeyraAbilitySlot::E, Enemy) == EVeyraCastRejection::None));
			const double Gap = FVector::Dist2D(Caster->GetActorLocation(), Enemy->GetActorLocation());
			ASSERT_THAT(IsTrue(Gap < Enemy->GetSimpleCollisionRadius() + Caster->GetSimpleCollisionRadius() + ShadowFixture::Beside + 1.0, TEXT("beside it")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(FArchetypeTestWorld::HealthLost(*Enemy), ShadowFixture::Slash), TEXT("and it strikes")));
		}

		TEST_METHOD(ABlinkMayNameItsOwnMarkerOnlyWithinReach)
		{
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Learn(*Caster, EVeyraAbilitySlot::E, ArchetypeTestId(TEXT("test_step")))));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Learn(*Caster, EVeyraAbilitySlot::W, ArchetypeTestId(TEXT("test_shadow")))));
			const FVector There(0.0, -ShadowFixture::PlaceRange / 2.0, 0.0);
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Caster, EVeyraAbilitySlot::W, There) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Learn(*Caster, EVeyraAbilitySlot::E, ArchetypeTestId(TEXT("test_step")))));
			// Out of its reach first: it is refused.
			Caster->SetActorLocation(FVector(0.0, ShadowFixture::BlinkRange * 2.0, 0.0));
			ASSERT_THAT(IsTrue(CastOn(EVeyraAbilitySlot::E, StandingShadow()) == EVeyraCastRejection::OutOfRange));
			Caster->SetActorLocation(FVector::ZeroVector);
			ASSERT_THAT(IsTrue(CastOn(EVeyraAbilitySlot::E, StandingShadow()) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(FVector::Dist2D(Caster->GetActorLocation(), There) < 1.0, TEXT("to the shadow, which stays")));
			ASSERT_THAT(IsTrue(FVector::Dist2D(StandingShadow()->GetActorLocation(), There) < 1.0));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::HealthLost(*Enemy) == 0.0));
		}

		TEST_METHOD(WithItsMarkerStandingTheShotIsThrownTwiceAndNoUnitTakesTwoFullHits)
		{
			using namespace ShadowFixture;
			// The shadow stands off to the side; both shots fly toward the enemy, and the shadow's flies on
			// to a second enemy only it can reach.
			PlaceAt(FVector(0.0, 400.0, 0.0));
			const FVector Point(600.0, 200.0, 0.0);
			Enemy->SetActorLocation(Point);
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Beyond = World.Spawn(EVeyraTeam::B, FVector(900.0, 100.0, 0.0));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Learn(*Caster, EVeyraAbilitySlot::Q, ArchetypeTestId(TEXT("test_needle")))));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Caster, EVeyraAbilitySlot::Q, Point) == EVeyraCastRejection::None));
			TArray<AVeyraProjectile*> Shots;
			for (TActorIterator<AVeyraProjectile> It(&Spawner.GetWorld()); It; ++It)
			{
				Shots.Add(*It);
			}
			ASSERT_THAT(AreEqual(2, Shots.Num()));
			for (AVeyraProjectile* Shot : Shots)
			{
				Shot->AdvanceBy(ShotRange / ShotSpeed);
			}
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(FArchetypeTestWorld::HealthLost(*Enemy), Needle + Repeat), TEXT("one full hit, then the repeat")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(FArchetypeTestWorld::HealthLost(Beyond), Needle), TEXT("the shadow's alone hits in full")));
		}

		TEST_METHOD(ValidationWantsAMarkerForABlinkToOne)
		{
			FVeyraAbilitiesTuning Broken = Tuning;
			Broken.Blink.FindChecked(ArchetypeTestId(TEXT("test_swap"))).MarkerAbility = { ArchetypeTestId(TEXT("test_needle")) };
			Broken.Skillshot.FindChecked(ArchetypeTestId(TEXT("test_needle"))).Mimic[0].MarkerAbility = ArchetypeTestId(TEXT("test_step"));
			const int32 Ranks[] = { 5, 3 };
			const FString All = FString::Join(VeyraAbilityRules::Validate(Broken, Ranks), TEXT(" | "));
			ASSERT_THAT(IsTrue(All.Contains(TEXT("/blink/test_swap/markerAbility")), All));
			ASSERT_THAT(IsTrue(All.Contains(TEXT("/skillshot/test_needle/mimic/0/markerAbility")), All));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
