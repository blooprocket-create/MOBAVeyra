// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Greybox/VeyraBodyFeedback.h"

#include "GameFramework/Actor.h"
#include "Greybox/VeyraGreyboxSettings.h"

namespace VeyraBodyFeedback
{
namespace
{
	/** Toward Cue's target on the ground, from its unit; none without both. */
	FVector TowardTarget(const FVeyraCombatCue& Cue)
	{
		const AActor* Unit = Cue.Unit.Get();
		const AActor* Target = Cue.Target.Get();
		return Unit && Target ? (Target->GetActorLocation() - Unit->GetActorLocation()).GetSafeNormal2D() : FVector::ZeroVector;
	}

	/** How far through a span of Seconds that began at From the moment Now is, unset outside it. */
	TOptional<double> Through(double From, double Seconds, double Now)
	{
		const double Elapsed = Now - From;
		if (Elapsed < 0.0 || Seconds <= 0.0 || Elapsed >= Seconds)
		{
			return {};
		}
		return Elapsed / Seconds;
	}
}

void Note(FVeyraBodyFeedbackState& State, const FVeyraCombatCue& Cue, double Now, double ServerNow)
{
	switch (Cue.Kind)
	{
	case EVeyraCombatCueKind::Hit:
		State.HitAt = Now;
		break;
	case EVeyraCombatCueKind::Death:
		State.DiedAt = Now;
		State.LeanFrom.Reset();
		State.SnapAt.Reset();
		break;
	case EVeyraCombatCueKind::AttackWindup:
		State.LeanFrom = Now;
		State.LeanUntil = Now + FMath::Max(0.0, Cue.EndsAt - ServerNow);
		State.LeanDirection = TowardTarget(Cue);
		break;
	case EVeyraCombatCueKind::AttackCommit:
	{
		// Toward the target, or where the windup leaned when it is out of sight.
		const FVector Toward = TowardTarget(Cue);
		State.SnapDirection = Toward.IsZero() ? State.LeanDirection : Toward;
		State.SnapAt = Now;
		State.LeanFrom.Reset();
		break;
	}
	case EVeyraCombatCueKind::CastWindup:
	case EVeyraCombatCueKind::CastCommit:
		break;
	}
}

void NoteAlive(FVeyraBodyFeedbackState& State)
{
	State.DiedAt.Reset();
}

FVeyraBodyPose PoseAt(const FVeyraBodyFeedbackState& State, double Now, bool bReduceFlashing, const UVeyraGreyboxSettings& Settings)
{
	FVeyraBodyPose Pose;
	if (State.DiedAt)
	{
		// It sinks to its collapsed height and stays there while it is dead.
		const double Fallen = FMath::Clamp((Now - State.DiedAt.GetValue()) / FMath::Max(Settings.CollapseSeconds, UE_KINDA_SMALL_NUMBER), 0.0, 1.0);
		Pose.HeightShare = FMath::Lerp(1.0, static_cast<double>(Settings.CollapsedHeightShare), Fallen);
		return Pose;
	}
	if (State.LeanFrom)
	{
		const double From = State.LeanFrom.GetValue();
		double Leaned = 0.0;
		if (Now <= State.LeanUntil)
		{
			Leaned = State.LeanUntil > From ? (Now - From) / (State.LeanUntil - From) : 1.0;
		}
		else if (const TOptional<double> Settling = Through(State.LeanUntil, Settings.SnapSeconds, Now))
		{
			// A windup that ended with no commit, as a cancelled one, settles back.
			Leaned = 1.0 - Settling.GetValue();
		}
		Pose.Offset += State.LeanDirection * Settings.LeanDistance * Leaned;
	}
	if (const TOptional<double> Snapped = State.SnapAt ? Through(State.SnapAt.GetValue(), Settings.SnapSeconds, Now) : TOptional<double>())
	{
		// Out at once, then back, as a strike does.
		Pose.Offset += State.SnapDirection * Settings.SnapDistance * FMath::Square(1.0 - Snapped.GetValue());
	}
	if (State.HitAt)
	{
		if (const TOptional<double> Flashed = Through(State.HitAt.GetValue(), Settings.HitFlashSeconds, Now))
		{
			Pose.Flash = (1.0 - Flashed.GetValue()) * (bReduceFlashing ? Settings.ReducedFlashStrength : 1.0);
		}
		if (const TOptional<double> Squashed = Through(State.HitAt.GetValue(), Settings.RecoilSeconds, Now))
		{
			Pose.HeightShare = 1.0 - Settings.RecoilSquash * FMath::Sin(UE_DOUBLE_PI * Squashed.GetValue());
		}
	}
	return Pose;
}
}
