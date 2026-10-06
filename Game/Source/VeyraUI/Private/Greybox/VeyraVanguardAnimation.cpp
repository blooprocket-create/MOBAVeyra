// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Greybox/VeyraVanguardAnimation.h"

namespace
{
	/** The rate at which a clip of ClipSeconds plays out in WithinSeconds, within the shape's limits. */
	float FitRate(float ClipSeconds, float WithinSeconds, const FVeyraVanguardAnimShape& Shape)
	{
		return WithinSeconds > KINDA_SMALL_NUMBER ? FMath::Clamp(ClipSeconds / WithinSeconds, Shape.MinPlayRate, Shape.MaxPlayRate) : Shape.MaxPlayRate;
	}

	/**
	 * Makes Clip the current animation. The one it replaces fades out as it fades in, unless bSeamless: then it takes
	 * the other's place and weight at once, as a strike does from the windup whose last pose is its first.
	 */
	void Play(FVeyraVanguardAnimState& State, EVeyraVanguardClip Clip, float Position, float Rate, float HoldAt, bool bSeamless)
	{
		FVeyraVanguardAnimSlot Next;
		Next.Clip = Clip;
		Next.Position = Position;
		Next.Rate = Rate;
		Next.HoldAt = HoldAt;
		if (bSeamless)
		{
			Next.Weight = State.Current.Weight;
		}
		else if (State.Current.Clip != EVeyraVanguardClip::None && State.Current.Weight >= State.Previous.Weight)
		{
			// The heavier of the two keeps fading out from where it is.
			State.Previous = State.Current;
			State.Previous.bFadingOut = true;
			State.Previous.HoldAt = -1.0f;
		}
		State.Current = Next;
	}

	/** Fades Weight toward its end over the blend: in, or out once the slot is fading out. False once it has faded out. */
	bool Fade(FVeyraVanguardAnimSlot& Slot, float DeltaSeconds, float BlendSeconds)
	{
		const float Step = BlendSeconds > KINDA_SMALL_NUMBER ? DeltaSeconds / BlendSeconds : 1.0f;
		Slot.Weight = FMath::Clamp(Slot.Weight + (Slot.bFadingOut ? -Step : Step), 0.0f, 1.0f);
		return !(Slot.bFadingOut && Slot.Weight <= 0.0f);
	}

	void AdvanceSlot(FVeyraVanguardAnimSlot& Slot, float DeltaSeconds, const FVeyraVanguardAnimShape& Shape)
	{
		if (Slot.Clip == EVeyraVanguardClip::None)
		{
			return;
		}
		if (!Fade(Slot, DeltaSeconds, Shape.BlendSeconds))
		{
			Slot = FVeyraVanguardAnimSlot();
			return;
		}
		const float Length = Shape.Lengths.Of(Slot.Clip);
		Slot.Position += DeltaSeconds * Slot.Rate;
		if (Slot.HoldAt >= 0.0f && Slot.Position >= Slot.HoldAt)
		{
			Slot.Position = Slot.HoldAt;
		}
		else if (VeyraVanguardAnim::Loops(Slot.Clip))
		{
			Slot.Position = Length > 0.0f ? FMath::Fmod(Slot.Position, Length) : 0.0f;
		}
		else if (Slot.Position >= Length)
		{
			// Played out: it rests on its last pose as it gives way.
			Slot.Position = Length;
			Slot.bFadingOut = true;
		}
	}

	bool Has(const FVeyraVanguardAnimShape& Shape, EVeyraVanguardClip Clip)
	{
		return Shape.Lengths.Of(Clip) > 0.0f;
	}
}

