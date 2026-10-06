// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Cues/VeyraCombatCues.h"

/** A Vanguard body's animations (ADR-064 §2), by the names its generator gives them. */
enum class EVeyraVanguardClip : uint8
{
	Idle,
	Run,
	AttackWindup,
	AttackStrike,
	Cast,
	Hit,
	Death,
	Recall,
	None,
};

/** The clips a body has, as long as each plays at its own speed, in seconds; 0 for one it lacks. */
struct FVeyraVanguardClipLengths
{
	float Seconds[static_cast<int32>(EVeyraVanguardClip::None)] = {};

	float Of(EVeyraVanguardClip Clip) const { return Clip == EVeyraVanguardClip::None ? 0.0f : Seconds[static_cast<int32>(Clip)]; }
};

/** What a body's animation is fitted to, beside its clips (UVeyraVanguardArtSet, UVeyraGreyboxSettings). */
struct FVeyraVanguardAnimShape
{
	FVeyraVanguardClipLengths Lengths;

	/** How far one Run cycle carries the body, in units. */
	float RunStride = 0.0f;

	/** The share of Cast that raises the hands to the release, where a cast waiting to commit holds. */
	float CastReleaseShare = 0.0f;

	/** How long one animation takes to give way to another, in seconds. */
	float BlendSeconds = 0.0f;

	/** The ground speed at which Run has wholly taken over from Idle, in units a second. */
	float RunBlendSpeed = 0.0f;

	/** The slowest and fastest an animation plays to fit what it shows. */
	float MinPlayRate = 0.0f;
	float MaxPlayRate = 0.0f;
};

/** One animation playing over the body's locomotion. */
struct FVeyraVanguardAnimSlot
{
	EVeyraVanguardClip Clip = EVeyraVanguardClip::None;

	/** Where in the clip it is, and how fast it plays, in seconds of clip a second. */
	float Position = 0.0f;
	float Rate = 1.0f;

	/** How much it shows, from 0 to 1, and whether it is fading in or out. */
	float Weight = 0.0f;
	bool bFadingOut = false;

	/** Where it stops and waits, in seconds of clip: a cast holds at its release until it commits. Negative for nowhere. */
	float HoldAt = -1.0f;

	bool IsActive() const { return Clip != EVeyraVanguardClip::None && !bFadingOut; }
};

/** What a body is doing this frame, as the presentation sees it. */
struct FVeyraVanguardAnimInputs
{
	/** How fast its capsule moves over the ground, in units a second. */
	float GroundSpeed = 0.0f;

	bool bAlive = true;
	bool bRecalling = false;

	/** Whether its attack is still winding up: a windup held for its commit gives way once it is not. */
	bool bAttackWindingUp = false;

	/** While it winds up, how long until its attack commits, in seconds. */
	float AttackWindupSecondsLeft = 0.0f;

	/** Whether a cast holds it, winding up or channelling: the hands stay at the release while one does. */
	bool bCastHeld = false;
};

/**
 * Everything a body's animation holds between frames (ADR-064 §3): where its Idle and Run cycles are, how far Run
 * has taken over, and the animations over them, the newest last. Presentation only: nothing reads it back.
 */
struct FVeyraVanguardAnimState
{
	float IdlePosition = 0.0f;
	float RunPosition = 0.0f;
	float RunRate = 1.0f;
	float RunWeight = 0.0f;

	/** The animation fading out as the current one fades in, and the current one. */
	FVeyraVanguardAnimSlot Previous;
	FVeyraVanguardAnimSlot Current;
};

/** Choosing what a Vanguard body plays (ADR-064 §3). */
namespace VeyraVanguardAnim
{
	/**
	 * Starts what Cue shows on the body it is about: an attack's windup, timed to end when the attack commits
	 * (SecondsLeft before it does), then its strike; a cast's rise, held at its release, then the release; a hit, unless
	 * an attack or a cast is under way; a death. Other cues, and clips the body lacks, change nothing.
	 */
	VEYRAUI_API void NoteCue(FVeyraVanguardAnimState& State, EVeyraCombatCueKind Cue, float SecondsLeft, const FVeyraVanguardAnimShape& Shape);

	/**
	 * Moves the body's animation on by DeltaSeconds: Idle and Run by its ground speed, the current animation to its end
	 * and then fading out, Death held while it is dead, and Recall looping while it recalls.
	 */
	VEYRAUI_API void Advance(FVeyraVanguardAnimState& State, float DeltaSeconds, const FVeyraVanguardAnimInputs& Inputs, const FVeyraVanguardAnimShape& Shape);

	/** Whether Clip plays on the whole body even while it runs; the others play on its upper body then. */
	VEYRAUI_API bool IsWholeBody(EVeyraVanguardClip Clip);

	/** Whether Clip repeats until something ends it. */
	VEYRAUI_API bool Loops(EVeyraVanguardClip Clip);

	/** The clip's name, as its generator and the art set give it. */
	VEYRAUI_API FName NameOf(EVeyraVanguardClip Clip);
}
