// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Absorption/VeyraDamageAbsorptionComponent.h"
#include "Algo/Count.h"
#include "CQTest.h"
#include "Delivery/VeyraLingeringArea.h"
#include "EngineUtils.h"
#include "Passives/VeyraMistTrailPassive.h"
#include "Progression/VeyraProgressionTuningSubsystem.h"
#include "Rules/VeyraVisionRules.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "Tuning/VeyraVanguardsTuningSubsystem.h"
#include "VeyraCombatVerbs.h"
#include "VeyraVisionSubsystem.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraVanguardsTests
{
	using VeyraAbilitiesTests::ArchetypeTestId;
	using VeyraAbilitiesTests::FArchetypeTestWorld;

	/** Fixture values for a Mist Trail: a patch of fog, the trail's reach, and its follow shield. */
	namespace MistTrailFixture
	{
		constexpr double FogRadius = 400.0;
		constexpr double Spacing = 200.0;
		constexpr double ApproachLength = 400.0;
		constexpr double LaySeconds = 2.0;
		constexpr double LookSeconds = 0.25;
		constexpr double TrailRadius = 150.0;
		constexpr double Lasts = 3.0;
		constexpr double Pulse = 0.5;
		constexpr double Speed = 0.2;
		constexpr double Shield = 60.0;
		constexpr double Blow = 1000.0;
		constexpr double Tolerance = 1e-3;
	}

	// Veyra.Vanguards.MistTrail.*: a trail into the fog that speeds allies along it and shields one who follows
	// into the same fog (ADR-036 §5).
	TEST_CLASS(MistTrail, "Veyra.Vanguards")
	{
		FActorTestSpawner Spawner;
		FVeyraAbilitiesTuning Abilities;
		FVeyraVanguardsTuning Vanguards;
		AVeyraVanguardCharacter* Guide = nullptr;
		AVeyraVanguardCharacter* Follower = nullptr;
		UVeyraMistTrailPassive* Passive = nullptr;

		BEFORE_EACH()
		{
			using namespace MistTrailFixture;
			FVeyraAreaAbilityTuning Trail;
			Trail.Cast = VeyraAbilitiesTests::InstantCast(0.0, 0.0, 0.0);
			FVeyraAreaZoneTuning& Zone = Trail.Zones.AddDefaulted_GetRef();
			Zone.Shape = VeyraAbilitiesTests::CircleOf(TrailRadius);
			FVeyraLingerTuning& Linger = Trail.Linger.AddDefaulted_GetRef();
			Linger.DurationSeconds = Lasts;
			Linger.PulseSeconds = Pulse;
			Linger.AllyStatuses = { ArchetypeTestId(TEXT("test_trail_speed")), ArchetypeTestId(TEXT("test_followed")) };
			Abilities.Area.Add(ArchetypeTestId(TEXT("test_mist_trail")), Trail);
			Abilities.Statuses.Add(ArchetypeTestId(TEXT("test_trail_speed")), VeyraAbilitiesTests::StatusOf(EVeyraStatusKind::MoveSpeed, Speed, Lasts));
			Abilities.Statuses.Add(ArchetypeTestId(TEXT("test_followed")), VeyraAbilitiesTests::StatusOf(EVeyraStatusKind::Counter, 0.0, Lasts));
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(&Abilities);

			Vanguards = UVeyraVanguardsTuningSubsystem::Get();
			FVeyraMistTrailTuning Bell;
			Bell.Area = ArchetypeTestId(TEXT("test_mist_trail"));
			Bell.Spacing = Spacing;
			Bell.ApproachLength = ApproachLength;
			Bell.LaySeconds = LaySeconds;
			Bell.LookSeconds = LookSeconds;
			Bell.FollowStatus = ArchetypeTestId(TEXT("test_followed"));
			Bell.FollowShield.Amount = Shield;
			Bell.FollowShield.DurationSeconds = Lasts;
			Vanguards.MistTrail.Add(PassiveId(), Bell);
			UVeyraVanguardsTuningSubsystem::SetTestOverride(&Vanguards);

			FArchetypeTestWorld World{ Spawner };
			Guide = &World.Spawn(EVeyraTeam::A, FVector(-FogRadius * 2.5, 0.0, 0.0));
			Follower = &World.Spawn(EVeyraTeam::A, FVector(-FogRadius * 3.0, FogRadius * 3.0, 0.0));
			UVeyraVisionSubsystem* Vision = Spawner.GetWorld().GetSubsystem<UVeyraVisionSubsystem>();
			Vision->Start();
			Vision->SetDenseFog({ FVeyraFogCircle{ FVector2D::ZeroVector, FogRadius } });
			AVeyraPlayerState* Participant = Guide->GetPlayerState<AVeyraPlayerState>();
			Passive = NewObject<UVeyraMistTrailPassive>(Participant);
			Passive->Start(*Participant->GetAbilitySystemComponent(), PassiveId());
			Participant->SetPassive(Passive);
		}

		AFTER_EACH()
		{
			UVeyraVanguardsTuningSubsystem::SetTestOverride(nullptr);
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(nullptr);
		}

		static FVeyraContentId PassiveId()
		{
			return ArchetypeTestId(TEXT("test_follow_the_bell"));
		}

		/** The guide walks to X along the lane through the fog, and the passive looks. */
		void GuideTo(double X)
		{
			Guide->SetActorLocation(FVector(X, 0.0, Guide->GetActorLocation().Z), false, nullptr, ETeleportType::TeleportPhysics);
			Passive->Look();
		}

		void Put(AVeyraVanguardCharacter& Unit, const FVector2D& Where)
		{
			Unit.SetActorLocation(FVector(Where, Unit.GetActorLocation().Z), false, nullptr, ETeleportType::TeleportPhysics);
		}

		int32 CountTrail()
		{
			int32 Count = 0;
			for (TActorIterator<AVeyraLingeringArea> It(&Spawner.GetWorld()); It; ++It)
			{
				Count += It->GetAbility() == ArchetypeTestId(TEXT("test_mist_trail")) ? 1 : 0;
			}
			return Count;
		}

		static double ShieldOf(const AVeyraVanguardCharacter& Unit)
		{
			const UVeyraDamageAbsorptionComponent* Absorption = Unit.GetPlayerState()->FindComponentByClass<UVeyraDamageAbsorptionComponent>();
			double Sum = 0.0;
			for (const FVeyraShieldEntry& Each : Absorption ? Absorption->GetLedger().Shields : TArray<FVeyraShieldEntry>())
			{
				Sum += Each.Remaining;
			}
			return Sum;
		}

		TEST_METHOD(SteppingIntoFogLeavesATrailBackAlongTheWaySheCame)
		{
			using namespace MistTrailFixture;
			for (const double X : { -FogRadius * 2.5, -FogRadius * 2.0, -FogRadius * 1.5, -FogRadius * 1.25 })
			{
				GuideTo(X);
			}
			ASSERT_THAT(AreEqual(0, CountTrail(), TEXT("none outside the fog")));
			GuideTo(-FogRadius * 0.75);
			ASSERT_THAT(AreEqual(1, Passive->GetTrailCount()));
			ASSERT_THAT(AreEqual(static_cast<int32>(ApproachLength / Spacing) + 1, CountTrail(), TEXT("where she entered, and every spacing back as far as it reaches")));
			// It goes on with her inside the fog for a while.
			GuideTo(-FogRadius * 0.75 + Spacing);
			ASSERT_THAT(AreEqual(static_cast<int32>(ApproachLength / Spacing) + 2, CountTrail()));
		}

		TEST_METHOD(WalkingOutsideFogLeavesNothing)
		{
			using namespace MistTrailFixture;
			for (const double X : { -FogRadius * 2.5, -FogRadius * 2.0, -FogRadius * 1.5, -FogRadius * 2.0 })
			{
				GuideTo(X);
			}
			ASSERT_THAT(AreEqual(0, Passive->GetTrailCount()));
			ASSERT_THAT(AreEqual(0, CountTrail()));
		}

		TEST_METHOD(AnAllyWhoFollowsHerIntoTheFogIsShieldedOnce)
		{
			using namespace MistTrailFixture;
			// The follower waits on her way in, so the trail lands on it.
			Put(*Follower, FVector2D(-FogRadius * 0.75 - Spacing, 0.0));
			for (const double X : { -FogRadius * 2.0, -FogRadius * 1.5, -FogRadius * 1.25, -FogRadius * 0.75 })
			{
				GuideTo(X);
			}
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Has(*Follower, TEXT("test_followed")) && FArchetypeTestWorld::Has(*Follower, TEXT("test_trail_speed")),
				TEXT("the trail marks and speeds it")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyZero(ShieldOf(*Follower)), TEXT("no shield before the fog")));
			Put(*Follower, FVector2D(-FogRadius * 0.25, 0.0));
			Passive->Look();
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(ShieldOf(*Follower), Shield, Tolerance), TEXT("into the same fog: her shield")));

			// Spent, it does not come again for this trail.
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(FogRadius * 5.0, 0.0, 0.0));
			FVeyraRawDamageEvent Hit;
			Hit.Components.Add({ EVeyraDamageType::TrueDamage, Shield });
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*Enemy.GetAbilitySystemComponent(), *Follower->GetAbilitySystemComponent(), Hit)));
			Passive->Look();
			ASSERT_THAT(IsTrue(FMath::IsNearlyZero(ShieldOf(*Follower)), TEXT("once per trail")));
		}

		TEST_METHOD(AMarkedAllyInOtherFogGetsNothing)
		{
			using namespace MistTrailFixture;
			const FVector2D Elsewhere(FogRadius * 6.0, 0.0);
			Spawner.GetWorld().GetSubsystem<UVeyraVisionSubsystem>()->SetDenseFog(
				{ FVeyraFogCircle{ FVector2D::ZeroVector, FogRadius }, FVeyraFogCircle{ Elsewhere, FogRadius } });
			for (const double X : { -FogRadius * 2.0, -FogRadius * 0.75 })
			{
				GuideTo(X);
			}
			const TOptional<FVeyraStatusSpec> Followed = UVeyraAbilitiesTuningSubsystem::FindStatus(ArchetypeTestId(TEXT("test_followed")));
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Guide->GetAbilitySystemComponent(), *Follower->GetAbilitySystemComponent(), Followed.GetValue())));
			Put(*Follower, Elsewhere);
			Passive->Look();
			ASSERT_THAT(IsTrue(FMath::IsNearlyZero(ShieldOf(*Follower)), TEXT("not the fog she entered")));
		}

		TEST_METHOD(ValidationWantsATrailThatMarksItsFollowers)
		{
			FVeyraVanguardsTuning Bad;
			FVeyraMistTrailTuning Bell = Vanguards.MistTrail.FindChecked(PassiveId());
			Bell.FollowStatus = ArchetypeTestId(TEXT("test_trail_speed_never_given"));
			Bell.Spacing = 0.0;
			Bad.MistTrail.Add(PassiveId(), Bell);
			const TArray<FString> Problems = VeyraVanguardRules::Validate(Bad, Abilities, UVeyraProgressionTuningSubsystem::Get());
			const int32 Trail = Algo::CountIf(Problems, [](const FString& Problem) { return Problem.Contains(TEXT("/mistTrail/")); });
			ASSERT_THAT(AreEqual(3, Trail, FString::Join(Problems, TEXT(" | "))));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