void VeyraVanguardAnim::NoteCue(FVeyraVanguardAnimState& State, EVeyraCombatCueKind Cue, float SecondsLeft, const FVeyraVanguardAnimShape& Shape)
{
	const bool bDead = State.Current.Clip == EVeyraVanguardClip::Death && State.Current.IsActive();
	switch (Cue)
	{
	case EVeyraCombatCueKind::AttackWindup:
		if (!bDead && Has(Shape, EVeyraVanguardClip::AttackWindup))
		{
			// Its last pose is the blow landing, so it ends as the attack commits, and waits there if the commit is late.
			const float Length = Shape.Lengths.Of(EVeyraVanguardClip::AttackWindup);
			Play(State, EVeyraVanguardClip::AttackWindup, 0.0f, FitRate(Length, SecondsLeft, Shape), Length, false);
		}
		break;
	case EVeyraCombatCueKind::AttackCommit:
		if (!bDead && Has(Shape, EVeyraVanguardClip::AttackStrike))
		{
			// Following through from the windup's last pose; from anywhere else it blends in.
			const float WindupLength = Shape.Lengths.Of(EVeyraVanguardClip::AttackWindup);
			const bool bFromImpact = State.Current.Clip == EVeyraVanguardClip::AttackWindup && State.Current.IsActive() &&
				State.Current.Position >= WindupLength - KINDA_SMALL_NUMBER;
			Play(State, EVeyraVanguardClip::AttackStrike, 0.0f, 1.0f, -1.0f, bFromImpact);
		}
		break;
	case EVeyraCombatCueKind::CastWindup:
		if (!bDead && Has(Shape, EVeyraVanguardClip::Cast))
		{
			Play(State, EVeyraVanguardClip::Cast, 0.0f, 1.0f, Shape.Lengths.Of(EVeyraVanguardClip::Cast) * Shape.CastReleaseShare, false);
		}
		break;
	case EVeyraCombatCueKind::CastCommit:
		if (bDead || !Has(Shape, EVeyraVanguardClip::Cast))
		{
			break;
		}
		if (State.Current.Clip == EVeyraVanguardClip::Cast && State.Current.IsActive())
		{
			// The windup's rise goes on through the release.
			State.Current.HoldAt = -1.0f;
		}
		else
		{
			// A cast with no windup (or whose windup gave way a frame before its commit came) releases at once.
			Play(State, EVeyraVanguardClip::Cast, Shape.Lengths.Of(EVeyraVanguardClip::Cast) * Shape.CastReleaseShare, 1.0f, -1.0f, false);
		}
		State.Current.bReleased = true;
		break;
	case EVeyraCombatCueKind::Hit:
	{
		// A flinch never cuts short what the body is doing, a recall included; the flash still shows the hit.
		const EVeyraVanguardClip Busy = State.Current.IsActive() ? State.Current.Clip : EVeyraVanguardClip::None;
		const bool bBusy = Busy == EVeyraVanguardClip::AttackWindup || Busy == EVeyraVanguardClip::AttackStrike || Busy == EVeyraVanguardClip::Cast ||
			Busy == EVeyraVanguardClip::Death || Busy == EVeyraVanguardClip::Recall;
		if (!bBusy && Has(Shape, EVeyraVanguardClip::Hit))
		{
			Play(State, EVeyraVanguardClip::Hit, 0.0f, 1.0f, -1.0f, false);
		}
		break;
	}
	case EVeyraCombatCueKind::Death:
		if (!bDead && Has(Shape, EVeyraVanguardClip::Death))
		{
			Play(State, EVeyraVanguardClip::Death, 0.0f, 1.0f, Shape.Lengths.Of(EVeyraVanguardClip::Death), false);
		}
		break;
	}
}

