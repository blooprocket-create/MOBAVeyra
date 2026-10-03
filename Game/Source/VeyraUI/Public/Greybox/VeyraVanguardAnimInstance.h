// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Animation/AnimInstance.h"
#include "Greybox/VeyraVanguardAnimation.h"

#include "VeyraVanguardAnimInstance.generated.h"

class UAnimSequence;
struct FVeyraVanguardArt;

/**
 * Plays a Vanguard body's generated animations (ADR-064 §3) with no Animation Blueprint: Idle and Run by its ground
 * speed, and over them what the combat cues show, chosen by VeyraVanguardAnim. Its proxy samples the clips and blends
 * them. The grey-box presentation feeds it; it holds no gameplay logic, and nothing reads it back.
 */
UCLASS(Transient, NotBlueprintable)
class VEYRAUI_API UVeyraVanguardAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	/** Fits it to Art and Shape's blend times and play rates; Shape's art-given values come from Art. Until then it holds the reference pose. */
	void Configure(const FVeyraVanguardArt& Art, const FVeyraVanguardAnimShape& Shape);

	bool IsConfigured() const { return Clips.Num() > 0; }

	/** Shows a combat cue about its body; SecondsLeft is how long an attack's windup has before it commits. */
	void NoteCue(EVeyraCombatCueKind Cue, float SecondsLeft);

	/** What its body is doing, for the frames that follow. */
	void SetInputs(const FVeyraVanguardAnimInputs& InInputs) { Inputs = InInputs; }

	const FVeyraVanguardAnimState& GetState() const { return State; }
	const FVeyraVanguardAnimShape& GetShape() const { return Shape; }

	/** The clip it plays as Clip, or null. */
	const UAnimSequence* GetClip(EVeyraVanguardClip Clip) const;

	FName GetUpperBodyBone() const { return UpperBodyBone; }

protected:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;

private:
	/** Its clips by EVeyraVanguardClip. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UAnimSequence>> Clips;

	FName UpperBodyBone;
	FVeyraVanguardAnimShape Shape;
	FVeyraVanguardAnimState State;
	FVeyraVanguardAnimInputs Inputs;
};
