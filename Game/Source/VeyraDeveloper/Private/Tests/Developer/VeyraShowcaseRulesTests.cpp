// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER

#include "Components/ActorTestSpawner.h"
#include "Showcase/VeyraShowcaseRules.h"
#include "Statuses/VeyraStatusComponent.h"
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

		TEST_METHOD(AStatusTheLastClipLeftEnds)
		{
			// As a stance that plants its caster outlasts its clip and would keep it from stepping toward the next.
			VeyraAbilitiesTests::FArchetypeTestWorld World{ Spawner };
			UAbilitySystemComponent& Planted = *World.Spawn(EVeyraTeam::A, FVector::ZeroVector).GetAbilitySystemComponent();
			UVeyraStatusComponent* Statuses = Planted.GetOwner()->FindComponentByClass<UVeyraStatusComponent>();
			ASSERT_THAT(IsNotNull(Statuses));
			FVeyraStatusSpec Stance;
			Stance.Id = FVeyraContentId::FromText(TEXT("test_planted_stance")).GetValue();
			Stance.Kind = EVeyraStatusKind::Planted;
			Stance.DurationSeconds = HoldSeconds;
			ASSERT_THAT(IsTrue(Statuses->Apply(Planted, Stance)));
			ASSERT_THAT(IsFalse(Statuses->GetLedger().Entries.IsEmpty()));
			VeyraShowcase::ReleaseHolds(Planted);
			ASSERT_THAT(IsTrue(Statuses->GetLedger().Entries.IsEmpty(), TEXT("the stance ends, so the next clip starts from a free body")));
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
