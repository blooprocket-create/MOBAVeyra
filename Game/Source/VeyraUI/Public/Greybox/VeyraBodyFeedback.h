// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Cues/VeyraCombatCues.h"

class UVeyraGreyboxSettings;

/** What a unit's drawn body is doing, from the cues it raised (ADR-063 §2). Presentation only. */
struct FVeyraBodyFeedbackState
{
	/** A hit's flash and squash, from when it landed, in this machine's real seconds. */
	TOptional<double> HitAt;

	/** An attack's windup: the lean toward its target, from when it began until it ends. */
	TOptional<double> LeanFrom;
	double LeanUntil = 0.0;
	FVector LeanDirection = FVector::ZeroVector;

	/** An attack's commit: the snap toward its target. */
	TOptional<double> SnapAt;
	FVector SnapDirection = FVector::ZeroVector;

	/** The unit's death: the collapse, until it lives again. */
	TOptional<double> DiedAt;
};

/** How the body is drawn at a moment: moved, squashed and lit. */
struct FVeyraBodyPose
{
	/** Where the body is drawn from where it stands, in world space, on the ground. */
	FVector Offset = FVector::ZeroVector;

	/** The body's height as a share of its own: below 1 while it is squashed or collapsed. */
	double HeightShare = 1.0;

	/** The hit flash's strength, from 0 to 1. */
	double Flash = 0.0;
};

/** A unit's body answering its fight's moments (ADR-063 §2): leaning into a windup, snapping at a commit, flashing and squashing when hit, collapsing at death. */
namespace VeyraBodyFeedback
{
	/**
	 * Takes Cue into Unit's State at Now, in real seconds. A windup leans until ServerNow reaches the attack's end; a
	 * direction is toward the attack's target on the ground, or none.
	 */
	VEYRAUI_API void Note(FVeyraBodyFeedbackState& State, const FVeyraCombatCue& Cue, double Now, double ServerNow);

	/** Forgets the collapse of a unit that lives again. */
	VEYRAUI_API void NoteAlive(FVeyraBodyFeedbackState& State);

	/** The body's pose at Now. Under Reduce Flashing the flash is a weaker steady fade, never a sharp peak (Settings Bible §4.2). */
	VEYRAUI_API FVeyraBodyPose PoseAt(const FVeyraBodyFeedbackState& State, double Now, bool bReduceFlashing, const UVeyraGreyboxSettings& Settings);
}
