// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/CapsuleComponent.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "Tethers/VeyraTetherSubsystem.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "VeyraCombatVerbs.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraAbilitiesTests
{
	// Veyra.Abilities.AttachArchetype.*: a leap that holds on to an enemy, and the recast that throws
	// its caster off (ADR-018 §2), as Patch's Bear Hug. The leap itself is a dash, covered with Patch.
	TEST_CLASS(AttachArchetype, "Veyra.Abilities")
	{
		// Fixture values.
		static constexpr double Range = 450.0;
		static constexpr double LongSeconds = 60.0;
		static constexpr double HoldSeconds = 2.5;
		static constexpr double Hug = 50.0;
		static constexpr double Throw = 300.0;
		static constexpr double Stumble = 75.0;
		static constexpr double Slowed = 0.4;

		FActorTestSpawner Spawner;
		FVeyraAbilitiesTuning Tuning;
		AVeyraVanguardCharacter* Caster = nullptr;
		AVeyraVanguardCharacter* Host = nullptr;
		UVeyraAbilityLoadoutComponent* Loadout = nullptr;

		BEFORE_EACH()
		{
			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_hugged")), StatusOf(EVeyraStatusKind::Slow, Slowed, LongSeconds));

			FVeyraAttachAbilityTuning Bear;
			Bear.Cast = InstantCast(Range, LongSeconds, 0.0);
			FVeyraRecastTuning& Recast = Bear.Cast.RecastWindow.AddDefaulted_GetRef();
			Recast.Ability = ArchetypeTestId(TEXT("test_throw"));
			Recast.WindowSeconds = HoldSeconds;
			Bear.LeapSpeed = Range * 2.0;
			Bear.ReachOnArrival = Stumble;
			Bear.AttachSeconds = HoldSeconds;
			Bear.HostStatuses.Add(ArchetypeTestId(TEXT("test_hugged")));
			FVeyraDamageTuning& Damage = Bear.HostEffects.Damage.AddDefaulted_GetRef();
			Damage.Type = EVeyraDamageType::TrueDamage;
			Damage.AmountByRank = { Hug };
			Tuning.Attach.Add(ArchetypeTestId(TEXT("test_hug")), Bear);

			FVeyraDashAbilityTuning Toss;
			Toss.Cast = InstantCast(Range, 0.0, 0.0);
			Toss.Direction = EVeyraDashDirection::AwayFromHost;
			Toss.Distance = Throw;
			Toss.Speed = Throw * 5.0;
			FVeyraDisplacementTuning& Push = Toss.HostEffects.Displacement.AddDefaulted_GetRef();
			Push.Direction = EVeyraDisplacementDirection::AwayFromOrigin;
			Push.Distance = Stumble;
			Push.Speed = Stumble * 10.0;
			Tuning.Dash.Add(ArchetypeTestId(TEXT("test_throw")), Toss);
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(&Tuning);

			FArchetypeTestWorld World{ Spawner };
			Caster = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			// Touching, so it takes hold at once, without a leap.
			const double Touching = Caster->GetCapsuleComponent()->GetScaledCapsuleRadius() * 2.0;
			Host = &World.Spawn(EVeyraTeam::B, FVector(Touching, 0.0, 0.0));
			ASSERT_THAT(IsTrue(World.Learn(*Caster, EVeyraAbilitySlot::Q, ArchetypeTestId(TEXT("test_hug")))));
			Loadout = Caster->GetPlayerState()->FindComponentByClass<UVeyraAbilityLoadoutComponent>();
		}

		AFTER_EACH()
		{
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(nullptr);
		}

		EVeyraCastRejection Hugging() const
		{
			FVeyraCastTarget Target;
			Target.Actor = Host;
			return VeyraAbilities::TryCast(*Caster->GetAbilitySystemComponent(), EVeyraAbilitySlot::Q, Target);
		}

		bool QHolds(const TCHAR* Ability) const
		{
			const FVeyraLoadoutEntry* Entry = Loadout->FindSlot(EVeyraAbilitySlot::Q);
			return Entry && Entry->Ability == ArchetypeTestId(Ability);
		}

		TEST_METHOD(TouchingItsTargetItTakesHoldAtOnce)
		{
			ASSERT_THAT(IsTrue(Hugging() == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(Caster->GetVeyraMovement()->IsAttached() && Caster->GetVeyraMovement()->GetAttachHost() == Host));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Has(*Host, TEXT("test_hugged")), TEXT("the host holds its status while it holds on")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(FArchetypeTestWorld::HealthLost(*Host), Hug)));
			ASSERT_THAT(IsTrue(QHolds(TEXT("test_throw")), TEXT("its recast is ready")));
		}

		TEST_METHOD(ATargetThatTurnsUntargetableBeforeTheGrabLandsIsMissed)
		{
			constexpr double Windup = 0.3;
			constexpr float Step = 0.05f;
			Tuning.Attach.FindChecked(ArchetypeTestId(TEXT("test_hug"))).Cast.WindupSeconds = Windup;
			FVeyraStatusSpec Gone;
			Gone.Id = ArchetypeTestId(TEXT("test_gone"));
			Gone.Kind = EVeyraStatusKind::Untargetable;
			Gone.DurationSeconds = LongSeconds;
			ASSERT_THAT(IsTrue(Hugging() == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Host->GetAbilitySystemComponent(), *Host->GetAbilitySystemComponent(), Gone)));
			UWorld& World = Spawner.GetWorld();
			const double Until = World.GetTimeSeconds() + Windup * 2.0;
			while (World.GetTimeSeconds() < Until)
			{
				World.Tick(LEVELTICK_TimeOnly, Step);
				++GFrameCounter;
				World.GetTimerManager().Tick(Step);
			}
			ASSERT_THAT(IsFalse(Caster->GetVeyraMovement()->IsAttached(), TEXT("nothing to hold on to")));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::HealthLost(*Host) == 0.0 && !FArchetypeTestWorld::Has(*Host, TEXT("test_hugged")), TEXT("and no hit")));
		}

		TEST_METHOD(ASpellShieldBlocksTheWholeGrab)
		{
			FVeyraStatusSpec Ward;
			Ward.Id = ArchetypeTestId(TEXT("test_ward"));
			Ward.Kind = EVeyraStatusKind::SpellShield;
			Ward.DurationSeconds = LongSeconds;
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Host->GetAbilitySystemComponent(), *Host->GetAbilitySystemComponent(), Ward)));
			ASSERT_THAT(IsTrue(Hugging() == EVeyraCastRejection::None));
			ASSERT_THAT(IsFalse(Caster->GetVeyraMovement()->IsAttached(), TEXT("no hold (Combat Bible §19; ADR-025 §4)")));
			ASSERT_THAT(IsFalse(FArchetypeTestWorld::Has(*Host, TEXT("test_hugged")) || FArchetypeTestWorld::Has(*Host, TEXT("test_ward")), TEXT("no status, and the shield is spent")));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::HealthLost(*Host) == 0.0, TEXT("no hit")));
			ASSERT_THAT(IsTrue(QHolds(TEXT("test_hug")), TEXT("nothing to throw")));
		}

		TEST_METHOD(TheRecastThrowsItOffAndTheHostTheOtherWay)
		{
			ASSERT_THAT(IsTrue(Hugging() == EVeyraCastRejection::None));
			const FVector From = Caster->GetActorLocation();
			const FVector HostFrom = Host->GetActorLocation();
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Caster, EVeyraAbilitySlot::Q, HostFrom) == EVeyraCastRejection::None));
			const UVeyraMovementComponent& Thrown = *Caster->GetVeyraMovement();
			const UVeyraMovementComponent& Stumbled = *Host->GetVeyraMovement();
			ASSERT_THAT(IsTrue(!Thrown.IsAttached() && Thrown.IsDashing()));
			ASSERT_THAT(IsTrue(Thrown.GetForcedMoveDestination().GetValue().X < From.X, TEXT("thrown back, away from its host")));
			ASSERT_THAT(IsTrue(Stumbled.IsDisplaced() && Stumbled.GetForcedMoveDestination().GetValue().X > HostFrom.X, TEXT("the host stumbles the other way")));
			ASSERT_THAT(IsFalse(FArchetypeTestWorld::Has(*Host, TEXT("test_hugged"))));
			ASSERT_THAT(IsTrue(QHolds(TEXT("test_hug"))));
		}

		TEST_METHOD(KnockedOffItLosesItsRecast)
		{
			ASSERT_THAT(IsTrue(Hugging() == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(VeyraCombat::Displace(*Host->GetAbilitySystemComponent(), *Caster->GetAbilitySystemComponent(),
				FVeyraDisplacement{ FVector::BackwardVector, Throw, Throw })));
			ASSERT_THAT(IsTrue(QHolds(TEXT("test_hug")), TEXT("nothing is left to throw")));
			ASSERT_THAT(IsFalse(FArchetypeTestWorld::Has(*Host, TEXT("test_hugged"))));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Caster, EVeyraAbilitySlot::Q, Host->GetActorLocation()) != EVeyraCastRejection::None,
				TEXT("its cooldown stands")));
		}

		TEST_METHOD(ValidationKeepsTheArchetypesToTheirShapes)
		{
			constexpr int32 RankCounts[] = { 5, 3 };
			ASSERT_THAT(IsTrue(VeyraAbilityRules::Validate(Tuning, RankCounts).IsEmpty(), FString::Join(VeyraAbilityRules::Validate(Tuning, RankCounts), TEXT(" | "))));
			FVeyraAbilitiesTuning Broken = Tuning;
			Broken.Attach.FindChecked(ArchetypeTestId(TEXT("test_hug"))).LeapSpeed = 0.0;
			Broken.Attach.FindChecked(ArchetypeTestId(TEXT("test_hug"))).TargetKinds = { EVeyraUnitKind::Structure };
			Broken.Dash.FindChecked(ArchetypeTestId(TEXT("test_throw"))).Direction = EVeyraDashDirection::TowardPoint;
			FVeyraTetherAbilityTuning Thread;
			Thread.Cast = InstantCast(Range, LongSeconds, 0.0);
			Thread.MaxRange = Range / 2.0;
			Thread.DurationSeconds = HoldSeconds;
			Thread.SnapSpeed = Throw;
			Broken.Tether.Add(ArchetypeTestId(TEXT("test_thread")), Thread);
			const FString All = FString::Join(VeyraAbilityRules::Validate(Broken, RankCounts), TEXT(" | "));
			ASSERT_THAT(IsTrue(All.Contains(TEXT("/attach/test_hug:")), All));
			ASSERT_THAT(IsTrue(All.Contains(TEXT("/attach/test_hug/targetKinds:")), All));
			ASSERT_THAT(IsTrue(All.Contains(TEXT("/dash/test_throw/hostEffects:")), All));
			ASSERT_THAT(IsTrue(All.Contains(TEXT("/tether/test_thread/maxRange:")), All));
			ASSERT_THAT(IsTrue(All.Contains(TEXT("/tether/test_thread/snapSpeed:")), All));
		}
	};

	// Veyra.Abilities.TetherArchetype.*: an ability that tethers an enemy to its caster (Combat Bible
	// §43), as Patch's Don't Leave Me.
	TEST_CLASS(TetherArchetype, "Veyra.Abilities")
	{
		// Fixture values.
		static constexpr double Range = 550.0;
		static constexpr double Stretch = 750.0;
		static constexpr double Seconds = 3.0;
		static constexpr double LongSeconds = 60.0;

		FActorTestSpawner Spawner;
		FVeyraAbilitiesTuning Tuning;

		BEFORE_EACH()
		{
			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_threaded")), StatusOf(EVeyraStatusKind::Counter, 0.0, LongSeconds));
			FVeyraTetherAbilityTuning Thread;
			Thread.Cast = InstantCast(Range, LongSeconds, 0.0);
			Thread.TargetKinds = { EVeyraUnitKind::Vanguard };
			Thread.MaxRange = Stretch;
			Thread.DurationSeconds = Seconds;
			Thread.TargetStatuses.Add(ArchetypeTestId(TEXT("test_threaded")));
			Tuning.Tether.Add(ArchetypeTestId(TEXT("test_thread")), Thread);
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(&Tuning);
		}

		AFTER_EACH()
		{
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(nullptr);
		}

		TEST_METHOD(ItTethersAnEnemyVanguardInRangeAndNoOtherKind)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Caster = World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(Range / 2.0, 0.0, 0.0));
			AVeyraTestFluxborn& Minion = World.SpawnFluxborn(EVeyraTeam::B, FVector(0.0, Range / 2.0, 0.0));
			ASSERT_THAT(IsTrue(World.Learn(Caster, EVeyraAbilitySlot::E, ArchetypeTestId(TEXT("test_thread")))));
			FVeyraCastTarget Target;
			Target.Actor = &Minion;
			ASSERT_THAT(IsTrue(VeyraAbilities::TryCast(*Caster.GetAbilitySystemComponent(), EVeyraAbilitySlot::E, Target) == EVeyraCastRejection::InvalidTarget));
			Target.Actor = &Enemy;
			ASSERT_THAT(IsTrue(VeyraAbilities::TryCast(*Caster.GetAbilitySystemComponent(), EVeyraAbilitySlot::E, Target) == EVeyraCastRejection::None));
			const UVeyraTetherSubsystem& Tethers = *Spawner.GetWorld().GetSubsystem<UVeyraTetherSubsystem>();
			ASSERT_THAT(IsTrue(Tethers.IsTethered(*Caster.GetAbilitySystemComponent(), ArchetypeTestId(TEXT("test_thread")))));
			ASSERT_THAT(IsTrue(Tethers.IsTetheredBy(Enemy, EVeyraTeam::A) && FArchetypeTestWorld::Has(Enemy, TEXT("test_threaded"))));
		}

		TEST_METHOD(ASpellShieldBlocksTheTether)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Caster = World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(Range / 2.0, 0.0, 0.0));
			ASSERT_THAT(IsTrue(World.Learn(Caster, EVeyraAbilitySlot::E, ArchetypeTestId(TEXT("test_thread")))));
			FVeyraStatusSpec Ward;
			Ward.Id = ArchetypeTestId(TEXT("test_ward"));
			Ward.Kind = EVeyraStatusKind::SpellShield;
			Ward.DurationSeconds = LongSeconds;
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Enemy.GetAbilitySystemComponent(), *Enemy.GetAbilitySystemComponent(), Ward)));
			FVeyraCastTarget Target;
			Target.Actor = &Enemy;
			ASSERT_THAT(IsTrue(VeyraAbilities::TryCast(*Caster.GetAbilitySystemComponent(), EVeyraAbilitySlot::E, Target) == EVeyraCastRejection::None));
			const UVeyraTetherSubsystem& Tethers = *Spawner.GetWorld().GetSubsystem<UVeyraTetherSubsystem>();
			ASSERT_THAT(IsFalse(Tethers.IsTethered(*Caster.GetAbilitySystemComponent(), ArchetypeTestId(TEXT("test_thread"))), TEXT("no tether (Combat Bible §19)")));
			ASSERT_THAT(IsFalse(FArchetypeTestWorld::Has(Enemy, TEXT("test_threaded")) || FArchetypeTestWorld::Has(Enemy, TEXT("test_ward"))));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
