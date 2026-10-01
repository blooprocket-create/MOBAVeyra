// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Cooldowns/VeyraCooldownComponent.h"
#include "Progression/VeyraProgressionComponent.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraAbilitiesTests
{
	/** Fixture values for dashes that take over, lockouts and takedown refunds. */
	namespace TakeOverFixture
	{
		constexpr double LongDash = 600.0;
		constexpr double SlowSpeed = 300.0;
		constexpr double ShortDash = 200.0;
		constexpr double QuickSpeed = 1000.0;
		constexpr double Landing = 250.0;
		constexpr double Blow = 50.0;
		constexpr double Reach = 500.0;
		constexpr double Lockout = 8.0;
		constexpr double Cooldown = 10.0;
		constexpr double Lethal = 10000.0;
		/** Enough XP for a few levels, so several slots take a point. */
		constexpr double ManyLevels = 5000.0;

		inline FVeyraDashAbilityTuning DashOf(double Distance, double Speed, EVeyraDuringDash DuringDash)
		{
			FVeyraDashAbilityTuning Dash;
			Dash.Cast = InstantCast(0.0, 0.0, 0.0);
			Dash.Distance = Distance;
			Dash.Speed = Speed;
			Dash.DuringDash = DuringDash;
			return Dash;
		}

		inline FVeyraEffectBundleTuning TrueDamage(double Amount)
		{
			FVeyraEffectBundleTuning Effects;
			FVeyraDamageTuning& Damage = Effects.Damage.AddDefaulted_GetRef();
			Damage.Type = EVeyraDamageType::TrueDamage;
			Damage.AmountByRank = { Amount };
			return Effects;
		}
	}

	// Veyra.Abilities.TakeOver.*: a dash cast during the caster's own dash is refused unless it takes
	// over; a cast that refuses a target its caster locked out; a cooldown a takedown refunds (ADR-031
	// §7–§9).
	TEST_CLASS(TakeOver, "Veyra.Abilities")
	{
		FActorTestSpawner Spawner;
		FVeyraAbilitiesTuning Tuning;
		AVeyraVanguardCharacter* Caster = nullptr;
		AVeyraVanguardCharacter* Enemy = nullptr;
		TArray<EVeyraDashEndReason> Ends;

		BEFORE_EACH()
		{
			using namespace TakeOverFixture;
			FVeyraDashAbilityTuning Slow = DashOf(LongDash, SlowSpeed, EVeyraDuringDash::Refused);
			FVeyraAreaZoneTuning& Lands = Slow.EndZones.AddDefaulted_GetRef();
			Lands.Shape = CircleOf(Landing);
			Lands.Effects = TrueDamage(Blow);
			Tuning.Dash.Add(ArchetypeTestId(TEXT("test_slow_dash")), Slow);
			Tuning.Dash.Add(ArchetypeTestId(TEXT("test_plain_dash")), DashOf(ShortDash, QuickSpeed, EVeyraDuringDash::Refused));
			Tuning.Dash.Add(ArchetypeTestId(TEXT("test_quick_dash")), DashOf(ShortDash, QuickSpeed, EVeyraDuringDash::TakesOver));
			FVeyraDashAbilityTuning Pass = DashOf(ShortDash, QuickSpeed, EVeyraDuringDash::TakesOver);
			Pass.Direction = EVeyraDashDirection::ThroughTarget;
			Pass.Cast.CastRange = Reach;
			Pass.Cast.TargetMustNotHold = { ArchetypeTestId(TEXT("test_passed")) };
			Pass.ContactEffects.Statuses = { ArchetypeTestId(TEXT("test_passed")) };
			Tuning.Dash.Add(ArchetypeTestId(TEXT("test_pass")), Pass);
			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_passed")), StatusOf(EVeyraStatusKind::Counter, 0.0, Lockout));
			FVeyraSelfBuffAbilityTuning Reset;
			Reset.Cast = InstantCast(0.0, Cooldown, 0.0);
			Reset.Cast.TakedownRefund = { 1.0 };
			Reset.Statuses = { ArchetypeTestId(TEXT("test_passed")) };
			Tuning.SelfBuff.Add(ArchetypeTestId(TEXT("test_reset")), Reset);
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(&Tuning);
			const int32 Ranks[] = { 5, 3 };
			ASSERT_THAT(IsTrue(VeyraAbilityRules::Validate(Tuning, Ranks).IsEmpty(), FString::Join(VeyraAbilityRules::Validate(Tuning, Ranks), TEXT(" | "))));

			FArchetypeTestWorld World{ Spawner };
			Caster = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			Enemy = &World.Spawn(EVeyraTeam::B, FVector(Reach / 2.0, 0.0, 0.0));
			Caster->GetVeyraMovement()->OnDashEnded.AddLambda([this](const FVeyraDashEnd& End) { Ends.Add(End.Reason); });
		}

		AFTER_EACH()
		{
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(nullptr);
		}

		/** Grants each ability in its slot and ranks each once. */
		void LearnAll(TConstArrayView<TPair<EVeyraAbilitySlot, const TCHAR*>> Kit)
		{
			APlayerState* PlayerState = Caster->GetPlayerState();
			UVeyraProgressionComponent* Progression = PlayerState->FindComponentByClass<UVeyraProgressionComponent>();
			Progression->Initialize(FVeyraStatGrowth(), 0.0);
			Progression->AddExperience(TakeOverFixture::ManyLevels);
			UVeyraAbilityLoadoutComponent* Loadout = PlayerState->FindComponentByClass<UVeyraAbilityLoadoutComponent>();
			for (const TPair<EVeyraAbilitySlot, const TCHAR*>& Entry : Kit)
			{
				ASSERT_THAT(IsTrue(Loadout->Grant(*Caster->GetAbilitySystemComponent(), Entry.Key, ArchetypeTestId(Entry.Value))));
				ASSERT_THAT(IsTrue(Progression->AllocateRank(Entry.Key) == EVeyraRankRefusal::None));
			}
		}

		EVeyraCastRejection CastOn(EVeyraAbilitySlot Slot, AActor& Unit) const
		{
			FVeyraCastTarget Target;
			Target.Actor = &Unit;
			Target.bHasLocation = true;
			Target.Location = Unit.GetActorLocation();
			return VeyraAbilities::TryCast(*Caster->GetAbilitySystemComponent(), Slot, Target);
		}

		TEST_METHOD(ADashDuringADashIsRefusedUnlessItTakesOver)
		{
			LearnAll({ { EVeyraAbilitySlot::Q, TEXT("test_slow_dash") }, { EVeyraAbilitySlot::W, TEXT("test_plain_dash") }, { EVeyraAbilitySlot::E, TEXT("test_quick_dash") } });
			const FVector Ahead(0.0, TakeOverFixture::LongDash, 0.0);
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Caster, EVeyraAbilitySlot::Q, Ahead) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(Caster->GetVeyraMovement()->IsDashing()));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Caster, EVeyraAbilitySlot::W, Ahead) == EVeyraCastRejection::Busy, TEXT("not paid for and wasted")));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Caster, EVeyraAbilitySlot::E, -Ahead) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(Ends.Num() == 1 && Ends[0] == EVeyraDashEndReason::Replaced, TEXT("the first ends Replaced")));
			ASSERT_THAT(IsTrue(Caster->GetVeyraMovement()->IsDashing(), TEXT("and the second carries on from there")));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::HealthLost(*Enemy) == 0.0, TEXT("a replaced dash lands nothing")));
		}

		TEST_METHOD(ALockedOutTargetIsRefusedAndAnotherIsNot)
		{
			LearnAll({ { EVeyraAbilitySlot::E, TEXT("test_pass") } });
			ASSERT_THAT(IsTrue(CastOn(EVeyraAbilitySlot::E, *Enemy) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Has(*Enemy, TEXT("test_passed")), TEXT("locked out")));
			ASSERT_THAT(IsTrue(CastOn(EVeyraAbilitySlot::E, *Enemy) == EVeyraCastRejection::InvalidTarget));
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Other = World.Spawn(EVeyraTeam::B, Caster->GetActorLocation() + FVector(0.0, TakeOverFixture::Reach / 2.0, 0.0));
			ASSERT_THAT(IsTrue(CastOn(EVeyraAbilitySlot::E, Other) == EVeyraCastRejection::None, TEXT("each target has its own lockout")));
		}

		TEST_METHOD(ATakedownRefundsTheCooldownAndAFluxbornKillDoesNot)
		{
			LearnAll({ { EVeyraAbilitySlot::Q, TEXT("test_reset") } });
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Caster, EVeyraAbilitySlot::Q, Caster->GetActorLocation()) == EVeyraCastRejection::None));
			const UVeyraCooldownComponent* Cooldowns = Caster->GetPlayerState()->FindComponentByClass<UVeyraCooldownComponent>();
			const FVeyraContentId Reset = ArchetypeTestId(TEXT("test_reset"));
			ASSERT_THAT(IsTrue(Cooldowns->GetRemainingSecondsNow(Reset) > 0.0));
			FVeyraRawDamageEvent Kill;
			Kill.Components.Add({ EVeyraDamageType::TrueDamage, TakeOverFixture::Lethal });
			FArchetypeTestWorld World{ Spawner };
			AVeyraTestFluxborn& Fluxborn = World.SpawnFluxborn(EVeyraTeam::B, FVector(0.0, -TakeOverFixture::Reach, 0.0));
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*Caster->GetAbilitySystemComponent(), *Fluxborn.GetAbilitySystemComponent(), Kill)));
			ASSERT_THAT(IsTrue(Cooldowns->GetRemainingSecondsNow(Reset) > 0.0, TEXT("a Fluxborn is no takedown")));
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*Caster->GetAbilitySystemComponent(), *Enemy->GetAbilitySystemComponent(), Kill)));
			ASSERT_THAT(IsTrue(Cooldowns->GetRemainingSecondsNow(Reset) == 0.0, TEXT("an enemy Vanguard's death resets it")));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
