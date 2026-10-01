// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Attacks/VeyraBasicAttackComponent.h"
#include "Companions/VeyraCompanion.h"
#include "Companions/VeyraCompanionController.h"
#include "Companions/VeyraCompanionRules.h"
#include "Companions/VeyraCompanionSubsystem.h"
#include "CQTest.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraAbilitiesTests
{
	namespace DeployableFixture
	{
		// Fixture values: the companion's reach is CompanionFixture::Reach, its acquire range CompanionFixture::Acquire.
		constexpr double Range = 1000.0;
		constexpr double Out = 500.0;
		constexpr double Lifetime = 10.0;
		constexpr double Mark = 5.0;
		constexpr double Close = 130.0;
		constexpr double Closer = 120.0;
		constexpr double Beyond = 350.0;
		constexpr double Graze = 50.0;
		constexpr double Lethal = 100000.0;
		constexpr double Tolerance = 1.0;
		constexpr float Step = 0.1f;
	}

	// Veyra.Abilities.Deployables.*: a companion deployed at a point, anchored, and the Designation it prefers (ADR-037 §1, §5).
	TEST_CLASS(Deployables, "Veyra.Abilities")
	{
		FActorTestSpawner Spawner;
		FVeyraAbilitiesTuning Tuning;
		AVeyraVanguardCharacter* Owner = nullptr;

		BEFORE_EACH()
		{
			using namespace DeployableFixture;
			Tuning.Companions.Add(ArchetypeTestId(TEXT("test_picket")), ExampleCompanion());
			FVeyraCommandAbilityTuning Set;
			Set.Cast = InstantCast(Range, 0.0, 0.0);
			Set.Order = EVeyraCompanionOrder::Deploy;
			Set.Companion = { ArchetypeTestId(TEXT("test_picket")) };
			Set.LifetimeSeconds = Lifetime;
			Tuning.Command.Add(ArchetypeTestId(TEXT("test_set")), Set);
			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_designated")), StatusOf(EVeyraStatusKind::Designated, 0.0, Mark));
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(&Tuning);
			const int32 Ranks[] = { 5, 3 };
			ASSERT_THAT(IsTrue(VeyraAbilityRules::Validate(Tuning, Ranks).IsEmpty(), FString::Join(VeyraAbilityRules::Validate(Tuning, Ranks), TEXT(" | "))));

			FArchetypeTestWorld World{ Spawner };
			Owner = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Learn(*Owner, EVeyraAbilitySlot::Q, ArchetypeTestId(TEXT("test_set")))));
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

		AVeyraCompanion* Picket()
		{
			return Keeper().Find(OwnerAbilities());
		}

		AVeyraCompanionController& Brain()
		{
			return *CastChecked<AVeyraCompanionController>(Picket()->GetController());
		}

		EVeyraCastRejection Deploy(const FVector& Point)
		{
			return FArchetypeTestWorld::CastAt(*Owner, EVeyraAbilitySlot::Q, Point);
		}

		void Wait(double Seconds)
		{
			UWorld& World = Spawner.GetWorld();
			const double Until = World.GetTimeSeconds() + Seconds;
			while (World.GetTimeSeconds() < Until)
			{
				World.Tick(LEVELTICK_TimeOnly, DeployableFixture::Step);
				++GFrameCounter;
				World.GetTimerManager().Tick(DeployableFixture::Step);
			}
		}

		static bool Hit(AActor& Source, AActor& Target, double Amount)
		{
			FVeyraRawDamageEvent Damage;
			Damage.Components.Add({ EVeyraDamageType::TrueDamage, Amount });
			return VeyraCombat::DealDamage(*UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Source),
				*UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Target), Damage);
		}

		static double HealthOf(const AActor& Unit)
		{
			return UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Unit)->GetNumericAttribute(UVeyraVitalsSet::GetHealthAttribute());
		}

		bool Designate(AActor& Source, AActor& Unit)
		{
			const TOptional<FVeyraStatusSpec> Mark = UVeyraAbilitiesTuningSubsystem::FindStatus(ArchetypeTestId(TEXT("test_designated")));
			return Mark.IsSet() && VeyraCombat::ApplyStatus(*UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Source),
				*UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Unit), Mark.GetValue());
		}

		TEST_METHOD(ADeploymentFormsItAnchoredAtThePointFacingAwayFromItsCaster)
		{
			using namespace DeployableFixture;
			ASSERT_THAT(IsTrue(Deploy(FVector(Out, 0.0, 0.0)) == EVeyraCastRejection::None));
			AVeyraCompanion* Formed = Picket();
			ASSERT_THAT(IsNotNull(Formed));
			ASSERT_THAT(IsTrue(Formed->GetMode() == EVeyraCompanionMode::Anchored));
			ASSERT_THAT(IsTrue(FVector::Dist2D(Formed->GetActorLocation(), FVector(Out, 0.0, 0.0)) <= Tolerance, TEXT("at the point")));
			ASSERT_THAT(IsTrue(Formed->GetActorForwardVector().X > 1.0 - KINDA_SMALL_NUMBER, TEXT("facing away from its caster")));
		}

		TEST_METHOD(ItNeverWalksAndFightsOnlyWhatItsAttackReaches)
		{
			using namespace DeployableFixture;
			ASSERT_THAT(IsTrue(Deploy(FVector(Out, 0.0, 0.0)) == EVeyraCastRejection::None));
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(Out + Beyond, 0.0, 0.0));
			const FVector Stood = Picket()->GetActorLocation();
			Brain().Think();
			Wait(1.0);
			ASSERT_THAT(IsNull(Brain().GetTarget(), TEXT("within its acquire range but past its reach: no fight")));
			ASSERT_THAT(IsTrue(FVector::Dist2D(Picket()->GetActorLocation(), Stood) <= Tolerance, TEXT("and no walk")));
			Enemy.SetActorLocation(FVector(Out + Close, 0.0, 0.0));
			Brain().Think();
			ASSERT_THAT(IsTrue(Brain().GetTarget() == &Enemy, TEXT("within its reach, it fights")));
		}

		TEST_METHOD(ItPrefersAnEnemyItsOwnerDesignated)
		{
			using namespace DeployableFixture;
			ASSERT_THAT(IsTrue(Deploy(FVector(Out, 0.0, 0.0)) == EVeyraCastRejection::None));
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Nearer = World.Spawn(EVeyraTeam::B, FVector(Out + Closer, 0.0, 0.0));
			AVeyraVanguardCharacter& Marked = World.Spawn(EVeyraTeam::B, FVector(Out, Close, 0.0));
			AVeyraVanguardCharacter& Ally = World.Spawn(EVeyraTeam::A, FVector(-Out, 0.0, 0.0));
			// It thinks again between attacks: a windup under way runs its course first.
			Brain().Think();
			ASSERT_THAT(IsTrue(Brain().GetTarget() == &Nearer, TEXT("the nearer first")));
			ASSERT_THAT(IsTrue(Designate(Ally, Marked)));
			Picket()->GetBasicAttack()->CancelAttack();
			Brain().Think();
			ASSERT_THAT(IsTrue(Brain().GetTarget() == &Nearer, TEXT("another's mark sways it not")));
			ASSERT_THAT(IsTrue(Designate(*Owner, Marked)));
			Picket()->GetBasicAttack()->CancelAttack();
			Brain().Think();
			ASSERT_THAT(IsTrue(Brain().GetTarget() == &Marked, TEXT("its owner's mark first")));
		}

		TEST_METHOD(CastAgainItMovesKeepingItsHealthAndItsTimeStartsAgain)
		{
			using namespace DeployableFixture;
			ASSERT_THAT(IsTrue(Deploy(FVector(Out, 0.0, 0.0)) == EVeyraCastRejection::None));
			AVeyraCompanion* First = Picket();
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(-Range, -Range, 0.0));
			ASSERT_THAT(IsTrue(Hit(Enemy, *First, Graze)));
			const double Left = HealthOf(*First);
			Wait(Lifetime / 2.0);
			ASSERT_THAT(IsTrue(Deploy(FVector(0.0, Out, 0.0)) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(Picket() == First, TEXT("the same one")));
			ASSERT_THAT(IsTrue(FVector::Dist2D(First->GetActorLocation(), FVector(0.0, Out, 0.0)) <= Tolerance, TEXT("at the new point")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(HealthOf(*First), Left), TEXT("its Health kept")));
			ASSERT_THAT(IsTrue(First->GetActorForwardVector().Y > 1.0 - KINDA_SMALL_NUMBER, TEXT("facing the new way")));
			Wait(Lifetime * 0.75);
			ASSERT_THAT(IsNotNull(Picket(), TEXT("its time started again")));
			Wait(Lifetime / 2.0);
			ASSERT_THAT(IsNull(Picket(), TEXT("and ran out")));
		}

		TEST_METHOD(ItStandsThroughItsOwnersDeathAndFights)
		{
			using namespace DeployableFixture;
			ASSERT_THAT(IsTrue(Deploy(FVector(Out, 0.0, 0.0)) == EVeyraCastRejection::None));
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(Out + Close, 0.0, 0.0));
			ASSERT_THAT(IsTrue(Hit(Enemy, *Owner, Lethal)));
			ASSERT_THAT(IsFalse(VeyraTargeting::IsAlive(Owner)));
			Keeper().Keep(OwnerAbilities());
			AVeyraCompanion* Standing = Picket();
			ASSERT_THAT(IsTrue(Standing && Standing->IsAlive() && !Standing->IsBanished(), TEXT("it stands")));
			Brain().Think();
			ASSERT_THAT(IsTrue(Brain().GetTarget() == &Enemy, TEXT("and fights")));
		}

		TEST_METHOD(DestroyedItIsGoneUntilTheNextCastWhichFormsItWhole)
		{
			using namespace DeployableFixture;
			ASSERT_THAT(IsTrue(Deploy(FVector(Out, 0.0, 0.0)) == EVeyraCastRejection::None));
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(-Range, -Range, 0.0));
			ASSERT_THAT(IsTrue(Hit(Enemy, *Picket(), Lethal)));
			ASSERT_THAT(IsNull(Picket(), TEXT("gone")));
			Wait(CompanionFixture::Reform * 2.0);
			ASSERT_THAT(IsNull(Picket(), TEXT("it never reforms")));
			ASSERT_THAT(IsTrue(Deploy(FVector(0.0, Out, 0.0)) == EVeyraCastRejection::None));
			ASSERT_THAT(IsNotNull(Picket()));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(HealthOf(*Picket()), CompanionFixture::Health), TEXT("whole")));
		}

		TEST_METHOD(AKeptCompanionRefusesADeployment)
		{
			ASSERT_THAT(IsTrue(Keeper().Summon(OwnerAbilities(), ArchetypeTestId(TEXT("test_picket")))));
			AVeyraCompanion* Kept = Picket();
			ASSERT_THAT(IsNotNull(Kept));
			ASSERT_THAT(IsFalse(Keeper().Deploy(OwnerAbilities(), ArchetypeTestId(TEXT("test_picket")), FVector(DeployableFixture::Out, 0.0, 0.0), FVector::ForwardVector,
				DeployableFixture::Lifetime)));
			ASSERT_THAT(IsTrue(Picket() == Kept && Kept->GetMode() == EVeyraCompanionMode::Follow));
		}

		TEST_METHOD(ItsRulesRankItsOwnersMarkFirst)
		{
			FArchetypeTestWorld World{ Spawner };
			const AActor& A = World.SpawnFluxborn(EVeyraTeam::B, FVector::ZeroVector);
			const AActor& B = World.SpawnFluxborn(EVeyraTeam::B, FVector::ZeroVector);
			const AActor& C = World.SpawnFluxborn(EVeyraTeam::B, FVector::ZeroVector);
			FVeyraCompanionCandidate Near{ &A, false, true, 100.0, 1 };
			FVeyraCompanionCandidate Fought{ &B, true, false, 200.0, 2 };
			FVeyraCompanionCandidate Marked{ &C, false, false, 300.0, 3 };
			Marked.bDesignated = true;
			const FVeyraCompanionCandidate All[] = { Near, Fought, Marked };
			ASSERT_THAT(IsTrue(VeyraCompanionRules::Choose(EVeyraCompanionMode::Anchored, nullptr, All) == &C));
			ASSERT_THAT(IsTrue(VeyraCompanionRules::Choose(EVeyraCompanionMode::Hold, nullptr, All) == &C));
			const FVeyraCompanionCandidate Unmarked[] = { Near, Fought };
			ASSERT_THAT(IsTrue(VeyraCompanionRules::Choose(EVeyraCompanionMode::Anchored, nullptr, Unmarked) == &B, TEXT("then its owner's foe")));
			const FVeyraCompanionCandidate Strangers[] = { Near };
			ASSERT_THAT(IsTrue(VeyraCompanionRules::Choose(EVeyraCompanionMode::Anchored, nullptr, Strangers) == &A, TEXT("then the nearest")));
			ASSERT_THAT(IsNull(VeyraCompanionRules::Choose(EVeyraCompanionMode::Follow, nullptr, Strangers), TEXT("following, never a stranger")));
		}

		TEST_METHOD(ADeploymentNamesOneCompanionAndALifetime)
		{
			FVeyraAbilitiesTuning Broken = Tuning;
			Broken.Command[ArchetypeTestId(TEXT("test_set"))].LifetimeSeconds = 0.0;
			const int32 Ranks[] = { 5, 3 };
			ASSERT_THAT(IsFalse(VeyraAbilityRules::Validate(Broken, Ranks).IsEmpty()));
			Broken = Tuning;
			Broken.Command[ArchetypeTestId(TEXT("test_set"))].BindTo = EVeyraCompanionBind::Ally;
			ASSERT_THAT(IsFalse(VeyraAbilityRules::Validate(Broken, Ranks).IsEmpty()));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