void VeyraVanguardAnim::Advance(FVeyraVanguardAnimState& State, float DeltaSeconds, const FVeyraVanguardAnimInputs& Inputs, const FVeyraVanguardAnimShape& Shape)
{
	// Life first: a body seen dead lies down, and one alive again stands at once where it lives again.
	const bool bShowsDeath = State.Current.Clip == EVeyraVanguardClip::Death;
	if (!Inputs.bAlive && !bShowsDeath)
	{
		NoteCue(State, EVeyraCombatCueKind::Death, 0.0f, Shape);
	}
	else if (Inputs.bAlive && bShowsDeath)
	{
		State.Previous = FVeyraVanguardAnimSlot();
		State.Current = FVeyraVanguardAnimSlot();
	}
	// Recall loops while it lasts. It takes over from an action as soon as it begins (the recall order cancelled the
	// windup or cast it shows), and otherwise waits for what is playing, a flinch, to end.
	const EVeyraVanguardClip Doing = State.Current.IsActive() ? State.Current.Clip : EVeyraVanguardClip::None;
	const bool bAction = Doing == EVeyraVanguardClip::AttackWindup || Doing == EVeyraVanguardClip::AttackStrike || Doing == EVeyraVanguardClip::Cast;
	if (Inputs.bAlive && Inputs.bRecalling && (Doing == EVeyraVanguardClip::None || bAction) && Has(Shape, EVeyraVanguardClip::Recall))
	{
		Play(State, EVeyraVanguardClip::Recall, 0.0f, 1.0f, -1.0f, false);
	}
	else if (!Inputs.bRecalling && State.Current.Clip == EVeyraVanguardClip::Recall)
	{
		State.Current.bFadingOut = true;
	}
	// A body first seen mid-windup or mid-cast (out of the fog, say) had no cue for it: it takes up what it is seen doing,
	// so the warning still shows, the windup timed to land as the attack commits. What a cue began, it leaves be.
	const bool bShowing = State.Current.IsActive();
	const EVeyraVanguardClip Showing = bShowing ? State.Current.Clip : EVeyraVanguardClip::None;
	const bool bAttacking = Showing == EVeyraVanguardClip::AttackWindup || Showing == EVeyraVanguardClip::AttackStrike;
	if (Inputs.bAlive && Inputs.bAttackWindingUp && !bAttacking && Showing != EVeyraVanguardClip::Death)
	{
		NoteCue(State, EVeyraCombatCueKind::AttackWindup, Inputs.AttackWindupSecondsLeft, Shape);
	}
	else if (Inputs.bAlive && Inputs.bCastHeld && Showing != EVeyraVanguardClip::Cast && Showing != EVeyraVanguardClip::Death && !bAttacking)
	{
		NoteCue(State, EVeyraCombatCueKind::CastWindup, 0.0f, Shape);
	}
	// A windup gives way once nothing holds it: an attack's that will not commit fades out (a cast's: below). An attack's
	// cancelled midway (a move order, a new target: its input seen and now gone while its blow is more than a blend away)
	// fades at once, so no strike shows that will not land. Its input a frame behind its cue (not yet seen), or its commit a
	// frame behind its input (within a blend of the blow), cuts nothing short: held at its end, it waits for that commit
	// and gives way only if none comes.
	FVeyraVanguardAnimSlot& Held = State.Current;
	const bool bAtHold = Held.HoldAt >= 0.0f && Held.Position >= Held.HoldAt;
	const bool bWindup = Held.IsActive() && Held.Clip == EVeyraVanguardClip::AttackWindup;
	if (bWindup && Inputs.bAttackWindingUp)
	{
		Held.bHoldSeen = true;
	}
	const float ToBlow = Held.Rate > 0.0f ? (Held.HoldAt - Held.Position) / Held.Rate : 0.0f;
	if (bWindup && !Inputs.bAttackWindingUp && (bAtHold || (Held.bHoldSeen && ToBlow > Shape.BlendSeconds)))
	{
		Held.bFadingOut = true;
	}
	else if (Held.IsActive() && Held.Clip == EVeyraVanguardClip::Cast)
	{
		// A cast's hands wait at the release while it winds up or channels. Committed, they play the release once nothing
		// holds them. Cancelled before its commit (an interrupt: its hold seen and now gone, or none come by the release),
		// they lower without it. Its input a frame behind its cue cuts nothing short.
		const float Release = Shape.Lengths.Of(EVeyraVanguardClip::Cast) * Shape.CastReleaseShare;
		if (Inputs.bCastHeld)
		{
			Held.bHoldSeen = true;
			if (Held.Position <= Release)
			{
				Held.HoldAt = Release;
			}
		}
		else if (Held.bReleased)
		{
			Held.HoldAt = -1.0f;
		}
		else if (Held.bHoldSeen || bAtHold)
		{
			Held.bFadingOut = true;
		}
	}

	// Idle and Run, Run taking over with speed and stepping at the speed it is carried.
	const float Speed = FMath::Max(0.0f, Inputs.GroundSpeed);
	const float RunTarget = Shape.RunBlendSpeed > 0.0f ? FMath::Clamp(Speed / Shape.RunBlendSpeed, 0.0f, 1.0f) : (Speed > 0.0f ? 1.0f : 0.0f);
	const float Step = Shape.BlendSeconds > KINDA_SMALL_NUMBER ? DeltaSeconds / Shape.BlendSeconds : 1.0f;
	State.RunWeight = RunTarget > State.RunWeight ? FMath::Min(RunTarget, State.RunWeight + Step) : FMath::Max(RunTarget, State.RunWeight - Step);
	const float IdleLength = Shape.Lengths.Of(EVeyraVanguardClip::Idle);
	const float RunLength = Shape.Lengths.Of(EVeyraVanguardClip::Run);
	State.RunRate = Shape.RunStride > 0.0f ? FMath::Clamp(Speed * RunLength / Shape.RunStride, Shape.MinPlayRate, Shape.MaxPlayRate) : 1.0f;
	State.IdlePosition = IdleLength > 0.0f ? FMath::Fmod(State.IdlePosition + DeltaSeconds, IdleLength) : 0.0f;
	State.RunPosition = RunLength > 0.0f ? FMath::Fmod(State.RunPosition + DeltaSeconds * State.RunRate, RunLength) : 0.0f;

	AdvanceSlot(State.Previous, DeltaSeconds, Shape);
	AdvanceSlot(State.Current, DeltaSeconds, Shape);
}

bool VeyraVanguardAnim::IsWholeBody(EVeyraVanguardClip Clip)
{
	return Clip == EVeyraVanguardClip::Death || Clip == EVeyraVanguardClip::Recall;
}

bool VeyraVanguardAnim::Loops(EVeyraVanguardClip Clip)
{
	return Clip == EVeyraVanguardClip::Idle || Clip == EVeyraVanguardClip::Run || Clip == EVeyraVanguardClip::Recall;
}

FName VeyraVanguardAnim::NameOf(EVeyraVanguardClip Clip)
{
	switch (Clip)
	{
	case EVeyraVanguardClip::Idle: return TEXT("Idle");
	case EVeyraVanguardClip::Run: return TEXT("Run");
	case EVeyraVanguardClip::AttackWindup: return TEXT("AttackWindup");
	case EVeyraVanguardClip::AttackStrike: return TEXT("AttackStrike");
	case EVeyraVanguardClip::Cast: return TEXT("Cast");
	case EVeyraVanguardClip::Hit: return TEXT("Hit");
	case EVeyraVanguardClip::Death: return TEXT("Death");
	case EVeyraVanguardClip::Recall: return TEXT("Recall");
	default: return NAME_None;
	}
}
