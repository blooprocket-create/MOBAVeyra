// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "AbilitySystemComponent.h"
#include "Attacks/VeyraBasicAttackComponent.h"
#include "Attributes/VeyraOffenceSet.h"
#include "Attributes/VeyraVitalsSet.h"
#include "Components/ActorTestSpawner.h"
#include "CQTest.h"
#include "Fluxborn/VeyraFluxborn.h"
#include "Fluxborn/VeyraFluxbornController.h"
#include "Rules/VeyraFluxbornRules.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "Tests/World/VeyraBattlegroundTestLayout.h"
#include "Tuning/VeyraWorldTuningSubsystem.h"
#include "VeyraBattlegroundSubsystem.h"
#include "VeyraCombatVerbs.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraWorldTests
{
	// Veyra.World.FluxbornRules.*: whom a Fluxborn attacks, and where it rejoins its lane
	// (Battleground Bible §19; ADR-011 §7).
	TEST_CLASS(FluxbornRules, "Veyra.World")
	{
		FActorTestSpawner Spawner;

		// Fixture distances, edge to edge.
		static constexpr double Near = 100.0;
		static constexpr double Far = 400.0;

		const AActor* Unit()
		{
			return &Spawner.SpawnActor<AActor>();
		}

		static FVeyraFluxbornCandidate Of(const AActor* Unit, EVeyraUnitKind Kind, double Distance, bool bInAttackRange = false, uint32 Id = 0)
		{
			return { Unit, Kind, Distance, bInAttackRange, Id };
		}

		TEST_METHOD(FluxbornComeBeforeStructuresAndVanguards)
		{
			const AActor* Minion = Unit();
			const AActor* Spire = Unit();
			const AActor* Champion = Unit();
			const TArray<FVeyraFluxbornCandidate> InRange = { Of(Champion, EVeyraUnitKind::Vanguard, Near), Of(Spire, EVeyraUnitKind::Structure, Near, true),
				Of(Minion, EVeyraUnitKind::Fluxborn, Far) };
			ASSERT_THAT(IsTrue(VeyraFluxbornRules::Choose(nullptr, false, nullptr, /*bSiege*/ false, InRange).Target == Minion));
			ASSERT_THAT(IsTrue(VeyraFluxbornRules::Choose(nullptr, false, nullptr, false, { InRange[0], InRange[1] }).Target == Spire, TEXT("a structure before a Vanguard")));
			ASSERT_THAT(IsTrue(VeyraFluxbornRules::Choose(nullptr, false, nullptr, false, { InRange[0] }).Target == Champion));
		}

		TEST_METHOD(ASiegeUnitTakesAStructureInAttackRangeFirst)
		{
			const AActor* Minion = Unit();
			const AActor* Spire = Unit();
			ASSERT_THAT(IsTrue(VeyraFluxbornRules::Choose(nullptr, false, nullptr, /*bSiege*/ true,
				{ Of(Minion, EVeyraUnitKind::Fluxborn, Near), Of(Spire, EVeyraUnitKind::Structure, Far, /*bInAttackRange*/ true) }).Target == Spire));
			ASSERT_THAT(IsTrue(VeyraFluxbornRules::Choose(nullptr, false, nullptr, true,
				{ Of(Minion, EVeyraUnitKind::Fluxborn, Near), Of(Spire, EVeyraUnitKind::Structure, Far, /*bInAttackRange*/ false) }).Target == Minion,
				TEXT("a structure out of attack range waits")));
		}

		TEST_METHOD(ItKeepsATargetUnlessABetterRankIsOnOffer)
		{
			const AActor* Current = Unit();
			const AActor* Nearer = Unit();
			const AActor* Champion = Unit();
			ASSERT_THAT(IsTrue(VeyraFluxbornRules::Choose(Current, false, nullptr, false,
				{ Of(Current, EVeyraUnitKind::Fluxborn, Far), Of(Nearer, EVeyraUnitKind::Fluxborn, Near) }).Target == Current));
			ASSERT_THAT(IsTrue(VeyraFluxbornRules::Choose(Champion, false, nullptr, false,
				{ Of(Champion, EVeyraUnitKind::Vanguard, Near), Of(Nearer, EVeyraUnitKind::Fluxborn, Far) }).Target == Nearer,
				TEXT("a Fluxborn arriving draws it from a Vanguard it attacked for want of one")));
		}

		TEST_METHOD(ItAnswersAggressionUntilTheAttackerLeaves)
		{
			const AActor* Minion = Unit();
			const AActor* Attacker = Unit();
			const FVeyraFluxbornChoice Answer = VeyraFluxbornRules::Choose(Minion, false, Attacker, false,
				{ Of(Minion, EVeyraUnitKind::Fluxborn, Near), Of(Attacker, EVeyraUnitKind::Vanguard, Far) });
			ASSERT_THAT(IsTrue(Answer.Target == Attacker && Answer.bResponding));
			const FVeyraFluxbornChoice Kept = VeyraFluxbornRules::Choose(Attacker, true, nullptr, false,
				{ Of(Minion, EVeyraUnitKind::Fluxborn, Near), Of(Attacker, EVeyraUnitKind::Vanguard, Far) });
			ASSERT_THAT(IsTrue(Kept.Target == Attacker && Kept.bResponding));
			const FVeyraFluxbornChoice Released = VeyraFluxbornRules::Choose(Attacker, true, nullptr, false, { Of(Minion, EVeyraUnitKind::Fluxborn, Near) });
			ASSERT_THAT(IsTrue(Released.Target == Minion && !Released.bResponding, TEXT("it returns to normal targeting")));
		}

		TEST_METHOD(EquallyNearUnitsResolveByTheirStableIds)
		{
			const AActor* Low = Unit();
			const AActor* High = Unit();
			ASSERT_THAT(IsTrue(VeyraFluxbornRules::Choose(nullptr, false, nullptr, false,
				{ Of(High, EVeyraUnitKind::Fluxborn, Near, false, 2), Of(Low, EVeyraUnitKind::Fluxborn, Near, false, 1) }).Target == Low));
		}

		TEST_METHOD(AfterAChaseItResumesAtTheWaypointAhead)
		{
			// Fixture lane: an L of two legs.
			const TArray<FVector2D> Lane = { { 0.0, 0.0 }, { 1000.0, 0.0 }, { 1000.0, 1000.0 } };
			ASSERT_THAT(AreEqual(1, VeyraFluxbornRules::ResumeWaypoint(Lane, { 400.0, -300.0 }), TEXT("beside the first leg")));
			ASSERT_THAT(AreEqual(2, VeyraFluxbornRules::ResumeWaypoint(Lane, { 1200.0, 600.0 }), TEXT("beside the second leg")));
			ASSERT_THAT(AreEqual(2, VeyraFluxbornRules::ResumeWaypoint(Lane, { 1300.0, -300.0 }), TEXT("past the corner, never back to it")));
			ASSERT_THAT(AreEqual(2, VeyraFluxbornRules::ResumeWaypoint(Lane, { 1000.0, 1500.0 }), TEXT("past the end")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraFluxbornRules::DistanceFromLane(Lane, { 400.0, -300.0 }), 300.0)));
		}
	};

	// Veyra.World.FluxbornUnits.*: the battleground spawns Fluxborn on a lane with their kind's stats,
	// Team Flux strengthens them live, they pick their targets, and the fallen are removed (ADR-011 §7, §10).
	TEST_CLASS(FluxbornUnits, "Veyra.World")
	{
		static constexpr double Tolerance = 1e-3;

		// Fixture values: a strength, and a hit to take some Health.
		static constexpr double Stronger = 1.1;
		static constexpr double Hit = 100.0;

		FActorTestSpawner Spawner;
		UVeyraBattlegroundSubsystem* Battleground = nullptr;

		BEFORE_EACH()
		{
			Battleground = Spawner.GetWorld().GetSubsystem<UVeyraBattlegroundSubsystem>();
			ASSERT_THAT(IsNotNull(Battleground));
			SpawnCompactGround(Spawner.GetWorld());
			Battleground->SpawnStructures(CompactBattleground());
		}

		static FVeyraContentId Kind(const TCHAR* Id)
		{
			return FVeyraContentId::FromText(Id).GetValue();
		}

		static double MaxHealthOf(const AVeyraFluxborn& Unit)
		{
			return Unit.GetAbilitySystemComponent()->GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute());
		}

		TEST_METHOD(ItSpawnsWithItsKindsStatsAndWalksTowardTheEnemyWell)
		{
			AVeyraFluxborn* Strider = Battleground->SpawnFluxborn(Kind(TEXT("strider")), EVeyraTeam::A, EVeyraLane::Mid);
			ASSERT_THAT(IsNotNull(Strider));
			const FVeyraFluxbornDefinition* Definition = UVeyraWorldTuningSubsystem::Get().FindFluxborn(Kind(TEXT("strider")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(MaxHealthOf(*Strider), Definition->Stats.MaxHealth, Tolerance)));
			ASSERT_THAT(IsTrue(Strider->GetBasicAttack()->HasProfile() && Strider->GetVeyraTeam() == EVeyraTeam::A));
			ASSERT_THAT(IsNotNull(Cast<AVeyraFluxbornController>(Strider->GetController()), TEXT("its server controller possesses it")));
			// The compact lane's two ends, then team B's Prime Well.
			const TArray<FVector2D>& Waypoints = Strider->GetWaypoints();
			ASSERT_THAT(AreEqual(3, Waypoints.Num()));
			ASSERT_THAT(IsTrue(Waypoints.Last().Equals(FVector2D(2400.0, 2400.0), Tolerance)));
			ASSERT_THAT(IsTrue(Battleground->GetFluxborn().Contains(Strider)));
		}

		TEST_METHOD(AnUnknownKindIsRefused)
		{
			TestRunner->AddExpectedMessagePlain(TEXT("Refused to spawn"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 2);
			ASSERT_THAT(IsNull(Battleground->SpawnFluxborn(Kind(TEXT("no_such_fluxborn")), EVeyraTeam::A, EVeyraLane::Mid)));
			ASSERT_THAT(IsNull(Battleground->SpawnFluxborn(Kind(TEXT("strider")), EVeyraTeam::A, EVeyraLane::Top), TEXT("the compact battleground has no top lane")));
		}

		TEST_METHOD(TeamFluxStrengthensItLiveKeepingItsHealthsShare)
		{
			AVeyraFluxborn* Strider = Battleground->SpawnFluxborn(Kind(TEXT("strider")), EVeyraTeam::A, EVeyraLane::Mid);
			UAbilitySystemComponent& Unit = *Strider->GetAbilitySystemComponent();
			const double Base = MaxHealthOf(*Strider);
			FVeyraRawDamageEvent Damage;
			Damage.Components.Add({ EVeyraDamageType::TrueDamage, Hit });
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(Unit, Unit, Damage)));
			const double Share = Unit.GetNumericAttribute(UVeyraVitalsSet::GetHealthAttribute()) / Base;

			FVeyraTeamFluxStrength Flux;
			Flux.HealthMultiplier = Stronger;
			Flux.DamageMultiplier = Stronger;
			Battleground->SetTeamFlux(EVeyraTeam::A, Flux);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(MaxHealthOf(*Strider), Base * Stronger, Tolerance)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Unit.GetNumericAttribute(UVeyraVitalsSet::GetHealthAttribute()) / MaxHealthOf(*Strider), Share, Tolerance)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Unit.GetNumericAttribute(UVeyraOffenceSet::GetOutgoingDamageMultiplierAttribute()), Stronger, Tolerance)));

			AVeyraFluxborn* Later = Battleground->SpawnFluxborn(Kind(TEXT("spark")), EVeyraTeam::A, EVeyraLane::Mid);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(MaxHealthOf(*Later), UVeyraWorldTuningSubsystem::Get().FindFluxborn(Kind(TEXT("spark")))->Stats.MaxHealth * Stronger,
				Tolerance), TEXT("a new one starts as strong")));
			AVeyraFluxborn* Enemy = Battleground->SpawnFluxborn(Kind(TEXT("strider")), EVeyraTeam::B, EVeyraLane::Mid);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(MaxHealthOf(*Enemy), Base, Tolerance), TEXT("the other team's are untouched")));
		}

		TEST_METHOD(ItAttacksTheNearestEnemyFluxbornInReach)
		{
			AVeyraFluxborn* Strider = Battleground->SpawnFluxborn(Kind(TEXT("strider")), EVeyraTeam::A, EVeyraLane::Mid);
			const double Reach = Strider->GetBasicAttack()->GetProfile().AcquisitionRadius;
			// Fixture placement: an enemy within its reach, and a Vanguard nearer still.
			VeyraAbilitiesTests::FArchetypeTestWorld World{ Spawner };
			const FVector Here = Strider->GetActorLocation();
			AVeyraVanguardCharacter& Champion = World.Spawn(EVeyraTeam::B, Here + FVector(Reach / 4.0, 0.0, 0.0));
			AVeyraFluxborn* Enemy = Battleground->SpawnFluxborn(Kind(TEXT("strider")), EVeyraTeam::B, EVeyraLane::Mid);
			Enemy->SetActorLocation(Here + FVector(0.0, Reach / 2.0, 0.0));
			AVeyraFluxbornController* Controller = Cast<AVeyraFluxbornController>(Strider->GetController());
			Controller->Think();
			ASSERT_THAT(IsTrue(Controller->GetTarget() == Enemy && Controller->GetTarget() != &Champion));
		}

		TEST_METHOD(AVanguardWhoHurtsAnAllyDrawsNearbyFluxborn)
		{
			AVeyraFluxborn* Strider = Battleground->SpawnFluxborn(Kind(TEXT("strider")), EVeyraTeam::A, EVeyraLane::Mid);
			const double Reach = Strider->GetBasicAttack()->GetProfile().AcquisitionRadius;
			VeyraAbilitiesTests::FArchetypeTestWorld World{ Spawner };
			const FVector Here = Strider->GetActorLocation();
			AVeyraVanguardCharacter& Ally = World.Spawn(EVeyraTeam::A, Here + FVector(Reach / 4.0, 0.0, 0.0));
			AVeyraVanguardCharacter& Attacker = World.Spawn(EVeyraTeam::B, Here + FVector(Reach / 2.0, 0.0, 0.0));
			// Beyond its attack range, so it closes rather than winds up an attack.
			AVeyraFluxborn* Enemy = Battleground->SpawnFluxborn(Kind(TEXT("strider")), EVeyraTeam::B, EVeyraLane::Mid);
			Enemy->SetActorLocation(Here + FVector(0.0, Reach / 2.0, 0.0));
			AVeyraFluxbornController* Controller = Cast<AVeyraFluxbornController>(Strider->GetController());
			Controller->Think();
			ASSERT_THAT(IsTrue(Controller->GetTarget() == Enemy && !Controller->IsResponding()));

			FVeyraRawDamageEvent Damage;
			Damage.Components.Add({ EVeyraDamageType::TrueDamage, Hit });
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*Attacker.GetAbilitySystemComponent(), *Ally.GetAbilitySystemComponent(), Damage)));
			Controller->Think();
			ASSERT_THAT(IsTrue(Controller->GetTarget() == &Attacker && Controller->IsResponding(), TEXT("it turns on the attacker")));
		}

		TEST_METHOD(AFallenFluxbornIsRemoved)
		{
			AVeyraFluxborn* Strider = Battleground->SpawnFluxborn(Kind(TEXT("strider")), EVeyraTeam::A, EVeyraLane::Mid);
			UAbilitySystemComponent& Unit = *Strider->GetAbilitySystemComponent();
			FVeyraRawDamageEvent Lethal;
			Lethal.Components.Add({ EVeyraDamageType::TrueDamage, MaxHealthOf(*Strider) });
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(Unit, Unit, Lethal) && !Strider->IsAlive()));
			ASSERT_THAT(IsFalse(Battleground->GetFluxborn().Contains(Strider)));
			ASSERT_THAT(IsNull(Strider->GetController(), TEXT("its controller is gone")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Strider->GetLifeSpan(), UVeyraWorldTuningSubsystem::Get().Fluxborn.CorpseSeconds, Tolerance)));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
