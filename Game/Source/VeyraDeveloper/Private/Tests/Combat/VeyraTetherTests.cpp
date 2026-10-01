// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "Life/VeyraLifeComponent.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "Tethers/VeyraTetherSubsystem.h"
#include "TimerManager.h"
#include "Tuning/VeyraCombatTuningSubsystem.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraCombatTests
{
	using VeyraAbilitiesTests::FArchetypeTestWorld;

	// Veyra.Combat.Attachment.*: a unit holding on to another's body (ADR-018 §2). What it does as the
	// host moves runs in the body's movement, which Veyra.Net.ForcedMovement covers.
	TEST_CLASS(Attachment, "Veyra.Combat")
	{
		// Fixture values.
		static constexpr double Apart = 300.0;
		static constexpr double HoldSeconds = 60.0;
		static constexpr double Tolerance = 1.0;

		FActorTestSpawner Spawner;
		AVeyraVanguardCharacter* Holder = nullptr;
		AVeyraVanguardCharacter* Host = nullptr;
		TArray<FVeyraAttachEnd> Ends;

		BEFORE_EACH()
		{
			FArchetypeTestWorld World{ Spawner };
			Holder = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			Host = &World.Spawn(EVeyraTeam::B, FVector(Apart, 0.0, 0.0));
			Movement().OnAttachEnded.AddLambda([this](const FVeyraAttachEnd& End) { Ends.Add(End); });
		}

		UVeyraMovementComponent& Movement() const
		{
			return *Holder->GetVeyraMovement();
		}

		UAbilitySystemComponent& Abilities() const
		{
			return *Holder->GetAbilitySystemComponent();
		}

		TEST_METHOD(ItTakesItsSeatAtTheHostsBackAndMayCastButNotAttack)
		{
			ASSERT_THAT(IsTrue(VeyraCombat::Attach(Abilities(), *Host, HoldSeconds)));
			ASSERT_THAT(IsTrue(Movement().IsAttached() && VeyraCombat::GetAttachHost(Abilities()) == Host));
			// The host faces +X, so its back is toward -X.
			ASSERT_THAT(IsTrue(Holder->GetActorLocation().X < Host->GetActorLocation().X));
			ASSERT_THAT(IsTrue(VeyraTargeting::EdgeToEdgeDistance(*Holder, *Host) <= Tolerance,
				FString::Printf(TEXT("%g apart"), VeyraTargeting::EdgeToEdgeDistance(*Holder, *Host))));
			const EVeyraActionBlocks Blocks = VeyraCombat::GetActionBlocks(Abilities());
			ASSERT_THAT(IsTrue(EnumHasAllFlags(Blocks, EVeyraActionBlocks::Move | EVeyraActionBlocks::Attack)));
			ASSERT_THAT(IsFalse(EnumHasAnyFlags(Blocks, EVeyraActionBlocks::Cast), TEXT("it may still cast (ADR-018 §8)")));
			ASSERT_THAT(IsTrue(Holder->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Ignore));
		}

		TEST_METHOD(ADisplacementTakesOverAndDetachLetsGo)
		{
			const ECollisionResponse Before = Holder->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Pawn);
			ASSERT_THAT(IsTrue(VeyraCombat::Attach(Abilities(), *Host, HoldSeconds)));
			ASSERT_THAT(IsTrue(VeyraCombat::Displace(*Host->GetAbilitySystemComponent(), Abilities(), FVeyraDisplacement{ FVector::BackwardVector, Apart, Apart })));
			ASSERT_THAT(IsTrue(Ends.Num() == 1 && Ends[0].Reason == EVeyraAttachEndReason::Replaced && Ends[0].Host.Get() == Host));
			ASSERT_THAT(IsTrue(Movement().IsDisplaced() && !Movement().IsAttached()));

			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Other = World.Spawn(EVeyraTeam::A, FVector(0.0, Apart, 0.0));
			UAbilitySystemComponent& OtherAbilities = *Other.GetAbilitySystemComponent();
			ASSERT_THAT(IsTrue(VeyraCombat::Attach(OtherAbilities, *Host, HoldSeconds)));
			VeyraCombat::Detach(OtherAbilities);
			ASSERT_THAT(IsFalse(Other.GetVeyraMovement()->IsAttached()));
			ASSERT_THAT(IsTrue(Other.GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Pawn) == Before, TEXT("it blocks units again")));
		}

		TEST_METHOD(ItIsRefusedItsOwnBodyAndWhileStunned)
		{
			ASSERT_THAT(IsFalse(VeyraCombat::Attach(Abilities(), *Holder, HoldSeconds)));
			FVeyraStatusSpec Daze;
			Daze.Id = FVeyraContentId::FromText(TEXT("daze")).GetValue();
			Daze.Kind = EVeyraStatusKind::Stun;
			Daze.DurationSeconds = HoldSeconds;
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Host->GetAbilitySystemComponent(), Abilities(), Daze)));
			ASSERT_THAT(IsFalse(VeyraCombat::Attach(Abilities(), *Host, HoldSeconds)));
		}
	};

	// Veyra.Combat.TetherLinks.*: the tether ledger (Combat Bible §43): its snap, its time, its units'
	// lives and the statuses it holds.
	TEST_CLASS(TetherLinks, "Veyra.Combat")
	{
		// Fixture values.
		static constexpr double Near = 300.0;
		static constexpr double Reach = 600.0;
		static constexpr double Far = 1200.0;
		static constexpr double LongSeconds = 60.0;
		static constexpr double ShortSeconds = 0.5;
		static constexpr double Snap = 300.0;
		static constexpr double SnapSpeed = 1000.0;
		static constexpr float Step = 0.05f;

		FActorTestSpawner Spawner;
		AVeyraVanguardCharacter* Source = nullptr;
		AVeyraVanguardCharacter* Target = nullptr;
		UVeyraTetherSubsystem* Tethers = nullptr;
		TArray<FVeyraTetherEnd> Ends;

		BEFORE_EACH()
		{
			FArchetypeTestWorld World{ Spawner };
			Source = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			Target = &World.Spawn(EVeyraTeam::B, FVector(Near, 0.0, 0.0));
			Tethers = Spawner.GetWorld().GetSubsystem<UVeyraTetherSubsystem>();
			ASSERT_THAT(IsNotNull(Tethers));
			Tethers->OnTetherEnded.AddLambda([this](const FVeyraTetherEnd& End) { Ends.Add(End); });
		}

		static FVeyraTetherSpec Spec(double Seconds, double SnapDistance = 0.0)
		{
			FVeyraTetherSpec Tether;
			Tether.Id = FVeyraContentId::FromText(TEXT("test_tether")).GetValue();
			Tether.MaxRange = Reach;
			Tether.DurationSeconds = Seconds;
			Tether.SnapDistance = SnapDistance;
			Tether.SnapSpeed = SnapDistance > 0.0 ? SnapSpeed : 0.0;
			FVeyraStatusSpec& Mark = Tether.TargetStatuses.AddDefaulted_GetRef();
			Mark.Id = FVeyraContentId::FromText(TEXT("test_tethered")).GetValue();
			Mark.Kind = EVeyraStatusKind::Counter;
			Mark.DurationSeconds = 1.0;
			return Tether;
		}

		bool Tether(const FVeyraTetherSpec& With) const
		{
			return Tethers->Tether(*Source->GetAbilitySystemComponent(), *Target->GetAbilitySystemComponent(), With);
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

		double CheckSeconds() const
		{
			return UVeyraCombatTuningSubsystem::Get().Tethers.CheckSeconds;
		}

		TEST_METHOD(StretchedItSnapsItsTargetBackOnceAndEnds)
		{
			ASSERT_THAT(IsTrue(Tether(Spec(LongSeconds, Snap))));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Has(*Target, TEXT("test_tethered")), TEXT("it holds its status for as long as it lasts")));
			Wait(CheckSeconds() * 2.0);
			ASSERT_THAT(IsTrue(Ends.IsEmpty() && Tethers->IsTethered(*Source->GetAbilitySystemComponent(), Spec(LongSeconds).Id)));
			Target->SetActorLocation(FVector(Far, 0.0, Target->GetActorLocation().Z));
			Wait(CheckSeconds() * 2.0);
			ASSERT_THAT(IsTrue(Ends.Num() == 1 && Ends[0].Reason == EVeyraTetherEndReason::Stretched));
			const UVeyraMovementComponent& Pulled = *Target->GetVeyraMovement();
			ASSERT_THAT(IsTrue(Pulled.IsDisplaced() && Pulled.GetForcedMoveDestination().GetValue().X < Far, TEXT("pulled back toward its source")));
			ASSERT_THAT(IsFalse(FArchetypeTestWorld::Has(*Target, TEXT("test_tethered"))));
		}

		TEST_METHOD(AHostileTetherBreaksOnUntargetability)
		{
			ASSERT_THAT(IsTrue(Tether(Spec(LongSeconds))));
			FVeyraStatusSpec Vanish;
			Vanish.Id = FVeyraContentId::FromText(TEXT("test_untargetable")).GetValue();
			Vanish.Kind = EVeyraStatusKind::Untargetable;
			Vanish.DurationSeconds = LongSeconds;
			UAbilitySystemComponent& Held = *Target->GetAbilitySystemComponent();
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(Held, Held, Vanish)));
			Wait(CheckSeconds() * 2.0);
			ASSERT_THAT(IsTrue(Ends.Num() == 1 && Ends[0].Reason == EVeyraTetherEndReason::Untargetable, TEXT("Combat Bible §43")));
		}

		TEST_METHOD(ItEndsWithItsTimeAndWithADeath)
		{
			ASSERT_THAT(IsTrue(Tether(Spec(ShortSeconds))));
			Wait(ShortSeconds + CheckSeconds() * 2.0);
			ASSERT_THAT(IsTrue(Ends.Num() == 1 && Ends[0].Reason == EVeyraTetherEndReason::Expired));
			ASSERT_THAT(IsTrue(Tether(Spec(LongSeconds))));
			Source->GetPlayerState()->FindComponentByClass<UVeyraLifeComponent>()->SetState(EVeyraLifeState::Dead);
			Wait(CheckSeconds() * 2.0);
			ASSERT_THAT(IsTrue(Ends.Num() == 2 && Ends[1].Reason == EVeyraTetherEndReason::Died));
		}

		TEST_METHOD(ANewerTetherReplacesItsSourcesOlderAndItsSideSeesTheTarget)
		{
			ASSERT_THAT(IsTrue(Tether(Spec(LongSeconds))));
			ASSERT_THAT(IsTrue(Tethers->IsTetheredBy(*Target, EVeyraTeam::A) && !Tethers->IsTetheredBy(*Target, EVeyraTeam::B)));
			ASSERT_THAT(IsTrue(Tether(Spec(LongSeconds))));
			ASSERT_THAT(IsTrue(Ends.Num() == 1 && Ends[0].Reason == EVeyraTetherEndReason::Replaced, TEXT("the older lets go as the newer takes its place")));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Has(*Target, TEXT("test_tethered"))));
			ASSERT_THAT(IsFalse(Tethers->Tether(*Source->GetAbilitySystemComponent(), *Source->GetAbilitySystemComponent(), Spec(LongSeconds)), TEXT("not to itself")));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
