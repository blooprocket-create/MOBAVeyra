// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Cooldowns/VeyraCooldownComponent.h"
#include "EngineUtils.h"
#include "Entities/VeyraPlacedMarker.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "TimerManager.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraAbilitiesTests
{
	/** Fixture values shared by the ambush and the illusion. */
	namespace AmbushFixture
	{
		constexpr double Reach = 900.0;
		constexpr double Near = 300.0;
		constexpr double Cooldown = 20.0;
		constexpr double Blow = 50.0;
		constexpr double BurstRadius = 350.0;
		constexpr double Lifetime = 3.0;
		constexpr double Recent = 4.0;
		constexpr double Vanish = 0.6;
		constexpr double Beside = 50.0;
		constexpr double LongSeconds = 60.0;
		constexpr float Step = 0.05f;

		inline FVeyraEffectBundleTuning Hurts()
		{
			FVeyraEffectBundleTuning Effects;
			FVeyraDamageTuning& Damage = Effects.Damage.AddDefaulted_GetRef();
			Damage.Type = EVeyraDamageType::Magic;
			Damage.AmountByRank = { Blow };
			return Effects;
		}

		/** Lets at least Seconds of world time pass, timers and all. */
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

	// Veyra.Abilities.BuffMarker.*: a self-buff that leaves a placed marker, as Tavi's illusion (ADR-030 §5).
	TEST_CLASS(BuffMarker, "Veyra.Abilities")
	{
		FActorTestSpawner Spawner;
		FVeyraAbilitiesTuning Tuning;
		AVeyraVanguardCharacter* Caster = nullptr;
		AVeyraVanguardCharacter* Enemy = nullptr;
		TArray<FVeyraMarkerEnd> Ends;

		BEFORE_EACH()
		{
			using namespace AmbushFixture;
			FVeyraSelfBuffAbilityTuning Hide;
			Hide.Cast = InstantCast(0.0, Cooldown, 0.0);
			Hide.Statuses = { ArchetypeTestId(TEXT("test_hidden")) };
			FVeyraBuffMarkerTuning& Illusion = Hide.Marker.AddDefaulted_GetRef();
			Illusion.LifetimeSeconds = Lifetime;
			Illusion.HitsToDestroy = 1;
			Illusion.Look = EVeyraMarkerLook::AsOwner;
			FVeyraAreaZoneTuning& Burst = Illusion.BurstZones.AddDefaulted_GetRef();
			Burst.Shape = CircleOf(BurstRadius);
			Burst.Effects = Hurts();
			Tuning.SelfBuff.Add(ArchetypeTestId(TEXT("test_hide")), Hide);
			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_hidden")), StatusOf(EVeyraStatusKind::Invisible, 0.0, Lifetime / 2.0));
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(&Tuning);

			FArchetypeTestWorld World{ Spawner };
			Caster = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			Enemy = &World.Spawn(EVeyraTeam::B, FVector(Near, 0.0, 0.0));
			ASSERT_THAT(IsTrue(World.Learn(*Caster, EVeyraAbilitySlot::W, ArchetypeTestId(TEXT("test_hide")))));
			Spawner.GetWorld().GetSubsystem<UVeyraCombatEventSubsystem>()->OnMarkerEnded.AddLambda([this](const FVeyraMarkerEnd& End) { Ends.Add(End); });
		}

		AFTER_EACH()
		{
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(nullptr);
		}

		AVeyraPlacedMarker* Standing()
		{
			for (TActorIterator<AVeyraPlacedMarker> It(&Spawner.GetWorld()); It; ++It)
			{
				if (!It->IsActorBeingDestroyed())
				{
					return *It;
				}
			}
			return nullptr;
		}

		EVeyraCastRejection Hide() const
		{
			return FArchetypeTestWorld::CastAt(*Caster, EVeyraAbilitySlot::W, Caster->GetActorLocation());
		}

		TEST_METHOD(ItHidesItsCasterAndLeavesADecoyWhereItStood)
		{
			const FVector Stood = Caster->GetActorLocation();
			ASSERT_THAT(IsTrue(Hide() == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Has(*Caster, TEXT("test_hidden")), TEXT("its caster is Invisible")));
			const AVeyraPlacedMarker* Illusion = Standing();
			ASSERT_THAT(IsNotNull(Illusion));
			ASSERT_THAT(IsTrue(FVector::Dist2D(Illusion->GetActorLocation(), Stood) < 1.0 && Illusion->GetPresentedAs() == Caster->GetPlayerState()));
		}

		TEST_METHOD(ARecastRecallsItAndItBurstsAroundItself)
		{
			ASSERT_THAT(IsTrue(Hide() == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::HealthLost(*Enemy) == 0.0));
			ASSERT_THAT(IsTrue(Hide() == EVeyraCastRejection::None, TEXT("free, on cooldown or not, while it stands")));
			ASSERT_THAT(IsTrue(Ends.Num() == 1 && Ends[0].Reason == EVeyraMarkerEndReason::Recalled));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::HealthLost(*Enemy) > 0.0, TEXT("the burst hits the enemy beside it")));
			ASSERT_THAT(IsTrue(Hide() == EVeyraCastRejection::OnCooldown, TEXT("with it gone, the ability waits its cooldown")));
		}

		TEST_METHOD(DestroyedItBurstsTooAndExpiredItDoesNot)
		{
			ASSERT_THAT(IsTrue(Hide() == EVeyraCastRejection::None));
			AVeyraPlacedMarker* Illusion = Standing();
			ASSERT_THAT(IsNotNull(Illusion));
			FVeyraRawDamageEvent Hit;
			Hit.Components.Add({ EVeyraDamageType::Physical, AmbushFixture::Blow });
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*Enemy->GetAbilitySystemComponent(), *Illusion->GetAbilitySystemComponent(), Hit)));
			ASSERT_THAT(IsTrue(Ends.Num() == 1 && Ends[0].Reason == EVeyraMarkerEndReason::Destroyed));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::HealthLost(*Enemy) > 0.0, TEXT("a decoy that is struck bursts at whoever is near")));

			const double Lost = FArchetypeTestWorld::HealthLost(*Enemy);
			Caster->GetPlayerState()->FindComponentByClass<UVeyraCooldownComponent>()->ClearCooldown(ArchetypeTestId(TEXT("test_hide")));
			ASSERT_THAT(IsTrue(Hide() == EVeyraCastRejection::None));
			AmbushFixture::Wait(Spawner, AmbushFixture::Lifetime + AmbushFixture::Step * 2.0);
			ASSERT_THAT(IsTrue(Ends.Num() == 2 && Ends[1].Reason == EVeyraMarkerEndReason::Expired));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::HealthLost(*Enemy) == Lost, TEXT("one that simply ends does not burst")));
		}
	};

	// Veyra.Abilities.Ambush.*: vanish, then strike beside a Vanguard its caster hurt lately (ADR-030 §9).
	TEST_CLASS(Ambush, "Veyra.Abilities")
	{
		FActorTestSpawner Spawner;
		FVeyraAbilitiesTuning Tuning;
		AVeyraVanguardCharacter* Caster = nullptr;
		AVeyraVanguardCharacter* Enemy = nullptr;

		BEFORE_EACH()
		{
			using namespace AmbushFixture;
			FVeyraAmbushAbilityTuning Ready;
			Ready.Cast = InstantCast(Reach, Cooldown, 0.0);
			Ready.RecentSeconds = Recent;
			Ready.VanishSeconds = Vanish;
			Ready.VanishStatuses = { ArchetypeTestId(TEXT("test_gone")), ArchetypeTestId(TEXT("test_out_of_reach")) };
			Ready.BesideDistance = Beside;
			Ready.Effects = Hurts();
			FVeyraMissingHealthDamageTuning& Missing = Ready.Effects.MissingHealthDamage.AddDefaulted_GetRef();
			Missing.Type = EVeyraDamageType::Magic;
			Missing.MissingHealthRatio = 0.5;
			Tuning.Ambush.Add(ArchetypeTestId(TEXT("test_ready")), Ready);
			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_gone")), StatusOf(EVeyraStatusKind::Invisible, 0.0, Vanish));
			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_out_of_reach")), StatusOf(EVeyraStatusKind::Untargetable, 0.0, Vanish));
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(&Tuning);

			FArchetypeTestWorld World{ Spawner };
			Caster = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			Enemy = &World.Spawn(EVeyraTeam::B, FVector(Reach / 2.0, 0.0, 0.0));
			// In Q: an ultimate's slot opens only at its level, and the archetype does not care which slot holds it.
			ASSERT_THAT(IsTrue(World.Learn(*Caster, EVeyraAbilitySlot::Q, ArchetypeTestId(TEXT("test_ready")))));
		}

		AFTER_EACH()
		{
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(nullptr);
		}

		EVeyraCastRejection Ready() const
		{
			FVeyraCastTarget On;
			On.Actor = Enemy;
			On.bHasLocation = true;
			On.Location = Enemy->GetActorLocation();
			return VeyraAbilities::TryCast(*Caster->GetAbilitySystemComponent(), EVeyraAbilitySlot::Q, On);
		}

		void Hurt(double Amount) const
		{
			FVeyraRawDamageEvent Hit;
			Hit.Components.Add({ EVeyraDamageType::TrueDamage, Amount });
			VeyraCombat::DealDamage(*Caster->GetAbilitySystemComponent(), *Enemy->GetAbilitySystemComponent(), Hit);
		}

		TEST_METHOD(OnlyAVanguardItHurtLatelyIsAPlaymate)
		{
			ASSERT_THAT(IsTrue(Ready() == EVeyraCastRejection::InvalidTarget, TEXT("not one it never hurt")));
			Hurt(AmbushFixture::Blow);
			ASSERT_THAT(IsTrue(Ready() == EVeyraCastRejection::None));
		}

		TEST_METHOD(ItVanishesThenStrikesBesideItsTargetForMoreTheMoreItIsHurt)
		{
			using namespace AmbushFixture;
			Hurt(Blow * 4.0);
			const double Wounded = FArchetypeTestWorld::HealthLost(*Enemy);
			ASSERT_THAT(IsTrue(Ready() == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Has(*Caster, TEXT("test_gone")) && FArchetypeTestWorld::Has(*Caster, TEXT("test_out_of_reach")), TEXT("gone")));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::HealthLost(*Enemy) == Wounded, TEXT("not yet")));
			Wait(Spawner, Vanish + Step * 2.0);
			ASSERT_THAT(IsFalse(FArchetypeTestWorld::Has(*Caster, TEXT("test_gone")) || FArchetypeTestWorld::Has(*Caster, TEXT("test_out_of_reach")), TEXT("back")));
			const double Apart = FVector::Dist2D(Caster->GetActorLocation(), Enemy->GetActorLocation());
			const double Edges = Caster->GetSimpleCollisionRadius() + Enemy->GetSimpleCollisionRadius() + Beside;
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Apart, Edges, 2.0), FString::Printf(TEXT("beside its target: %g apart"), Apart)));
			ASSERT_THAT(IsTrue(Caster->GetActorLocation().X < Enemy->GetActorLocation().X, TEXT("on the side it came from")));
			const double Struck = FArchetypeTestWorld::HealthLost(*Enemy) - Wounded;
			ASSERT_THAT(IsTrue(Struck > Blow, FString::Printf(TEXT("its missing-Health share on top: %g"), Struck)));
		}

		TEST_METHOD(ATargetThatFallsMeanwhileLeavesItWhereItWas)
		{
			using namespace AmbushFixture;
			Hurt(Blow);
			const FVector Start = Caster->GetActorLocation();
			ASSERT_THAT(IsTrue(Ready() == EVeyraCastRejection::None));
			Hurt(100000.0);
			Wait(Spawner, Vanish + Step * 2.0);
			ASSERT_THAT(IsTrue(FVector::Dist2D(Caster->GetActorLocation(), Start) < 1.0 && !FArchetypeTestWorld::Has(*Caster, TEXT("test_gone"))));
		}

		TEST_METHOD(ARootedCasterCannotAmbush)
		{
			Hurt(AmbushFixture::Blow);
			FVeyraStatusSpec Rooted;
			Rooted.Id = ArchetypeTestId(TEXT("test_rooted"));
			Rooted.Kind = EVeyraStatusKind::Root;
			Rooted.DurationSeconds = AmbushFixture::LongSeconds;
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Enemy->GetAbilitySystemComponent(), *Caster->GetAbilitySystemComponent(), Rooted)));
			ASSERT_THAT(IsTrue(Ready() == EVeyraCastRejection::CrowdControlled));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
