// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Attributes/VeyraVitalsSet.h"
#include "Companions/VeyraCompanion.h"
#include "Companions/VeyraCompanionSubsystem.h"
#include "Cooldowns/VeyraCooldownComponent.h"
#include "CQTest.h"
#include "Delivery/VeyraLingeringArea.h"
#include "EngineUtils.h"
#include "Progression/VeyraProgressionComponent.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "TimerManager.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "VeyraVanguards.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraVanguardsTests
{
	using VeyraAbilitiesTests::FArchetypeTestWorld;

	/** Fixture values for Neris's tests: XP for many levels, a blow, where units stand, and a step of time. */
	namespace NerisFixture
	{
		constexpr double ManyLevels = 50000.0;
		constexpr double Injury = 200.0;
		constexpr double Near = 150.0;
		constexpr float Step = 0.1f;
	}

	// Veyra.Vanguards.Tidebound.*: Neris's kit (Roster Bible §11; ADR-035), from the committed tuning.
	TEST_CLASS(Tidebound, "Veyra.Vanguards")
	{
		FActorTestSpawner Spawner;
		AVeyraVanguardCharacter* Neris = nullptr;

		static FVeyraContentId Id(const TCHAR* Text)
		{
			return FVeyraContentId::FromText(Text).GetValue();
		}

		BEFORE_EACH()
		{
			FArchetypeTestWorld World{ Spawner };
			Neris = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			AVeyraPlayerState* Participant = Neris->GetPlayerState<AVeyraPlayerState>();
			const FVeyraPreparedVanguard Prepared = VeyraVanguards::PrepareCombatant(*Participant->GetAbilitySystemComponent(), Id(TEXT("neris")));
			ASSERT_THAT(IsTrue(Prepared.bPrepared));
			UVeyraProgressionComponent* Progression = Participant->FindComponentByClass<UVeyraProgressionComponent>();
			Progression->AddExperience(NerisFixture::ManyLevels);
			for (const EVeyraAbilitySlot Slot : { EVeyraAbilitySlot::Q, EVeyraAbilitySlot::W, EVeyraAbilitySlot::E, EVeyraAbilitySlot::R })
			{
				ASSERT_THAT(IsTrue(Progression->AllocateRank(Slot) == EVeyraRankRefusal::None));
			}
		}

		UAbilitySystemComponent& Abilities() const
		{
			return *Neris->GetAbilitySystemComponent();
		}

		bool Holds(EVeyraAbilitySlot Slot, const TCHAR* Ability) const
		{
			const UVeyraAbilityLoadoutComponent* Loadout = Neris->GetPlayerState()->FindComponentByClass<UVeyraAbilityLoadoutComponent>();
			const FVeyraLoadoutEntry* Entry = Loadout ? Loadout->FindSlot(Slot) : nullptr;
			return Entry && Entry->Ability == Id(Ability);
		}

		EVeyraCastRejection CastOn(EVeyraAbilitySlot Slot, AActor& Unit) const
		{
			FVeyraCastTarget Target;
			Target.Actor = &Unit;
			Target.bHasLocation = true;
			Target.Location = Unit.GetActorLocation();
			return VeyraAbilities::TryCast(Abilities(), Slot, Target);
		}

		EVeyraCastRejection CastHere(EVeyraAbilitySlot Slot) const
		{
			return FArchetypeTestWorld::CastAt(*Neris, Slot, Neris->GetActorLocation() + FVector(NerisFixture::Near, 0.0, 0.0));
		}

		void Wait(double Seconds)
		{
			UWorld& World = Spawner.GetWorld();
			const double Until = World.GetTimeSeconds() + Seconds;
			while (World.GetTimeSeconds() < Until)
			{
				World.Tick(LEVELTICK_TimeOnly, NerisFixture::Step);
				++GFrameCounter;
				World.GetTimerManager().Tick(NerisFixture::Step);
			}
		}

		static double Lost(const AActor& Unit)
		{
			const UAbilitySystemComponent& Health = *UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Unit);
			return Health.GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute()) - Health.GetNumericAttribute(UVeyraVitalsSet::GetHealthAttribute());
		}

		static bool Wound(AActor& Source, AActor& Target, double Amount)
		{
			FVeyraRawDamageEvent Damage;
			Damage.Components.Add({ EVeyraDamageType::TrueDamage, Amount });
			return VeyraCombat::DealDamage(*UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Source),
				*UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Target), Damage);
		}

		/** The lingering areas of Ability standing now. */
		int32 Areas(const TCHAR* Ability)
		{
			int32 Count = 0;
			for (TActorIterator<AVeyraLingeringArea> It(&Spawner.GetWorld()); It; ++It)
			{
				Count += It->GetAbility() == Id(Ability) ? 1 : 0;
			}
			return Count;
		}

		TEST_METHOD(BreakingWaveRidesAndItsRecastCrashesIt)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(NerisFixture::Near, 0.0, 0.0));
			AVeyraVanguardCharacter& Ally = World.Spawn(EVeyraTeam::A, FVector(0.0, NerisFixture::Near, 0.0));
			ASSERT_THAT(IsTrue(Wound(Enemy, Ally, NerisFixture::Injury)));
			ASSERT_THAT(IsTrue(CastHere(EVeyraAbilitySlot::Q) == EVeyraCastRejection::None && Neris->GetVeyraMovement()->IsRiding()));
			ASSERT_THAT(IsTrue(Holds(EVeyraAbilitySlot::Q, TEXT("neris_wave_crash")), TEXT("its recast holds Q while she rides")));
			ASSERT_THAT(IsTrue(CastHere(EVeyraAbilitySlot::Q) == EVeyraCastRejection::None));
			ASSERT_THAT(IsFalse(Neris->GetVeyraMovement()->IsRiding()));
			ASSERT_THAT(IsTrue(Lost(Enemy) > 0.0, TEXT("the crash strikes the enemy")));
			ASSERT_THAT(IsTrue(Lost(Ally) < NerisFixture::Injury, TEXT("and heals the ally")));
		}

		TEST_METHOD(ChangeTheWeatherSwapsHerKitAndKeepsItsCooldowns)
		{
			ASSERT_THAT(IsTrue(Holds(EVeyraAbilitySlot::Q, TEXT("neris_breaking_wave")) && Holds(EVeyraAbilitySlot::R, TEXT("neris_tidebreaker"))));
			ASSERT_THAT(IsTrue(CastHere(EVeyraAbilitySlot::Q) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(CastHere(EVeyraAbilitySlot::Q) == EVeyraCastRejection::None, TEXT("crash out of the ride")));
			ASSERT_THAT(IsTrue(CastHere(EVeyraAbilitySlot::E) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(Holds(EVeyraAbilitySlot::Q, TEXT("neris_breaking_wave_storm")) && Holds(EVeyraAbilitySlot::W, TEXT("neris_little_current_storm"))
				&& Holds(EVeyraAbilitySlot::R, TEXT("neris_tidebreaker_storm")), TEXT("Storm Waters hold Q, W and R")));
			ASSERT_THAT(IsTrue(CastHere(EVeyraAbilitySlot::Q) == EVeyraCastRejection::OnCooldown, TEXT("Storm's wave waits on Calm's cooldown")));
			const UVeyraCooldownComponent& Cooldowns = *Neris->GetPlayerState()->FindComponentByClass<UVeyraCooldownComponent>();
			ASSERT_THAT(IsTrue(Cooldowns.GetRemainingSecondsNow(Id(TEXT("neris_change_the_weather"))) > 0.0, TEXT("and the switch has a cooldown of its own")));
		}

		TEST_METHOD(LittleCurrentEscortsAnAllyInCalmAndItsRecastRedirectsIt)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& First = World.Spawn(EVeyraTeam::A, FVector(NerisFixture::Near, 0.0, 0.0));
			AVeyraVanguardCharacter& Second = World.Spawn(EVeyraTeam::A, FVector(0.0, NerisFixture::Near, 0.0));
			ASSERT_THAT(IsTrue(CastOn(EVeyraAbilitySlot::W, First) == EVeyraCastRejection::None));
			const AVeyraCompanion* Waterling = Spawner.GetWorld().GetSubsystem<UVeyraCompanionSubsystem>()->Find(Abilities());
			ASSERT_THAT(IsTrue(Waterling && Waterling->GetMode() == EVeyraCompanionMode::Escort && Waterling->GetBoundTo() == &First));
			ASSERT_THAT(IsTrue(Holds(EVeyraAbilitySlot::W, TEXT("neris_little_current_redirect"))));
			ASSERT_THAT(IsTrue(CastOn(EVeyraAbilitySlot::W, Second) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(Waterling->GetBoundTo() == &Second));
		}

		TEST_METHOD(InStormTheWaterlingHuntsAnEnemy)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(NerisFixture::Near * 2.0, 0.0, 0.0));
			ASSERT_THAT(IsTrue(CastHere(EVeyraAbilitySlot::E) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(CastOn(EVeyraAbilitySlot::W, Enemy) == EVeyraCastRejection::None));
			const AVeyraCompanion* Waterling = Spawner.GetWorld().GetSubsystem<UVeyraCompanionSubsystem>()->Find(Abilities());
			ASSERT_THAT(IsTrue(Waterling && Waterling->GetMode() == EVeyraCompanionMode::Hunt && Waterling->GetBoundTo() == &Enemy));
		}

		TEST_METHOD(TidebreakerLocksTheSeaStateAndLeavesItsWake)
		{
			ASSERT_THAT(IsTrue(CastHere(EVeyraAbilitySlot::R) == EVeyraCastRejection::None && Neris->GetVeyraMovement()->IsRiding()));
			ASSERT_THAT(AreEqual(1, Areas(TEXT("neris_healing_wake")), TEXT("a Healing Wake as it sets off")));
			ASSERT_THAT(IsTrue(CastHere(EVeyraAbilitySlot::E) == EVeyraCastRejection::HeldBack, TEXT("the Sea State holds while it rides")));
			const FVeyraRideAbilityTuning* Tidebreaker = UVeyraAbilitiesTuningSubsystem::FindRide(Id(TEXT("neris_tidebreaker")));
			Wait(Tidebreaker->DurationSeconds + NerisFixture::Step * 3.0);
			ASSERT_THAT(IsFalse(Neris->GetVeyraMovement()->IsRiding()));
			ASSERT_THAT(IsTrue(CastHere(EVeyraAbilitySlot::E) == EVeyraCastRejection::None, TEXT("and may change once it ends")));
		}

		TEST_METHOD(StormsTidebreakerLeavesARiptide)
		{
			ASSERT_THAT(IsTrue(CastHere(EVeyraAbilitySlot::E) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(CastHere(EVeyraAbilitySlot::R) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(Areas(TEXT("neris_riptide")) == 1 && Areas(TEXT("neris_healing_wake")) == 0));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
