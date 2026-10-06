// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER && WITH_VEYRA_UI

#include "Greybox/VeyraVanguardAnimation.h"

namespace VeyraVanguardAnimationTests
{
	// Veyra.UI.VanguardAnimation.*: what an animated Vanguard body plays, from its speed, life, recall and combat cues
	// (ADR-064 §3), with fixture clips and settings.
	TEST_CLASS(VanguardAnimation, "Veyra.UI")
	{
		// Fixture clips' lengths in seconds, how far a Run cycle carries the body, and the settings.
		static constexpr float IdleSeconds = 2.0f;
		static constexpr float RunSeconds = 0.6f;
		static constexpr float WindupSeconds = 0.4f;
		static constexpr float StrikeSeconds = 0.3f;
		static constexpr float CastSeconds = 0.5f;
		static constexpr float HitSeconds = 0.3f;
		static constexpr float DeathSeconds = 1.0f;
		static constexpr float RecallSeconds = 2.0f;
		static constexpr float RunStride = 180.0f;
		static constexpr float CastReleaseShare = 0.4f;
		static constexpr float BlendSeconds = 0.1f;
		static constexpr float RunBlendSpeed = 150.0f;
		static constexpr float MinPlayRate = 0.25f;
		static constexpr float MaxPlayRate = 4.0f;
		static constexpr float Step = 0.01f;
		static constexpr float Slack = 0.02f;

		static FVeyraVanguardAnimShape Shape()
		{
			FVeyraVanguardAnimShape Shape;
			const float Seconds[] = { IdleSeconds, RunSeconds, WindupSeconds, StrikeSeconds, CastSeconds, HitSeconds, DeathSeconds, RecallSeconds };
			for (int32 Index = 0; Index < UE_ARRAY_COUNT(Seconds); ++Index)
			{
				Shape.Lengths.Seconds[Index] = Seconds[Index];
			}
			Shape.RunStride = RunStride;
			Shape.CastReleaseShare = CastReleaseShare;
			Shape.BlendSeconds = BlendSeconds;
			Shape.RunBlendSpeed = RunBlendSpeed;
			Shape.MinPlayRate = MinPlayRate;
			Shape.MaxPlayRate = MaxPlayRate;
			return Shape;
		}

		/** Advances State by Seconds in small steps, as frames would. */
		static void Elapse(FVeyraVanguardAnimState& State, float Seconds, const FVeyraVanguardAnimInputs& Inputs = FVeyraVanguardAnimInputs())
		{
			for (float Done = 0.0f; Done < Seconds - KINDA_SMALL_NUMBER; Done += Step)
			{
				VeyraVanguardAnim::Advance(State, Step, Inputs, Shape());
			}
		}

		TEST_METHOD(RunTakesOverWithSpeedAndStepsAtTheSpeedItIsCarried)
		{
			FVeyraVanguardAnimState State;
			Elapse(State, BlendSeconds * 2.0f);
			ASSERT_THAT(IsNear(State.RunWeight, 0.0f, Slack, TEXT("standing still idles")));
			FVeyraVanguardAnimInputs Moving;
			Moving.GroundSpeed = RunStride / RunSeconds * 1.5f;
			Elapse(State, BlendSeconds * 2.0f, Moving);
			ASSERT_THAT(IsNear(State.RunWeight, 1.0f, Slack));
			ASSERT_THAT(IsNear(State.RunRate, 1.5f, Slack, TEXT("one and a half strides' speed plays Run half again as fast")));
			FVeyraVanguardAnimInputs Slow;
			Slow.GroundSpeed = RunBlendSpeed / 2.0f;
			Elapse(State, BlendSeconds * 2.0f, Slow);
			ASSERT_THAT(IsNear(State.RunWeight, 0.5f, Slack, TEXT("below the full speed, Idle and Run share the body")));
		}

		TEST_METHOD(AWindupEndsAsItsAttackCommitsAndTheStrikeFollowsThrough)
		{
			FVeyraVanguardAnimState State;
			const float SecondsLeft = WindupSeconds / 2.0f;
			VeyraVanguardAnim::NoteCue(State, EVeyraCombatCueKind::AttackWindup, SecondsLeft, Shape());
			ASSERT_THAT(IsTrue(State.Current.Clip == EVeyraVanguardClip::AttackWindup));
			ASSERT_THAT(IsNear(State.Current.Rate, 2.0f, Slack, TEXT("a windup half its clip's length plays it twice as fast")));
			FVeyraVanguardAnimInputs WindingUp;
			WindingUp.bAttackWindingUp = true;
			Elapse(State, SecondsLeft, WindingUp);
			ASSERT_THAT(IsNear(State.Current.Position, WindupSeconds, Slack, TEXT("its last pose, the blow landing, as the attack commits")));
			Elapse(State, SecondsLeft, WindingUp);
			ASSERT_THAT(IsNear(State.Current.Position, WindupSeconds, Slack, TEXT("a late commit finds it waiting there")));
			const float Weight = State.Current.Weight;
			VeyraVanguardAnim::NoteCue(State, EVeyraCombatCueKind::AttackCommit, 0.0f, Shape());
			ASSERT_THAT(IsTrue(State.Current.Clip == EVeyraVanguardClip::AttackStrike));
			ASSERT_THAT(IsNear(State.Current.Weight, Weight, Slack, TEXT("from the windup's last pose the strike takes over at once")));
			ASSERT_THAT(IsTrue(State.Previous.Clip == EVeyraVanguardClip::None));
			Elapse(State, StrikeSeconds + BlendSeconds * 2.0f);
			ASSERT_THAT(IsTrue(State.Current.Clip == EVeyraVanguardClip::None, TEXT("played out, it gives way to locomotion")));
		}

		TEST_METHOD(ABodyFirstSeenMidWindupOrCastTakesUpWhatItIsSeenDoing)
		{
			// Seen for the first time already winding up (out of the fog, say): no cue came, yet it winds up, timed to land
			// as the attack commits.
			FVeyraVanguardAnimState State;
			FVeyraVanguardAnimInputs WindingUp;
			WindingUp.bAttackWindingUp = true;
			WindingUp.AttackWindupSecondsLeft = WindupSeconds / 2.0f;
			VeyraVanguardAnim::Advance(State, Step, WindingUp, Shape());
			ASSERT_THAT(IsTrue(State.Current.Clip == EVeyraVanguardClip::AttackWindup, TEXT("the warning still shows")));
			ASSERT_THAT(IsNear(State.Current.Rate, 2.0f, Slack, TEXT("timed to the commit it will meet")));
			const float Position = State.Current.Position;
			VeyraVanguardAnim::Advance(State, Step, WindingUp, Shape());
			ASSERT_THAT(IsTrue(State.Current.Position > Position, TEXT("taken up once, then left to play")));
			// Seen mid-cast, its hands rise to the release and hold there while the cast does.
			FVeyraVanguardAnimState Casting;
			FVeyraVanguardAnimInputs Held;
			Held.bCastHeld = true;
			Elapse(Casting, CastSeconds, Held);
			ASSERT_THAT(IsTrue(Casting.Current.Clip == EVeyraVanguardClip::Cast));
			ASSERT_THAT(IsNear(Casting.Current.Position, CastSeconds * CastReleaseShare, Slack));
			// A dead body takes up neither.
			FVeyraVanguardAnimState Dead;
			FVeyraVanguardAnimInputs Gone = WindingUp;
			Gone.bAlive = false;
			VeyraVanguardAnim::Advance(Dead, Step, Gone, Shape());
			ASSERT_THAT(IsTrue(Dead.Current.Clip == EVeyraVanguardClip::Death));
		}

		TEST_METHOD(AWindupFitsItsTimeWithinItsPlayRates)
		{
			FVeyraVanguardAnimState State;
			VeyraVanguardAnim::NoteCue(State, EVeyraCombatCueKind::AttackWindup, WindupSeconds / (MaxPlayRate * 4.0f), Shape());
			ASSERT_THAT(IsNear(State.Current.Rate, MaxPlayRate, Slack));
			VeyraVanguardAnim::NoteCue(State, EVeyraCombatCueKind::AttackWindup, WindupSeconds / MinPlayRate * 4.0f, Shape());
			ASSERT_THAT(IsNear(State.Current.Rate, MinPlayRate, Slack));
		}

		TEST_METHOD(AWindupThatWillNotCommitGivesWay)
		{
			FVeyraVanguardAnimState State;
			VeyraVanguardAnim::NoteCue(State, EVeyraCombatCueKind::AttackWindup, WindupSeconds, Shape());
			Elapse(State, WindupSeconds + Step * 3.0f);
			ASSERT_THAT(IsTrue(State.Current.bFadingOut, TEXT("held at its end with no windup left, as a cancelled attack")));
		}

		TEST_METHOD(ACancelledWindupGivesWayBeforeItsBlowLands)
		{
			FVeyraVanguardAnimInputs WindingUp;
			WindingUp.bAttackWindingUp = true;
			const FVeyraVanguardAnimInputs Cancelled;
			// Cancelled a quarter of the way in (a move order, a new target): it fades at once, never reaching the blow.
			FVeyraVanguardAnimState State;
			VeyraVanguardAnim::NoteCue(State, EVeyraCombatCueKind::AttackWindup, WindupSeconds, Shape());
			Elapse(State, WindupSeconds / 4.0f, WindingUp);
			VeyraVanguardAnim::Advance(State, Step, Cancelled, Shape());
			ASSERT_THAT(IsTrue(State.Current.bFadingOut, TEXT("a strike that will not come is not shown")));
			// Its input a frame behind its cue cuts nothing short.
			FVeyraVanguardAnimState Cued;
			VeyraVanguardAnim::NoteCue(Cued, EVeyraCombatCueKind::AttackWindup, WindupSeconds, Shape());
			VeyraVanguardAnim::Advance(Cued, Step, Cancelled, Shape());
			ASSERT_THAT(IsFalse(Cued.Current.bFadingOut, TEXT("the windup the cue began plays on")));
			// Nor does its commit a frame behind its input, at the end of the windup: the strike still follows on.
			FVeyraVanguardAnimState Landing;
			VeyraVanguardAnim::NoteCue(Landing, EVeyraCombatCueKind::AttackWindup, WindupSeconds, Shape());
			Elapse(Landing, WindupSeconds - BlendSeconds / 2.0f, WindingUp);
			VeyraVanguardAnim::Advance(Landing, Step, Cancelled, Shape());
			ASSERT_THAT(IsFalse(Landing.Current.bFadingOut, TEXT("within a blend of its blow, it waits for the commit")));
			VeyraVanguardAnim::NoteCue(Landing, EVeyraCombatCueKind::AttackCommit, 0.0f, Shape());
			ASSERT_THAT(IsTrue(Landing.Current.Clip == EVeyraVanguardClip::AttackStrike));
		}

		TEST_METHOD(AHitNeverCutsAnAttackOrACastShort)
		{
			FVeyraVanguardAnimState State;
			VeyraVanguardAnim::NoteCue(State, EVeyraCombatCueKind::AttackWindup, WindupSeconds, Shape());
			VeyraVanguardAnim::NoteCue(State, EVeyraCombatCueKind::Hit, 0.0f, Shape());
			ASSERT_THAT(IsTrue(State.Current.Clip == EVeyraVanguardClip::AttackWindup));
			VeyraVanguardAnim::NoteCue(State, EVeyraCombatCueKind::CastWindup, 0.0f, Shape());
			VeyraVanguardAnim::NoteCue(State, EVeyraCombatCueKind::Hit, 0.0f, Shape());
			ASSERT_THAT(IsTrue(State.Current.Clip == EVeyraVanguardClip::Cast));
			FVeyraVanguardAnimState Idle;
			VeyraVanguardAnim::NoteCue(Idle, EVeyraCombatCueKind::Hit, 0.0f, Shape());
			ASSERT_THAT(IsTrue(Idle.Current.Clip == EVeyraVanguardClip::Hit, TEXT("a body doing nothing flinches")));
		}

		TEST_METHOD(ACastHoldsItsReleaseUntilItCommitsAndThroughAChannel)
		{
			FVeyraVanguardAnimState State;
			FVeyraVanguardAnimInputs Holding;
			Holding.bCastHeld = true;
			VeyraVanguardAnim::NoteCue(State, EVeyraCombatCueKind::CastWindup, 0.0f, Shape());
			Elapse(State, CastSeconds, Holding);
			const float Release = CastSeconds * CastReleaseShare;
			ASSERT_THAT(IsNear(State.Current.Position, Release, Slack, TEXT("winding up, the hands wait at the release")));
			VeyraVanguardAnim::NoteCue(State, EVeyraCombatCueKind::CastCommit, 0.0f, Shape());
			Elapse(State, CastSeconds, Holding);
			ASSERT_THAT(IsNear(State.Current.Position, Release, Slack, TEXT("channelling, they stay there")));
			Elapse(State, Step * 2.0f);
			ASSERT_THAT(IsTrue(State.Current.Position > Release, TEXT("once nothing holds the cast, they lower")));
		}

		TEST_METHOD(ACastWithNoWindupReleasesAtOnce)
		{
			FVeyraVanguardAnimState State;
			VeyraVanguardAnim::NoteCue(State, EVeyraCombatCueKind::CastCommit, 0.0f, Shape());
			ASSERT_THAT(IsTrue(State.Current.Clip == EVeyraVanguardClip::Cast));
			ASSERT_THAT(IsNear(State.Current.Position, CastSeconds * CastReleaseShare, Slack));
		}

		TEST_METHOD(ADeathLiesStillUntilItsBodyLivesAgain)
		{
			FVeyraVanguardAnimState State;
			FVeyraVanguardAnimInputs Dead;
			Dead.bAlive = false;
			Elapse(State, DeathSeconds * 2.0f, Dead);
			ASSERT_THAT(IsTrue(State.Current.Clip == EVeyraVanguardClip::Death, TEXT("a body seen dead lies down, cue or not")));
			ASSERT_THAT(IsNear(State.Current.Position, DeathSeconds, Slack));
			ASSERT_THAT(IsNear(State.Current.Weight, 1.0f, Slack));
			VeyraVanguardAnim::NoteCue(State, EVeyraCombatCueKind::AttackWindup, WindupSeconds, Shape());
			ASSERT_THAT(IsTrue(State.Current.Clip == EVeyraVanguardClip::Death, TEXT("nothing else plays on the dead")));
			Elapse(State, Step);
			ASSERT_THAT(IsTrue(State.Current.Clip == EVeyraVanguardClip::None, TEXT("alive again, it stands at once")));
		}

		TEST_METHOD(RecallLoopsWhileItLasts)
		{
			FVeyraVanguardAnimState State;
			FVeyraVanguardAnimInputs Recalling;
			Recalling.bRecalling = true;
			Elapse(State, RecallSeconds * 1.5f, Recalling);
			ASSERT_THAT(IsTrue(State.Current.Clip == EVeyraVanguardClip::Recall && State.Current.IsActive()));
			ASSERT_THAT(IsTrue(State.Current.Position < RecallSeconds, TEXT("it loops")));
			Elapse(State, BlendSeconds * 2.0f);
			ASSERT_THAT(IsTrue(State.Current.Clip == EVeyraVanguardClip::None, TEXT("it fades once the recall ends")));
		}

		TEST_METHOD(RecallTakesOverAnActionAsItBeginsAndAFlinchLeavesItBe)
		{
			FVeyraVanguardAnimInputs Recalling;
			Recalling.bRecalling = true;
			// The recall order cancels a windup under way: the body shows the recall at once, not the rest of the windup.
			FVeyraVanguardAnimState State;
			VeyraVanguardAnim::NoteCue(State, EVeyraCombatCueKind::AttackWindup, WindupSeconds, Shape());
			VeyraVanguardAnim::Advance(State, Step, Recalling, Shape());
			ASSERT_THAT(IsTrue(State.Current.Clip == EVeyraVanguardClip::Recall, TEXT("the recall shows as soon as it begins")));
			VeyraVanguardAnim::NoteCue(State, EVeyraCombatCueKind::Hit, 0.0f, Shape());
			ASSERT_THAT(IsTrue(State.Current.Clip == EVeyraVanguardClip::Recall, TEXT("a flinch never cuts it short")));
			// And a cast's raised hands.
			FVeyraVanguardAnimState Casting;
			VeyraVanguardAnim::NoteCue(Casting, EVeyraCombatCueKind::CastWindup, 0.0f, Shape());
			VeyraVanguardAnim::Advance(Casting, Step, Recalling, Shape());
			ASSERT_THAT(IsTrue(Casting.Current.Clip == EVeyraVanguardClip::Recall));
		}

		TEST_METHOD(OnlyDeathAndRecallTakeTheLegsFromARun)
		{
			ASSERT_THAT(IsTrue(VeyraVanguardAnim::IsWholeBody(EVeyraVanguardClip::Death) && VeyraVanguardAnim::IsWholeBody(EVeyraVanguardClip::Recall)));
			for (const EVeyraVanguardClip Clip : { EVeyraVanguardClip::AttackWindup, EVeyraVanguardClip::AttackStrike, EVeyraVanguardClip::Cast, EVeyraVanguardClip::Hit })
			{
				ASSERT_THAT(IsFalse(VeyraVanguardAnim::IsWholeBody(Clip)));
			}
		}
	};
}

#endif
