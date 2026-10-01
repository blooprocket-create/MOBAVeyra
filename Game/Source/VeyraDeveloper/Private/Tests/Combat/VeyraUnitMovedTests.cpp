// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraCombatTests
{
	using VeyraAbilitiesTests::FArchetypeTestWorld;

	// Veyra.Combat.UnitMoved.*: a unit's own dash or blink, announced as it ends; never a displacement,
	// nor a dash one interrupts (ADR-032 §1).
	TEST_CLASS(UnitMoved, "Veyra.Combat")
	{
		// Fixture values: how far and how fast the moves go, a step of movement, and a bound on the steps.
		static constexpr double Far = 300.0;
		static constexpr double Speed = 1000.0;
		static constexpr float MoveStep = 0.05f;
		static constexpr int32 MaxSteps = 100;
		static constexpr double Tolerance = 1.0;

		FActorTestSpawner Spawner;
		AVeyraVanguardCharacter* Mover = nullptr;
		TArray<FVeyraUnitMovedEvent> Moves;

		BEFORE_EACH()
		{
			FArchetypeTestWorld World{ Spawner };
			Mover = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			UVeyraCombatEventSubsystem* Events = Spawner.GetWorld().GetSubsystem<UVeyraCombatEventSubsystem>();
			ASSERT_THAT(IsNotNull(Events));
			Events->OnUnitMoved.AddLambda([this](const FVeyraUnitMovedEvent& Moved) { Moves.Add(Moved); });
		}

		static FVeyraDash Forward()
		{
			FVeyraDash Dash;
			Dash.Direction = FVector::ForwardVector;
			Dash.Distance = Far;
			Dash.Speed = Speed;
			return Dash;
		}

		/** Moves the forced move under way on until it ends. */
		void Settle() const
		{
			UVeyraMovementComponent* Movement = Mover->GetVeyraMovement();
			for (int32 Steps = 0; (Movement->IsDashing() || Movement->IsDisplaced()) && Steps < MaxSteps; ++Steps)
			{
				Movement->AdvanceForcedMove(MoveStep);
			}
		}

		TEST_METHOD(ADashThatArrivesIsAnnouncedFromWhereItBeganToWhereItLanded)
		{
			const FVector Start = Mover->GetActorLocation();
			ASSERT_THAT(IsTrue(VeyraCombat::Dash(*Mover->GetAbilitySystemComponent(), Forward())));
			ASSERT_THAT(IsTrue(Moves.IsEmpty(), TEXT("not while it is under way")));
			Settle();
			ASSERT_THAT(AreEqual(1, Moves.Num()));
			ASSERT_THAT(IsTrue(Moves[0].Move == EVeyraOwnMove::Dash && Moves[0].Unit.Get() == Mover->GetAbilitySystemComponent()));
			ASSERT_THAT(IsTrue(FVector::Dist2D(Moves[0].From, Start) < Tolerance, TEXT("from where it began")));
			ASSERT_THAT(IsTrue(FVector::Dist2D(Moves[0].To, Start + FVector(Far, 0.0, 0.0)) < Tolerance, TEXT("to where it landed")));
		}

		TEST_METHOD(ABlinkIsAnnounced)
		{
			const FVector Destination = Mover->GetActorLocation() + FVector(Far, 0.0, 0.0);
			ASSERT_THAT(IsTrue(VeyraCombat::Blink(*Mover->GetAbilitySystemComponent(), Destination)));
			ASSERT_THAT(AreEqual(1, Moves.Num()));
			ASSERT_THAT(IsTrue(Moves[0].Move == EVeyraOwnMove::Blink));
			ASSERT_THAT(IsTrue(FVector::Dist2D(Moves[0].To, Destination) < Tolerance));
		}

		TEST_METHOD(ADisplacementIsNotTheUnitsOwnMoveNorIsTheDashItInterrupts)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(-Far, 0.0, 0.0));
			ASSERT_THAT(IsTrue(VeyraCombat::Dash(*Mover->GetAbilitySystemComponent(), Forward())));
			FVeyraDisplacement Push;
			Push.Direction = FVector::RightVector;
			Push.Distance = Far;
			Push.Speed = Speed;
			ASSERT_THAT(IsTrue(VeyraCombat::Displace(*Enemy.GetAbilitySystemComponent(), *Mover->GetAbilitySystemComponent(), Push)));
			Settle();
			ASSERT_THAT(IsFalse(Mover->GetVeyraMovement()->IsDisplaced(), TEXT("the push landed")));
			ASSERT_THAT(IsTrue(Moves.IsEmpty()));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
