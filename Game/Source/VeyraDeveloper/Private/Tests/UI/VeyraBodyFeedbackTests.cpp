// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER && WITH_VEYRA_UI

#include "Components/ActorTestSpawner.h"
#include "GameFramework/Character.h"
#include "Greybox/VeyraBodyFeedback.h"
#include "Greybox/VeyraGreyboxSettings.h"

namespace VeyraBodyFeedbackTests
{
	// Veyra.UI.BodyFeedback.*: a body answering its fight's moments (ADR-063 §2), from the committed grey-box settings.
	TEST_CLASS(BodyFeedback, "Veyra.UI")
	{
		// Fixture values: when the cue arrives, in real and server seconds, a windup's length, and where the target stands.
		static constexpr double Now = 50.0;
		static constexpr double ServerNow = 200.0;
		static constexpr double WindupSeconds = 0.4;
		static constexpr double Slack = 0.001;
		inline static const FVector TargetAt = FVector(300.0, 0.0, 0.0);

		FActorTestSpawner Spawner;

		static const UVeyraGreyboxSettings& Settings()
		{
			return *GetDefault<UVeyraGreyboxSettings>();
		}

		FVeyraCombatCue CueOf(EVeyraCombatCueKind Kind)
		{
			FVeyraCombatCue Cue;
			Cue.Kind = Kind;
			Cue.Unit = &Spawner.SpawnActor<ACharacter>();
			Cue.Target = &Spawner.SpawnActorAt<ACharacter>(TargetAt, FRotator::ZeroRotator);
			Cue.EndsAt = ServerNow + WindupSeconds;
			return Cue;
		}

		TEST_METHOD(AWindupLeansTowardItsTargetUntilItsEnd)
		{
			FVeyraBodyFeedbackState State;
			VeyraBodyFeedback::Note(State, CueOf(EVeyraCombatCueKind::AttackWindup), Now, ServerNow);
			const FVeyraBodyPose Start = VeyraBodyFeedback::PoseAt(State, Now, false, Settings());
			const FVeyraBodyPose Halfway = VeyraBodyFeedback::PoseAt(State, Now + WindupSeconds / 2.0, false, Settings());
			const FVeyraBodyPose End = VeyraBodyFeedback::PoseAt(State, Now + WindupSeconds, false, Settings());
			ASSERT_THAT(IsTrue(Start.Offset.IsNearlyZero(Slack)));
			ASSERT_THAT(IsNear(Halfway.Offset.X, Settings().LeanDistance / 2.0, Slack));
			ASSERT_THAT(IsNear(End.Offset.X, static_cast<double>(Settings().LeanDistance), Slack));
			ASSERT_THAT(IsTrue(FMath::IsNearlyZero(End.Offset.Y) && FMath::IsNearlyZero(End.Offset.Z), TEXT("toward the target, on the ground")));
			// A windup that ends with no commit, as a cancelled one, settles back.
			ASSERT_THAT(IsTrue(VeyraBodyFeedback::PoseAt(State, Now + WindupSeconds + Settings().SnapSeconds, false, Settings()).Offset.IsNearlyZero(Slack)));
		}

		TEST_METHOD(ACommitSnapsForwardAndBack)
		{
			FVeyraBodyFeedbackState State;
			VeyraBodyFeedback::Note(State, CueOf(EVeyraCombatCueKind::AttackCommit), Now, ServerNow);
			ASSERT_THAT(IsNear(VeyraBodyFeedback::PoseAt(State, Now, false, Settings()).Offset.X, static_cast<double>(Settings().SnapDistance), Slack));
			ASSERT_THAT(IsTrue(VeyraBodyFeedback::PoseAt(State, Now + Settings().SnapSeconds, false, Settings()).Offset.IsNearlyZero(Slack)));
		}

		TEST_METHOD(AHitFlashesAndSquashesThenFades)
		{
			FVeyraBodyFeedbackState State;
			VeyraBodyFeedback::Note(State, CueOf(EVeyraCombatCueKind::Hit), Now, ServerNow);
			ASSERT_THAT(IsNear(VeyraBodyFeedback::PoseAt(State, Now, false, Settings()).Flash, 1.0, Slack));
			ASSERT_THAT(IsNear(VeyraBodyFeedback::PoseAt(State, Now, true, Settings()).Flash, static_cast<double>(Settings().ReducedFlashStrength), Slack,
				TEXT("Reduce Flashing: weaker")));
			const FVeyraBodyPose Squashed = VeyraBodyFeedback::PoseAt(State, Now + Settings().RecoilSeconds / 2.0, false, Settings());
			ASSERT_THAT(IsNear(Squashed.HeightShare, 1.0 - Settings().RecoilSquash, Slack));
			const FVeyraBodyPose After = VeyraBodyFeedback::PoseAt(State, Now + FMath::Max(Settings().HitFlashSeconds, Settings().RecoilSeconds), false, Settings());
			ASSERT_THAT(IsTrue(After.Flash == 0.0 && After.HeightShare == 1.0));
		}

		TEST_METHOD(ADeathCollapsesUntilTheUnitLivesAgain)
		{
			FVeyraBodyFeedbackState State;
			VeyraBodyFeedback::Note(State, CueOf(EVeyraCombatCueKind::AttackWindup), Now, ServerNow);
			VeyraBodyFeedback::Note(State, CueOf(EVeyraCombatCueKind::Death), Now, ServerNow);
			const FVeyraBodyPose Fallen = VeyraBodyFeedback::PoseAt(State, Now + Settings().CollapseSeconds * 10.0, false, Settings());
			ASSERT_THAT(IsNear(Fallen.HeightShare, static_cast<double>(Settings().CollapsedHeightShare), Slack));
			ASSERT_THAT(IsTrue(Fallen.Offset.IsNearlyZero(), TEXT("the dead lean no more")));
			VeyraBodyFeedback::NoteAlive(State);
			ASSERT_THAT(IsNear(VeyraBodyFeedback::PoseAt(State, Now + Settings().CollapseSeconds * 10.0, false, Settings()).HeightShare, 1.0, Slack));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER && WITH_VEYRA_UI
