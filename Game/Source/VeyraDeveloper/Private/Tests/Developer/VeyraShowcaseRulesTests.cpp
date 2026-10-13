// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER

#include "Components/ActorTestSpawner.h"
#include "Showcase/VeyraShowcaseRules.h"
#include "Showcase/VeyraShowcaseSettings.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "VeyraCombatVerbs.h"

namespace VeyraShowcaseRulesTests
{
	// Veyra.Developer.ShowcaseRules.*: how the recorder reads the stage's reset and the server's refusals (ADR-074 §3).
	TEST_CLASS(ShowcaseRules, "Veyra.Developer")
	{
		TEST_METHOD(AResetIsReadOnlyFromItsOwnReply)
		{
			const FString Set = VeyraShowcase::ResetReply(7, /*bSet*/ true, TEXT("done"));
			const FString NotSet = VeyraShowcase::ResetReply(8, /*bSet*/ false, TEXT("rides on"));
			ASSERT_THAT(AreEqual(VeyraShowcase::ReadResetReply(Set, 7), VeyraShowcase::EResetAnswer::Set));
			ASSERT_THAT(AreEqual(VeyraShowcase::ReadResetReply(NotSet, 8), VeyraShowcase::EResetAnswer::NotSet,
				TEXT("a body that could not be stood at its mark leaves the stage unset")));
			ASSERT_THAT(AreEqual(VeyraShowcase::ReadResetReply(Set, 8), VeyraShowcase::EResetAnswer::None,
				TEXT("an earlier reset's reply does not answer this one")));
			ASSERT_THAT(AreEqual(VeyraShowcase::ReadResetReply(Set, 70), VeyraShowcase::EResetAnswer::None));
			ASSERT_THAT(AreEqual(VeyraShowcase::ReadResetReply(TEXT("Restored 0 Health."), 7), VeyraShowcase::EResetAnswer::None));
		}

		TEST_METHOD(ARefusedCastIsRetriedPrimedOrSkipped)
		{
			using VeyraShowcase::EAfterRefusal;
			ASSERT_THAT(AreEqual(VeyraShowcase::AfterRefusal(EVeyraCastRejection::CrowdControlled, false), EAfterRefusal::Retry));
			// A target the ability needs prepared, as one hurt a moment ago: the dummy is hurt, then the cast goes again.
			ASSERT_THAT(AreEqual(VeyraShowcase::AfterRefusal(EVeyraCastRejection::InvalidTarget, false), EAfterRefusal::Prime));
			ASSERT_THAT(AreEqual(VeyraShowcase::AfterRefusal(EVeyraCastRejection::InvalidTarget, true), EAfterRefusal::Skip,
				TEXT("prepared once and still refused, the ability is left out and the rest are recorded")));
			ASSERT_THAT(AreEqual(VeyraShowcase::AfterRefusal(EVeyraCastRejection::NotLearned, false), EAfterRefusal::Skip));
		}

		TEST_METHOD(TheMarksTurnAboutTheCentre)
		{
			// Fixture values: starts on the X axis, and a body's distance from the centre.
			const FVector StartA(-2000.0, 0.0, 0.0);
			const FVector StartB(2000.0, 0.0, 0.0);
			const double Stand = 300.0;
			const double Slack = 0.01;
			ASSERT_THAT(IsTrue(VeyraShowcase::MarkOf(FVector::ZeroVector, StartA, Stand, 0.0).Equals(FVector(-Stand, 0.0, 0.0), Slack),
				TEXT("unturned, a body stands on the line between the starts")));
			ASSERT_THAT(IsTrue(VeyraShowcase::MarkOf(FVector::ZeroVector, StartA, Stand, 90.0).Equals(FVector(0.0, -Stand, 0.0), Slack),
				TEXT("turned 90 degrees, the side starting at -X stands at -Y, the screen's left")));
			ASSERT_THAT(IsTrue(VeyraShowcase::MarkOf(FVector::ZeroVector, StartB, Stand, 90.0).Equals(FVector(0.0, Stand, 0.0), Slack)));
		}

		TEST_METHOD(TheRecordedVanguardStandsLeftOfTheDummy)
		{
			// The player fills side A, which starts at -X; the camera looks along +X, so the screen's left is -Y.
			const UVeyraShowcaseSettings& Settings = *GetDefault<UVeyraShowcaseSettings>();
			const FVector Caster = VeyraShowcase::MarkOf(FVector::ZeroVector, FVector(-1.0, 0.0, 0.0), Settings.StandDistanceFromCenter, Settings.MarkYawDegrees);
			const FVector Dummy = VeyraShowcase::MarkOf(FVector::ZeroVector, FVector(1.0, 0.0, 0.0), Settings.StandDistanceFromCenter, Settings.MarkYawDegrees);
			ASSERT_THAT(IsTrue(Caster.Y < Dummy.Y, TEXT("the recorded Vanguard on the left, the dummy on the right")));
		}
	};

	// Veyra.Developer.ShowcaseRelease.*: the stage's reset frees a body to stand at its mark (ADR-074 §2).
	TEST_CLASS(ShowcaseRelease, "Veyra.Developer")
	{
		// Fixture values: a ride, and how long a hold lasts.
		static constexpr double RideSpeed = 800.0;
		static constexpr double TurnRate = 90.0;
		static constexpr double HoldSeconds = 10.0;

		FActorTestSpawner Spawner;

		TEST_METHOD(ARideStillUnderWayEnds)
		{
			VeyraAbilitiesTests::FArchetypeTestWorld World{ Spawner };
			UAbilitySystemComponent& Rider = *World.Spawn(EVeyraTeam::A, FVector::ZeroVector).GetAbilitySystemComponent();
			ASSERT_THAT(IsTrue(VeyraCombat::StartRide(Rider, FVeyraRide{ RideSpeed, TurnRate, 0.0 })));
			VeyraShowcase::ReleaseHolds(Rider);
			ASSERT_THAT(IsFalse(VeyraCombat::IsRiding(Rider), TEXT("the ride ends, so the body can be stood at its mark")));
		}

		TEST_METHOD(ABodyHeldOnToAnotherLetsGo)
		{
			VeyraAbilitiesTests::FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Host = World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			UAbilitySystemComponent& Holder = *World.Spawn(EVeyraTeam::A, FVector(200.0, 0.0, 0.0)).GetAbilitySystemComponent();
			ASSERT_THAT(IsTrue(VeyraCombat::Attach(Holder, Host, HoldSeconds)));
			VeyraShowcase::ReleaseHolds(Holder);
			ASSERT_THAT(IsNull(VeyraCombat::GetAttachHost(Holder), TEXT("it lets go, so it can be stood at its mark")));
		}

		TEST_METHOD(AFreeBodyIsLeftAsItIs)
		{
			VeyraAbilitiesTests::FArchetypeTestWorld World{ Spawner };
			UAbilitySystemComponent& Free = *World.Spawn(EVeyraTeam::A, FVector::ZeroVector).GetAbilitySystemComponent();
			VeyraShowcase::ReleaseHolds(Free);
			ASSERT_THAT(IsFalse(VeyraCombat::IsRiding(Free)));
			ASSERT_THAT(IsNull(VeyraCombat::GetAttachHost(Free)));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
