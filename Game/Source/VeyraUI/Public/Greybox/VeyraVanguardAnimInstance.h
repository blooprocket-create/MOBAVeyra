// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Animation/AnimInstance.h"
#include "Greybox/VeyraSpringChain.h"
#include "Greybox/VeyraVanguardAnimation.h"
#include "Greybox/VeyraVanguardArtSet.h"

#include "VeyraVanguardAnimInstance.generated.h"

class UAnimSequence;
struct FVeyraVanguardBody;

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
	void Configure(const FVeyraVanguardBody& Art, const FVeyraVanguardAnimShape& Shape);

	bool IsConfigured() const { return Clips.Num() > 0; }

	/**
	 * Shows a combat cue about its body; SecondsLeft is how long an attack's or a cast's windup has before it commits,
	 * and Ability a cast's, whose own clip plays if the body has one (ADR-072 §1).
	 */
	void NoteCue(EVeyraCombatCueKind Cue, float SecondsLeft, FName Ability = NAME_None);

	/** What its body is doing, for the frames that follow. */
	void SetInputs(const FVeyraVanguardAnimInputs& InInputs) { Inputs = InInputs; }

	const FVeyraVanguardAnimState& GetState() const { return State; }
	const FVeyraVanguardAnimShape& GetShape() const { return Shape; }

	/** The clip it plays as Clip, or null. */
	const UAnimSequence* GetClip(EVeyraVanguardClip Clip) const;

	/** The clip it plays for a cast of Ability, if the body has one of its own; null otherwise. */
	const UAnimSequence* GetSkillClip(FName Ability) const;

	FName GetUpperBodyBone() const { return UpperBodyBone; }

	/** Its limbs' inverse kinematics this frame (ADR-069), for the proxy: what the ground and the weapon ask of them. */
	struct FLimbFrame
	{
		/** Its legs, and each foot's ground: how far above (or below, negative) the floor its capsule stands on, in the skin's units, and its slope's normal in the skin's space. */
		TArray<FVeyraLimbChain> Feet;
		TArray<float> FootRestHeights;
		TArray<float> FootOffsets;
		TArray<FVector> FootNormals;
		/** How much of the feet's hold its speed leaves (1 standing). */
		float FootWeight = 0.0f;
		FName Pelvis;
		/** Its off hand and the weapon hand it holds to, and where the off hand rests on that hand. */
		FVeyraLimbChain OffHand;
		FName OffHandAnchor;
		FTransform OffHandFromAnchor = FTransform::Identity;
		float OffHandWeight = 0.0f;
		float OffHandRelease = 0.0f;
		/** Its loose parts' chains and the capsules they hang outside (ADR-069), and how every chain keeps time. */
		TArray<FVeyraSpringChainArt> Springs;
		TArray<FVeyraSpringColliderArt> SpringColliders;
		VeyraSpringChain::FTiming SpringTiming;
		float MaxStretch = 0.0f;
		float MaxPelvisDrop = 0.0f;
		float PlantFade = 0.0f;
		float GroundTilt = 0.0f;
	};

	const FLimbFrame& GetLimbFrame() const { return Limbs; }

protected:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	/** Finds the ground under each foot, from where the feet stood last frame (ADR-069), and eases toward it. */
	void TraceFeet(float DeltaSeconds);

	/** Its limbs' chains and rest from Art (ADR-069), as the settings allow. */
	void ConfigureLimbs(const FVeyraVanguardBody& Art);
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;

private:
	/** Its clips by EVeyraVanguardClip. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UAnimSequence>> Clips;

	/** Its skills' own clips by ability ID (ADR-072 §1). */
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UAnimSequence>> SkillClips;

	FName UpperBodyBone;
	FLimbFrame Limbs;
	FVeyraVanguardAnimShape Shape;
	FVeyraVanguardAnimState State;
	FVeyraVanguardAnimInputs Inputs;
};
