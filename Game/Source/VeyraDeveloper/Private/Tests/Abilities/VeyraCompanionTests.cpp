// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Attacks/VeyraBasicAttackComponent.h"
#include "Attributes/VeyraOffenceSet.h"
#include "Attributes/VeyraVitalsSet.h"
#include "Companions/VeyraCompanion.h"
#include "Companions/VeyraCompanionController.h"
#include "Companions/VeyraCompanionRules.h"
#include "Companions/VeyraCompanionSubsystem.h"
#include "Components/CapsuleComponent.h"
#include "CQTest.h"
#include "Stats/VeyraEquipmentStats.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraAbilitiesTests
{
	// Veyra.Abilities.Companions.*: a Vanguard's companion forms, follows, fights, is banished and reforms
	// (ADR-034 §3, §4).
	TEST_CLASS(Companions, "Veyra.Abilities")
	{
		FActorTestSpawner Spawner;
		FVeyraAbilitiesTuning Tuning;
		AVeyraVanguardCharacter* Owner = nullptr;

		BEFORE_EACH()
		{
			Tuning.Companions.Add(ArchetypeTestId(TEXT("test_pet")), ExampleCompanion());
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(&Tuning);
			const int32 Ranks[] = { 5, 3 };
			ASSERT_THAT(IsTrue(VeyraAbilityRules::Validate(Tuning, Ranks).IsEmpty(), FString::Join(VeyraAbilityRules::Validate(Tuning, Ranks), TEXT(" | "))));

			FArchetypeTestWorld World{ Spawner };
			Owner = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			ASSERT_THAT(IsTrue(Keeper().Summon(OwnerAbilities(), ArchetypeTestId(TEXT("test_pet")))));
			ASSERT_THAT(IsNotNull(Pet()));
		}

		AFTER_EACH()
		{
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(nullptr);
		}

		UVeyraCompanionSubsystem& Keeper()
		{
			return *Spawner.GetWorld().GetSubsystem<UVeyraCompanionSubsystem>();
		}

		UAbilitySystemComponent& OwnerAbilities() const
		{
			return *Owner->GetAbilitySystemComponent();
		}

		AVeyraCompanion* Pet()
		{
			return Keeper().Find(OwnerAbilities());
		}

		AVeyraCompanionController& Brain()
		{
			return *CastChecked<AVeyraCompanionController>(Pet()->GetController());
		}

		static double Stat(const AActor& Unit, const FGameplayAttribute& Attribute)
		{
			return UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Unit)->GetNumericAttribute(Attribute);
		}

		void Wait(double Seconds)
		{
			UWorld& World = Spawner.GetWorld();
			const double Until = World.GetTimeSeconds() + Seconds;
			while (World.GetTimeSeconds() < Until)
			{
				World.Tick(LEVELTICK_TimeOnly, CompanionFixture::Step);
				++GFrameCounter;
				World.GetTimerManager().Tick(CompanionFixture::Step);
			}
		}

		bool Hit(AActor& Source, AActor& Target, double Amount) const
		{
			FVeyraRawDamageEvent Damage;
			Damage.Components.Add({ EVeyraDamageType::TrueDamage, Amount });
			return VeyraCombat::DealDamage(*UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Source),
				*UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Target), Damage);
		}

		TEST_METHOD(ItFormsBesideItsOwnerOnItsSideAndAnswersToIt)
		{
			AVeyraCompanion& Companion = *Pet();
			ASSERT_THAT(IsTrue(Companion.GetVeyraTeam() == EVeyraTeam::A && Companion.GetVeyraUnitKind() == EVeyraUnitKind::Companion));
			ASSERT_THAT(IsTrue(Companion.GetOwnerAbilities() == &OwnerAbilities()));
			ASSERT_THAT(IsTrue(VeyraCombat::ResponsibleFor(Companion.GetAbilitySystemComponent()) == &OwnerAbilities()));
			ASSERT_THAT(IsTrue(VeyraTargeting::EdgeToEdgeDistance(Companion, *Owner) <= CompanionFixture::Follow, TEXT("beside its owner")));
			ASSERT_THAT(IsTrue(Companion.IsAlive() && !Companion.IsBanished()));
			ASSERT_THAT(IsTrue(Companion.GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Ignore, TEXT("ghosted: it never traps its owner")));
			ASSERT_THAT(IsFalse(Keeper().Summon(OwnerAbilities(), ArchetypeTestId(TEXT("test_pet"))), TEXT("one at most")));
		}

		TEST_METHOD(ItWaitsForItsOwnersBodyBeforeItForms)
		{
			// A participant's abilities answer for its PlayerState until its Vanguard spawns, as a passive
			// that summons at the start of a match finds them.
			AVeyraPlayerState& Participant = Spawner.SpawnActor<AVeyraPlayerState>();
			Participant.SetVeyraTeam(EVeyraTeam::A);
			UAbilitySystemComponent& Abilities = *Participant.GetAbilitySystemComponent();
			VeyraCombat::InitializeStats(Abilities, VeyraCombatTests::ExampleStats());
			ASSERT_THAT(IsTrue(Abilities.GetAvatarActor() == &Participant));
			ASSERT_THAT(IsTrue(Keeper().Summon(Abilities, ArchetypeTestId(TEXT("test_pet")))));
			ASSERT_THAT(IsNull(Keeper().Find(Abilities), TEXT("nothing forms beside a participant with no body")));

			AVeyraVanguardCharacter& Body = Spawner.SpawnActorAt<AVeyraVanguardCharacter>(FVector(CompanionFixture::Far, 0.0, 0.0), FRotator::ZeroRotator);
			Body.SetPlayerState(&Participant);
			Keeper().Keep(Abilities);
			const AVeyraCompanion* Companion = Keeper().Find(Abilities);
			ASSERT_THAT(IsNotNull(Companion, TEXT("it forms once its owner has a body")));
			ASSERT_THAT(IsTrue(VeyraTargeting::EdgeToEdgeDistance(*Companion, Body) <= CompanionFixture::Follow, TEXT("beside that body")));
		}

		TEST_METHOD(ItGrowsWithItsOwnerAndHoldsItsShareOfItsOwnersMagicPower)
		{
			using namespace CompanionFixture;
			AVeyraCompanion& Companion = *Pet();
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Stat(Companion, UVeyraVitalsSet::GetMaxHealthAttribute()), Health, Tolerance)));
			UVeyraProgressionComponent* Progression = Owner->GetPlayerState()->FindComponentByClass<UVeyraProgressionComponent>();
			Progression->Initialize(FVeyraStatGrowth(), 0.0);
			Progression->AddExperience(ManyLevels);
			const int32 Levels = Progression->GetLevel() - 1;
			ASSERT_THAT(IsTrue(Levels > 0));
			FVeyraEquipmentStats Items;
			Items.MagicPower = OwnerPower;
			ASSERT_THAT(IsTrue(VeyraCombat::SetEquipmentStats(OwnerAbilities(), Items)));
			const double OwnersPower = Stat(*Owner, UVeyraOffenceSet::GetMagicPowerAttribute());
			Keeper().Keep(OwnerAbilities());
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Stat(Companion, UVeyraVitalsSet::GetMaxHealthAttribute()), Health + HealthGrowth * Levels, Tolerance),
				TEXT("its Health grows with its owner's levels")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Stat(Companion, UVeyraOffenceSet::GetMagicPowerAttribute()), Power + PowerGrowth * Levels + Share * OwnersPower,
				Tolerance), TEXT("and its Magic Power holds its share of its owner's")));
		}

		TEST_METHOD(ItIsBanishedWithItsOwnerAndReformsAsItsOwnerRevives)
		{
			AVeyraCompanion& Companion = *Pet();
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(CompanionFixture::Far, 0.0, 0.0));
			int32 Banished = 0;
			const FDelegateHandle Handle = Keeper().OnBanished.AddLambda([&Banished](AVeyraCompanion&) { ++Banished; });
			ASSERT_THAT(IsTrue(Hit(Enemy, *Owner, CompanionFixture::Lethal)));
			ASSERT_THAT(IsTrue(Companion.IsBanished() && !Companion.IsAlive() && Companion.IsHidden(), TEXT("it leaves with its owner")));
			ASSERT_THAT(AreEqual(1, Banished));
			ASSERT_THAT(IsNull(Keeper().FindLiving(OwnerAbilities())));
			Keeper().Keep(OwnerAbilities());
			ASSERT_THAT(IsTrue(Companion.IsBanished(), TEXT("and stays gone while its owner is dead")));
			ASSERT_THAT(IsTrue(VeyraCombat::Revive(OwnerAbilities())));
			Keeper().Keep(OwnerAbilities());
			ASSERT_THAT(IsTrue(!Companion.IsBanished() && Companion.IsAlive() && !Companion.IsHidden(), TEXT("it returns as its owner does")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Stat(Companion, UVeyraVitalsSet::GetHealthAttribute()), Stat(Companion, UVeyraVitalsSet::GetMaxHealthAttribute()),
				CompanionFixture::Tolerance), TEXT("at full Health")));
			Keeper().OnBanished.Remove(Handle);
		}

		TEST_METHOD(KilledItReformsOnceItsTimeHasPassed)
		{
			AVeyraCompanion& Companion = *Pet();
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(CompanionFixture::Far, 0.0, 0.0));
			ASSERT_THAT(IsTrue(Hit(Enemy, Companion, CompanionFixture::Lethal)));
			ASSERT_THAT(IsTrue(Companion.IsBanished()));
			Wait(CompanionFixture::Reform / 2.0);
			Keeper().Keep(OwnerAbilities());
			ASSERT_THAT(IsTrue(Companion.IsBanished(), TEXT("not before its time")));
			Wait(CompanionFixture::Reform);
			Keeper().Keep(OwnerAbilities());
			ASSERT_THAT(IsTrue(Companion.IsAlive() && !Companion.IsBanished(), TEXT("then beside its owner")));
			ASSERT_THAT(IsTrue(VeyraTargeting::EdgeToEdgeDistance(Companion, *Owner) <= CompanionFixture::Follow));
		}

		TEST_METHOD(FollowingItFightsOnlyWhatItsOwnerFought)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraCompanion& Companion = *Pet();
			AVeyraTestFluxborn& Minion = World.SpawnFluxborn(EVeyraTeam::B, Companion.GetActorLocation() + FVector(CompanionFixture::Near, 0.0, 0.0));
			Brain().Think();
			ASSERT_THAT(IsNull(Brain().GetTarget(), TEXT("an enemy its owner left alone")));
			ASSERT_THAT(IsTrue(Hit(*Owner, Minion, CompanionFixture::Graze)));
			Brain().Think();
			ASSERT_THAT(IsTrue(Brain().GetTarget() == &Minion, TEXT("one its owner fought")));
			ASSERT_THAT(IsTrue(Companion.GetBasicAttack()->GetState().Phase == EVeyraAttackPhase::Windup, TEXT("it attacks it")));
		}

		TEST_METHOD(HoldingItFightsItsOwnersFoeThenAVanguardThenAnything)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraCompanion& Companion = *Pet();
			const FVector Point = Companion.GetActorLocation();
			AVeyraTestFluxborn& Minion = World.SpawnFluxborn(EVeyraTeam::B, Point + FVector(CompanionFixture::Near, 0.0, 0.0));
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, Point + FVector(0.0, CompanionFixture::Near * 2.0, 0.0));
			Companion.HoldAt(Point, Spawner.GetWorld().GetTimeSeconds() + CompanionFixture::Reform, FVeyraContentId());
			Brain().Think();
			ASSERT_THAT(IsTrue(Brain().GetTarget() == &Enemy, TEXT("a Vanguard before a Fluxborn")));
			ASSERT_THAT(IsTrue(Hit(*Owner, Minion, CompanionFixture::Graze)));
			Brain().Think();
			ASSERT_THAT(IsTrue(Brain().GetTarget() == &Minion, TEXT("its owner's foe first")));
			// The hold ends with its time.
			Wait(CompanionFixture::Reform);
			Brain().Think();
			ASSERT_THAT(IsTrue(Companion.GetMode() == EVeyraCompanionMode::Follow));
		}

		TEST_METHOD(TheRulesRankAndKeepTargets)
		{
			FArchetypeTestWorld World{ Spawner };
			const AActor& A = World.SpawnFluxborn(EVeyraTeam::B, FVector::ZeroVector);
			const AActor& B = World.SpawnFluxborn(EVeyraTeam::B, FVector::ZeroVector);
			const AActor& V = World.Spawn(EVeyraTeam::B, FVector::ZeroVector);
			const FVeyraCompanionCandidate Closest{ &A, false, false, 100.0, 2 };
			const FVeyraCompanionCandidate Further{ &B, false, false, 200.0, 1 };
			const FVeyraCompanionCandidate Champion{ &V, false, true, 300.0, 3 };
			const FVeyraCompanionCandidate Fought{ &B, true, false, 200.0, 1 };
			ASSERT_THAT(IsNull(VeyraCompanionRules::Choose(EVeyraCompanionMode::Follow, nullptr, { Closest, Champion }), TEXT("following, only its owner's foes")));
			ASSERT_THAT(IsTrue(VeyraCompanionRules::Choose(EVeyraCompanionMode::Follow, nullptr, { Closest, Fought }) == &B));
			ASSERT_THAT(IsTrue(VeyraCompanionRules::Choose(EVeyraCompanionMode::Hold, nullptr, { Closest, Further }) == &A, TEXT("the nearest")));
			ASSERT_THAT(IsTrue(VeyraCompanionRules::Choose(EVeyraCompanionMode::Hold, &B, { Closest, Further }) == &B, TEXT("keeping its target")));
			ASSERT_THAT(IsTrue(VeyraCompanionRules::Choose(EVeyraCompanionMode::Hold, &A, { Closest, Champion }) == &V, TEXT("a Vanguard outranks it")));
		}

		TEST_METHOD(ValidationKeepsTheLeashBeyondItsReach)
		{
			FVeyraAbilitiesTuning Bad = Tuning;
			FVeyraCompanionTuning& Broken = Bad.Companions.FindChecked(ArchetypeTestId(TEXT("test_pet")));
			Broken.LeashRange = Broken.FollowDistance;
			Broken.AcquireRange = Broken.BasicAttack.Range / 2.0;
			Broken.Stats.MaxResource = 1.0;
			const int32 Ranks[] = { 5, 3 };
			const TArray<FString> Problems = VeyraAbilityRules::Validate(Bad, Ranks);
			ASSERT_THAT(AreEqual(3, Problems.Num()));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER