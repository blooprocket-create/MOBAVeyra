// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Attacks/VeyraBasicAttackComponent.h"
#include "CQTest.h"
#include "Delivery/VeyraEffectDelivery.h"
#include "Delivery/VeyraProjectile.h"
#include "EngineUtils.h"
#include "Statuses/VeyraStatusComponent.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "VeyraCombatVerbs.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraCoverTests
{
	using namespace VeyraAbilitiesTests;

	// Veyra.Combat.Cover.*: an ally's cover against projectiles (Combat Bible §20; ADR-037 §4).
	TEST_CLASS(Cover, "Veyra.Combat")
	{
		// Fixture values: the cover faces +X from the origin; the shooter stands ahead of it, the sheltered behind.
		static constexpr double Hit = 100.0;
		static constexpr double Share = 0.5;
		static constexpr double Arc = 90.0;
		static constexpr double Reach = 300.0;
		static constexpr double Capacity = 1000.0;
		static constexpr double Transfer = 0.25;
		static constexpr double Behind = 150.0;
		static constexpr double Ahead = 600.0;
		static constexpr double LongSeconds = 60.0;
		static constexpr double ShotSpeed = 1000.0;
		static constexpr double ShotRadius = 10.0;
		static constexpr double Tolerance = 1e-3;

		FActorTestSpawner Spawner;

		static FVeyraStatusSpec CoverSpec(double Magnitude = Share, double Left = Capacity)
		{
			FVeyraStatusSpec Spec;
			Spec.Id = ArchetypeTestId(TEXT("test_bulwark"));
			Spec.Kind = EVeyraStatusKind::Cover;
			Spec.Magnitude = Magnitude;
			Spec.DurationSeconds = LongSeconds;
			Spec.ArcDegrees = Arc;
			Spec.CoverReach = Reach;
			Spec.CoverCapacity = Left;
			Spec.CoverTransferShare = Transfer;
			return Spec;
		}

		static FVeyraRawDamageEvent Shot(bool bProjectile = true)
		{
			FVeyraRawDamageEvent Damage;
			Damage.Components.Add({ EVeyraDamageType::TrueDamage, Hit });
			Damage.bProjectile = bProjectile;
			return Damage;
		}

		static bool Hold(AVeyraVanguardCharacter& Holder, const FVeyraStatusSpec& Spec)
		{
			UAbilitySystemComponent& Self = *Holder.GetAbilitySystemComponent();
			return VeyraCombat::ApplyStatus(Self, Self, Spec);
		}

		static bool ShootAt(AVeyraVanguardCharacter& Shooter, AActor& Target, bool bProjectile = true)
		{
			return VeyraCombat::DealDamage(*Shooter.GetAbilitySystemComponent(), *UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Target), Shot(bProjectile));
		}

		TEST_METHOD(AShotFromAheadLosesItsShareAndItsHolderTakesSomeOfIt)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Holder = World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			AVeyraVanguardCharacter& Sheltered = World.Spawn(EVeyraTeam::A, FVector(-Behind, 0.0, 0.0));
			AVeyraVanguardCharacter& Shooter = World.Spawn(EVeyraTeam::B, FVector(Ahead, 0.0, 0.0));
			ASSERT_THAT(IsTrue(Hold(Holder, CoverSpec())));
			ASSERT_THAT(IsTrue(ShootAt(Shooter, Sheltered)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(World.HealthLost(Sheltered), Hit * (1.0 - Share), Tolerance), FString::Printf(TEXT("lost %g"), World.HealthLost(Sheltered))));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(World.HealthLost(Holder), Hit * Share * Transfer, Tolerance), FString::Printf(TEXT("holder lost %g"), World.HealthLost(Holder))));
		}

		TEST_METHOD(MeleeAndShotsFromBesideOrPastItsReachPass)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Holder = World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			AVeyraVanguardCharacter& Sheltered = World.Spawn(EVeyraTeam::A, FVector(-Behind, 0.0, 0.0));
			AVeyraVanguardCharacter& Far = World.Spawn(EVeyraTeam::A, FVector(-Reach * 2.0, 0.0, 0.0));
			AVeyraVanguardCharacter& InFront = World.Spawn(EVeyraTeam::A, FVector(Behind, 0.0, 0.0));
			AVeyraVanguardCharacter& Shooter = World.Spawn(EVeyraTeam::B, FVector(Ahead, 0.0, 0.0));
			AVeyraVanguardCharacter& Flanker = World.Spawn(EVeyraTeam::B, FVector(0.0, Ahead, 0.0));
			ASSERT_THAT(IsTrue(Hold(Holder, CoverSpec())));
			ASSERT_THAT(IsTrue(ShootAt(Shooter, Sheltered, /*bProjectile*/ false)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(World.HealthLost(Sheltered), Hit, Tolerance), TEXT("what no projectile carries passes")));
			ASSERT_THAT(IsTrue(ShootAt(Flanker, Sheltered)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(World.HealthLost(Sheltered), Hit * 2.0, Tolerance), TEXT("a shot from outside its arc passes")));
			ASSERT_THAT(IsTrue(ShootAt(Shooter, Far)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(World.HealthLost(Far), Hit, Tolerance), TEXT("past its reach, no shelter")));
			ASSERT_THAT(IsTrue(ShootAt(Shooter, InFront)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(World.HealthLost(InFront), Hit, Tolerance), TEXT("in front of it, no shelter")));
			ASSERT_THAT(IsTrue(ShootAt(Shooter, Holder)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(World.HealthLost(Holder), Hit, Tolerance), TEXT("it never shelters its holder")));
		}

		TEST_METHOD(ItsCapacityRunsOutAndRefillsWhenItIsGivenAgain)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Holder = World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			AVeyraVanguardCharacter& Sheltered = World.Spawn(EVeyraTeam::A, FVector(-Behind, 0.0, 0.0));
			AVeyraVanguardCharacter& Shooter = World.Spawn(EVeyraTeam::B, FVector(Ahead, 0.0, 0.0));
			// Room for one full share and a little more.
			const double Small = Hit * Share * 1.2;
			ASSERT_THAT(IsTrue(Hold(Holder, CoverSpec(Share, Small))));
			ASSERT_THAT(IsTrue(ShootAt(Shooter, Sheltered)));
			double Expected = Hit * (1.0 - Share);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(World.HealthLost(Sheltered), Expected, Tolerance)));
			ASSERT_THAT(IsTrue(ShootAt(Shooter, Sheltered)));
			Expected += Hit - (Small - Hit * Share);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(World.HealthLost(Sheltered), Expected, Tolerance), FString::Printf(TEXT("the rest of its capacity: lost %g"), World.HealthLost(Sheltered))));
			ASSERT_THAT(IsTrue(ShootAt(Shooter, Sheltered)));
			Expected += Hit;
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(World.HealthLost(Sheltered), Expected, Tolerance), TEXT("spent, it shelters no more")));
			ASSERT_THAT(IsTrue(Hold(Holder, CoverSpec(Share, Small))));
			ASSERT_THAT(IsTrue(ShootAt(Shooter, Sheltered)));
			Expected += Hit * (1.0 - Share);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(World.HealthLost(Sheltered), Expected, Tolerance), TEXT("given again, it is whole again")));
		}

		TEST_METHOD(TheStrongestCoverAnswersAlone)
		{
			constexpr double Weaker = 0.3;
			constexpr double Stronger = 0.6;
			constexpr double Beside = 50.0;
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Light = World.Spawn(EVeyraTeam::A, FVector(0.0, Beside, 0.0));
			AVeyraVanguardCharacter& Heavy = World.Spawn(EVeyraTeam::A, FVector(0.0, -Beside, 0.0));
			AVeyraVanguardCharacter& Sheltered = World.Spawn(EVeyraTeam::A, FVector(-Behind, 0.0, 0.0));
			AVeyraVanguardCharacter& Shooter = World.Spawn(EVeyraTeam::B, FVector(Ahead, 0.0, 0.0));
			ASSERT_THAT(IsTrue(Hold(Light, CoverSpec(Weaker))));
			ASSERT_THAT(IsTrue(Hold(Heavy, CoverSpec(Stronger))));
			ASSERT_THAT(IsTrue(ShootAt(Shooter, Sheltered)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(World.HealthLost(Sheltered), Hit * (1.0 - Stronger), Tolerance), FString::Printf(TEXT("lost %g"), World.HealthLost(Sheltered))));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(World.HealthLost(Heavy), Hit * Stronger * Transfer, Tolerance)));
			ASSERT_THAT(IsTrue(World.HealthLost(Light) == 0.0, TEXT("the weaker cover takes nothing")));
		}

		TEST_METHOD(ItSheltersOnlyTheKindsItNames)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Holder = World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			AVeyraVanguardCharacter& Vanguard = World.Spawn(EVeyraTeam::A, FVector(-Behind, 0.0, 0.0));
			AVeyraTestFluxborn& Fluxborn = World.SpawnFluxborn(EVeyraTeam::A, FVector(-Behind, Behind / 2.0, 0.0));
			AVeyraVanguardCharacter& Shooter = World.Spawn(EVeyraTeam::B, FVector(Ahead, 0.0, 0.0));
			FVeyraStatusSpec Spec = CoverSpec();
			Spec.UnitKinds = { EVeyraUnitKind::Fluxborn };
			ASSERT_THAT(IsTrue(Hold(Holder, Spec)));
			ASSERT_THAT(IsTrue(ShootAt(Shooter, Vanguard)));
			ASSERT_THAT(IsTrue(ShootAt(Shooter, Fluxborn)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(World.HealthLost(Vanguard), Hit, Tolerance)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(World.HealthLost(Fluxborn), Hit * (1.0 - Share), Tolerance)));
		}

		TEST_METHOD(ARangedAttackIsShelteredAndAMeleeOneIsNot)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Holder = World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			AVeyraVanguardCharacter& Sheltered = World.Spawn(EVeyraTeam::A, FVector(-Behind, 0.0, 0.0));
			AVeyraVanguardCharacter& Archer = World.Spawn(EVeyraTeam::B, FVector(Ahead, 0.0, 0.0));
			AVeyraVanguardCharacter& Brawler = World.Spawn(EVeyraTeam::B, FVector(Behind, 0.0, 0.0));
			ASSERT_THAT(IsTrue(Hold(Holder, CoverSpec())));
			FVeyraBasicAttackProfile Melee;
			Melee.Range = Behind * 3.0;
			Melee.DamageType = EVeyraDamageType::TrueDamage;
			Melee.PhysicalPowerRatio = 1.0;
			Melee.WindupFraction = 0.25;
			Melee.AcquisitionRadius = Melee.Range;
			FVeyraBasicAttackProfile Ranged = Melee;
			Ranged.Range = Ahead + Behind * 2.0;
			Ranged.AcquisitionRadius = Ranged.Range;
			Ranged.Projectile.Add(FVeyraAttackProjectileTuning{ ShotSpeed, ShotRadius });
			UVeyraBasicAttackComponent* Bow = Archer.GetPlayerState()->FindComponentByClass<UVeyraBasicAttackComponent>();
			UVeyraBasicAttackComponent* Fists = Brawler.GetPlayerState()->FindComponentByClass<UVeyraBasicAttackComponent>();
			ASSERT_THAT(IsTrue(Bow && Fists && Bow->SetProfile(Ranged) && Fists->SetProfile(Melee)));
			const double Base = VeyraCombatTests::ExampleStats().PhysicalPower;

			ASSERT_THAT(IsTrue(Bow->StartAttack(Sheltered) == EVeyraAttackRejection::None));
			Bow->Commit();
			TActorIterator<AVeyraProjectile> Flight(&Spawner.GetWorld());
			ASSERT_THAT(IsTrue(static_cast<bool>(Flight)));
			Flight->AdvanceBy(Ranged.Range / ShotSpeed);
			const double FromTheShot = World.HealthLost(Sheltered);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(FromTheShot, Base * (1.0 - Share), Tolerance), FString::Printf(TEXT("the shot took %g"), FromTheShot)));
			ASSERT_THAT(IsTrue(Fists->StartAttack(Sheltered) == EVeyraAttackRejection::None));
			Fists->Commit();
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(World.HealthLost(Sheltered) - FromTheShot, Base, Tolerance), TEXT("a blow passes the cover")));
		}

		TEST_METHOD(AnAbilitysProjectileIsSheltered)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Holder = World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			AVeyraVanguardCharacter& Sheltered = World.Spawn(EVeyraTeam::A, FVector(-Behind, 0.0, 0.0));
			AVeyraVanguardCharacter& Caster = World.Spawn(EVeyraTeam::B, FVector(Ahead, 0.0, 0.0));
			ASSERT_THAT(IsTrue(Hold(Holder, CoverSpec())));
			UAbilitySystemComponent& Source = *Caster.GetAbilitySystemComponent();
			FVeyraPreparedEffects Effects;
			const FVeyraRawDamageEvent Raw = Shot(/*bProjectile*/ false);
			Effects.Damage = VeyraCombat::PrepareDamage(Source, Raw);
			Effects.RawDamage = Raw.Components;
			AVeyraProjectile& Bolt = Spawner.SpawnActorAt<AVeyraProjectile>(Caster.GetActorLocation(), FRotator::ZeroRotator);
			Bolt.LaunchHoming(Source, Sheltered, ShotSpeed, ShotRadius, Effects, ArchetypeTestId(TEXT("test_rivet")), 1);
			Bolt.AdvanceBy((Ahead + Behind) * 2.0 / ShotSpeed);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(World.HealthLost(Sheltered), Hit * (1.0 - Share), Tolerance), FString::Printf(TEXT("lost %g"), World.HealthLost(Sheltered))));
		}

		TEST_METHOD(ItsRulesRefuseAMalformedCoverAndDesignation)
		{
			ASSERT_THAT(IsTrue(VeyraStatuses::Validate(CoverSpec()).IsEmpty()));
			FVeyraStatusSpec Whole = CoverSpec(1.0);
			ASSERT_THAT(IsFalse(VeyraStatuses::Validate(Whole).IsEmpty(), TEXT("it prevents less than all")));
			FVeyraStatusSpec Stacked = CoverSpec();
			Stacked.MaxStacks = 2;
			ASSERT_THAT(IsFalse(VeyraStatuses::Validate(Stacked).IsEmpty(), TEXT("one stack")));
			FVeyraStatusSpec Empty = CoverSpec(Share, 0.0);
			ASSERT_THAT(IsFalse(VeyraStatuses::Validate(Empty).IsEmpty(), TEXT("it holds something")));
			FVeyraStatusSpec Generous = CoverSpec();
			Generous.CoverTransferShare = 1.5;
			ASSERT_THAT(IsFalse(VeyraStatuses::Validate(Generous).IsEmpty(), TEXT("its holder takes at most all")));
			FVeyraStatusSpec Armless = CoverSpec();
			Armless.ArcDegrees = 0.0;
			ASSERT_THAT(IsFalse(VeyraStatuses::Validate(Armless).IsEmpty(), TEXT("it faces an arc")));
			FVeyraStatusSpec Stray = CoverSpec();
			Stray.Kind = EVeyraStatusKind::DamageReduction;
			Stray.ArcDegrees = 0.0;
			ASSERT_THAT(IsFalse(VeyraStatuses::Validate(Stray).IsEmpty(), TEXT("no other kind carries cover")));
			FVeyraStatusSpec Designated;
			Designated.Id = ArchetypeTestId(TEXT("test_designated"));
			Designated.Kind = EVeyraStatusKind::Designated;
			Designated.DurationSeconds = LongSeconds;
			ASSERT_THAT(IsTrue(VeyraStatuses::Validate(Designated).IsEmpty()));
			Designated.Magnitude = Share;
			ASSERT_THAT(IsFalse(VeyraStatuses::Validate(Designated).IsEmpty(), TEXT("a designation changes no stat")));
		}

		TEST_METHOD(ItSheltersWhatStandsBehindItFromWhatComesAhead)
		{
			const FVector Facing(1.0, 0.0, 0.0);
			const FVector Source(Ahead, 0.0, 0.0);
			ASSERT_THAT(IsTrue(VeyraStatuses::Shelters(FVector::ZeroVector, Facing, Arc, Reach, FVector(-Behind, 0.0, 0.0), Source)));
			ASSERT_THAT(IsTrue(VeyraStatuses::Shelters(FVector::ZeroVector, Facing, Arc, Reach, FVector(0.0, Behind, 0.0), Source), TEXT("beside it counts")));
			ASSERT_THAT(IsFalse(VeyraStatuses::Shelters(FVector::ZeroVector, Facing, Arc, Reach, FVector(Behind, 0.0, 0.0), Source), TEXT("not ahead of it")));
			ASSERT_THAT(IsFalse(VeyraStatuses::Shelters(FVector::ZeroVector, Facing, Arc, Reach, FVector(-Reach * 2.0, 0.0, 0.0), Source), TEXT("not past its reach")));
			ASSERT_THAT(IsFalse(VeyraStatuses::Shelters(FVector::ZeroVector, Facing, Arc, Reach, FVector(-Behind, 0.0, 0.0), FVector(0.0, Ahead, 0.0)), TEXT("not from outside its arc")));
			ASSERT_THAT(IsFalse(VeyraStatuses::Shelters(FVector::ZeroVector, Facing, Arc, Reach, FVector(-Behind, 0.0, 0.0), FVector(-Ahead, 0.0, 0.0)), TEXT("not from behind")));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
