// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "AbilitySystemComponent.h"
#include "CQTest.h"
#include "Components/ActorTestSpawner.h"
#include "Rules/VeyraVisionRules.h"
#include "State/VeyraVisionTeamState.h"
#include "Statuses/VeyraStatusTypes.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "Tuning/VeyraVisionTuningSubsystem.h"
#include "VeyraCombatVerbs.h"
#include "VeyraPlayerState.h"
#include "VeyraVanguardCharacter.h"
#include "VeyraVisionSubsystem.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraVisionTests
{
	/** Fixture values for a Sounded enemy: its fog's size, and how long the mark lasts. */
	namespace SoundedFixture
	{
		constexpr double BushRadius = 300.0;
		constexpr double MarkSeconds = 5.0;
	}

	// Veyra.Vision.Sounded.*: a Sounded enemy in Dense Fog pings its presence to the side that sounded it,
	// and reveals nothing (ADR-036 §2).
	TEST_CLASS(Sounded, "Veyra.Vision")
	{
		FActorTestSpawner Spawner;
		AVeyraVanguardCharacter* Enemy = nullptr;
		AVeyraVanguardCharacter* Sounder = nullptr;
		AVeyraVanguardCharacter* EnemysAlly = nullptr;

		BEFORE_EACH()
		{
			VeyraAbilitiesTests::FArchetypeTestWorld World{ Spawner };
			Enemy = &World.Spawn(EVeyraTeam::B, FVector::ZeroVector);
			EnemysAlly = &World.Spawn(EVeyraTeam::B, FVector(Far(), 0.0, 0.0));
			Sounder = &World.Spawn(EVeyraTeam::A, FVector(0.0, Far(), 0.0));
			Vision().Start();
		}

		/** Further than anything in this world sees. */
		static double Far()
		{
			return 10.0 * UVeyraVisionTuningSubsystem::Get().Sight.Vanguard;
		}

		UVeyraVisionSubsystem& Vision()
		{
			return *Spawner.GetWorld().GetSubsystem<UVeyraVisionSubsystem>();
		}

		static FVeyraStatusSpec SoundedSpec()
		{
			FVeyraStatusSpec Spec;
			Spec.Id = VeyraAbilitiesTests::ArchetypeTestId(TEXT("test_sounded"));
			Spec.Kind = EVeyraStatusKind::Sounded;
			Spec.DurationSeconds = SoundedFixture::MarkSeconds;
			return Spec;
		}

		/** Source sounds Enemy. */
		bool Sound(const AVeyraVanguardCharacter& Source)
		{
			return VeyraCombat::ApplyStatus(*Source.GetAbilitySystemComponent(), *Enemy->GetAbilitySystemComponent(), SoundedSpec());
		}

		const AVeyraVisionTeamState& StateOf(EVeyraTeam Side)
		{
			return *AVeyraVisionTeamState::Find(&Spawner.GetWorld(), Side);
		}

		TEST_METHOD(ItMarksWithoutBlockingOrChangingAnything)
		{
			ASSERT_THAT(IsTrue(VeyraStatuses::Validate(SoundedSpec()).IsEmpty()));
			FVeyraStatusSpec Bad = SoundedSpec();
			Bad.Magnitude = 1.0;
			ASSERT_THAT(IsFalse(VeyraStatuses::Validate(Bad).IsEmpty(), TEXT("its magnitude is 0")));
			ASSERT_THAT(IsTrue(Sound(*Sounder)));
			ASSERT_THAT(IsTrue(VeyraCombat::GetActionBlocks(*Enemy->GetAbilitySystemComponent()) == EVeyraActionBlocks::None));
		}

		TEST_METHOD(InFogItPingsTheSideThatSoundedItAndRevealsNothing)
		{
			using namespace SoundedFixture;
			const FVector2D Bush(Enemy->GetActorLocation());
			Vision().SetDenseFog({ FVeyraFogCircle{ Bush, BushRadius } });
			ASSERT_THAT(IsTrue(Sound(*Sounder)));
			Vision().UpdateNow();
			ASSERT_THAT(AreEqual(StateOf(EVeyraTeam::A).GetPings().Num(), 1));
			ASSERT_THAT(IsTrue(StateOf(EVeyraTeam::A).GetPings()[0].Centre.Equals(Bush), TEXT("its fog's circle, never where it stands")));
			ASSERT_THAT(IsFalse(Vision().IsVisibleToTeam(EVeyraTeam::A, *Enemy), TEXT("it reveals nothing")));
			ASSERT_THAT(IsTrue(StateOf(EVeyraTeam::B).GetPings().IsEmpty(), TEXT("its own side hears nothing")));
		}

		TEST_METHOD(OutOfFogItPingsNothing)
		{
			ASSERT_THAT(IsTrue(Sound(*Sounder)));
			Vision().UpdateNow();
			ASSERT_THAT(IsTrue(StateOf(EVeyraTeam::A).GetPings().IsEmpty()));
		}

		TEST_METHOD(SoundedByItsOwnSideItPingsNoEnemy)
		{
			using namespace SoundedFixture;
			Vision().SetDenseFog({ FVeyraFogCircle{ FVector2D(Enemy->GetActorLocation()), BushRadius } });
			ASSERT_THAT(IsTrue(Sound(*EnemysAlly)));
			Vision().UpdateNow();
			ASSERT_THAT(IsTrue(StateOf(EVeyraTeam::A).GetPings().IsEmpty()));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
