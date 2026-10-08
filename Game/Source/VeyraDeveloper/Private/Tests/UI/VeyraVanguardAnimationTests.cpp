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
		// A skill's own clip (ADR-072 §1): its length and the share of it that rises to its release.
		static constexpr float SkillSeconds = 1.2f;
		static constexpr float SkillReleaseShare = 0.5f;
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
			Shape.Skills.Add(SkillAbility(), { SkillSeconds, SkillReleaseShare });
			return Shape;
		}

		/** The fixture's ability with a clip of its own, and one without. */
		static FName SkillAbility() { return TEXT("a_skill"); }
		static FName PlainAbility() { return TEXT("a_plain_cast"); }

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

		TEST_METHOD(ABodyDrawnLargerStepsSlowerSoItsFeetKeepToTheGround)
		{
			// Drawn at twice its size, each Run cycle sweeps its feet twice as far, so it plays at half the rate (ADR-071 §1).
			constexpr float DrawScale = 2.0f;
			FVeyraVanguardAnimShape Drawn = Shape();
			Drawn.DrawScale = DrawScale;
			FVeyraVanguardAnimState State;
			FVeyraVanguardAnimInputs Moving;
			Moving.GroundSpeed = RunStride / RunSeconds;
			VeyraVanguardAnim::Advance(State, Step, Moving, Drawn);
			ASSERT_THAT(IsNear(State.RunRate, 1.0f / DrawScale, Slack, TEXT("one stride's speed at twice the size plays Run at half speed")));
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

		TEST_METHOD(ACastCancelledBeforeItsReleaseLowersWithoutReleasing)
		{
			FVeyraVanguardAnimInputs Holding;
			Holding.bCastHeld = true;
			const FVeyraVanguardAnimInputs Cancelled;
			const float Release = CastSeconds * CastReleaseShare;
			// Interrupted as its hands rise (a stun, say): seen held and now not, with no commit, it fades at once.
			FVeyraVanguardAnimState State;
			VeyraVanguardAnim::NoteCue(State, EVeyraCombatCueKind::CastWindup, 0.0f, Shape());
			Elapse(State, Release / 4.0f, Holding);
			VeyraVanguardAnim::Advance(State, Step, Cancelled, Shape());
			ASSERT_THAT(IsTrue(State.Current.bFadingOut, TEXT("a release that will not come is not shown")));
			// Interrupted at the release, waiting for its commit: the hands lower without playing it.
			FVeyraVanguardAnimState Waiting;
			VeyraVanguardAnim::NoteCue(Waiting, EVeyraCombatCueKind::CastWindup, 0.0f, Shape());
			Elapse(Waiting, CastSeconds, Holding);
			VeyraVanguardAnim::Advance(Waiting, Step, Cancelled, Shape());
			ASSERT_THAT(IsTrue(Waiting.Current.bFadingOut));
			Elapse(Waiting, BlendSeconds / 2.0f);
			ASSERT_THAT(IsTrue(Waiting.Current.Position <= Release + Slack, TEXT("never past the release")));
			// Its input a frame behind its cue cuts nothing short.
			FVeyraVanguardAnimState Cued;
			VeyraVanguardAnim::NoteCue(Cued, EVeyraCombatCueKind::CastWindup, 0.0f, Shape());
			VeyraVanguardAnim::Advance(Cued, Step, Cancelled, Shape());
			ASSERT_THAT(IsFalse(Cued.Current.bFadingOut, TEXT("the cast the cue began rises on")));
			// Its commit a frame behind its input still releases, from the release, as a cast with no windup does.
			VeyraVanguardAnim::NoteCue(Waiting, EVeyraCombatCueKind::CastCommit, 0.0f, Shape());
			ASSERT_THAT(IsTrue(Waiting.Current.IsActive() && Waiting.Current.Clip == EVeyraVanguardClip::Cast));
			ASSERT_THAT(IsNear(Waiting.Current.Position, Release, Slack));
		}

		TEST_METHOD(ASkillWithItsOwnClipRisesToItsReleaseAsItsWindupEnds)
		{
			// Its release lands on the commit however long the windup (ADR-072 §3): the rise plays at the rate that fits it.
			constexpr float WindupLeft = 0.3f;
			const float Release = SkillSeconds * SkillReleaseShare;
			FVeyraVanguardAnimState State;
			VeyraVanguardAnim::NoteCue(State, EVeyraCombatCueKind::CastWindup, WindupLeft, Shape(), SkillAbility());
			ASSERT_THAT(IsTrue(State.Current.Clip == EVeyraVanguardClip::Cast && State.Current.Skill == SkillAbility(), TEXT("it plays its own clip")));
			ASSERT_THAT(IsNear(State.Current.Rate, Release / WindupLeft, Slack));
			FVeyraVanguardAnimInputs Holding;
			Holding.bCastHeld = true;
			Holding.CastAbility = SkillAbility();
			Elapse(State, WindupLeft, Holding);
			ASSERT_THAT(IsNear(State.Current.Position, Release, Slack, TEXT("at the release as the windup ends")));
			Elapse(State, WindupLeft, Holding);
			ASSERT_THAT(IsNear(State.Current.Position, Release, Slack, TEXT("held there while the cast waits or channels")));
			// Committed, it follows through at its own pace to its own end, then gives way.
			VeyraVanguardAnim::NoteCue(State, EVeyraCombatCueKind::CastCommit, 0.0f, Shape(), SkillAbility());
			ASSERT_THAT(IsNear(State.Current.Rate, 1.0f, Slack));
			Elapse(State, SkillSeconds - Release - Step * 2.0f);
			ASSERT_THAT(IsTrue(State.Current.IsActive() && State.Current.Position > CastSeconds, TEXT("its own length, not Cast's")));
			Elapse(State, Step * 4.0f);
			ASSERT_THAT(IsTrue(State.Current.bFadingOut || State.Current.Clip == EVeyraVanguardClip::None));
		}

		TEST_METHOD(ASkillWithoutAClipOfItsOwnPlaysCast)
		{
			FVeyraVanguardAnimState State;
			VeyraVanguardAnim::NoteCue(State, EVeyraCombatCueKind::CastWindup, 0.3f, Shape(), PlainAbility());
			ASSERT_THAT(IsTrue(State.Current.Clip == EVeyraVanguardClip::Cast && State.Current.Skill.IsNone()));
			ASSERT_THAT(IsNear(State.Current.HoldAt, CastSeconds * CastReleaseShare, Slack));
			ASSERT_THAT(IsNear(State.Current.Rate, 1.0f, Slack, TEXT("Cast keeps its own pace")));
		}

		TEST_METHOD(ASkillWithNoWindupReleasesFromItsOwnRelease)
		{
			FVeyraVanguardAnimState State;
			VeyraVanguardAnim::NoteCue(State, EVeyraCombatCueKind::CastCommit, 0.0f, Shape(), SkillAbility());
			ASSERT_THAT(IsTrue(State.Current.Skill == SkillAbility()));
			ASSERT_THAT(IsNear(State.Current.Position, SkillSeconds * SkillReleaseShare, Slack));
			// A commit of another cast while one clip rises plays that cast's, not the rest of the first.
			FVeyraVanguardAnimState Switched;
			VeyraVanguardAnim::NoteCue(Switched, EVeyraCombatCueKind::CastWindup, 0.3f, Shape(), SkillAbility());
			VeyraVanguardAnim::NoteCue(Switched, EVeyraCombatCueKind::CastCommit, 0.0f, Shape(), PlainAbility());
			ASSERT_THAT(IsTrue(Switched.Current.Skill.IsNone() && Switched.Current.Clip == EVeyraVanguardClip::Cast));
		}

		TEST_METHOD(ABodyFirstSeenMidSkillTakesUpItsClip)
		{
			// Out of the fog mid-windup: no cue began it, so its inputs do, with the ability and the time left.
			FVeyraVanguardAnimState State;
			FVeyraVanguardAnimInputs Holding;
			Holding.bCastHeld = true;
			Holding.CastAbility = SkillAbility();
			Holding.CastWindupSecondsLeft = 0.2f;
			VeyraVanguardAnim::Advance(State, Step, Holding, Shape());
			ASSERT_THAT(IsTrue(State.Current.Skill == SkillAbility()));
			ASSERT_THAT(IsNear(State.Current.Rate, SkillSeconds * SkillReleaseShare / Holding.CastWindupSecondsLeft, Slack));
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
