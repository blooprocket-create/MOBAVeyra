// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Engine/DataAsset.h"
#include "Greybox/VeyraVanguardAnimation.h"

#include "VeyraVanguardArtSet.generated.h"

class UAnimSequence;
class UNiagaraSystem;
class USkeletalMesh;

/** A two-bone limb its inverse kinematics solves (ADR-069): its root (a hip or shoulder), its joint and its end. */
USTRUCT(BlueprintType)
struct VEYRAUI_API FVeyraLimbChain
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Art")
	FName Root;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Art")
	FName Joint;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Art")
	FName End;

	bool IsSet() const { return !Root.IsNone() && !Joint.IsNone() && !End.IsNone(); }
};

/**
 * A chain of bones a body's loose part hangs on (ADR-069): a cloak's edge, a coat's tail, a lock of hair, from the bone
 * it hangs from to its tip, trailing the clips' pose as cloth does. Stiffness: the spring drawing each joint back toward
 * where its clip has it (per second squared); Drag: how much of its speed through the air it loses (per second);
 * Damping: how much of its speed relative to its clip it loses (per second); MaxAngleDegrees: how far a bone may turn
 * from its clip's.
 */
USTRUCT(BlueprintType)
struct VEYRAUI_API FVeyraSpringChainArt
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Art")
	TArray<FName> Bones;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Art")
	float Stiffness = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Art")
	float Drag = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Art")
	float Damping = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Art")
	float MaxAngleDegrees = 0.0f;
};

/** A capsule a body's chains hang outside (its torso, a thigh): between two bones' heads, Radius about them. */
USTRUCT(BlueprintType)
struct VEYRAUI_API FVeyraSpringColliderArt
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Art")
	FName From;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Art")
	FName To;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Art")
	float Radius = 0.0f;
};

/** One generated body (ADR-064 §3): its skeletal mesh, its animations, and what they are fitted to. */
USTRUCT(BlueprintType)
struct VEYRAUI_API FVeyraVanguardBody
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Art")
	TObjectPtr<USkeletalMesh> Mesh;

	/** Its animations by name (VeyraVanguardAnim::NameOf): Idle, Run, AttackWindup, AttackStrike, Cast, Hit, Death and Recall. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Art")
	TMap<FName, TObjectPtr<UAnimSequence>> Animations;

	/** How far one Run cycle carries the body, in units, so Run steps at the speed its capsule moves. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Art")
	float RunStride = 0.0f;

	/** The share of Cast that raises the hands to the release. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Art")
	float CastReleaseShare = 0.0f;

	/** The bone whose chain is the upper body, which alone plays an attack, a cast or a hit while the body runs. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Art")
	FName UpperBodyBone;

	/**
	 * What the body is made of where no mesh can show it, as Nix's smoke: a looping effect that pours off each of
	 * EffectBones as the body moves, in EffectColor and at EffectScale (the body's own scale, so a larger form pours
	 * larger smoke). None for a body that is all mesh.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Art")
	TObjectPtr<UNiagaraSystem> Effect;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Art")
	TArray<FName> EffectBones;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Art")
	FLinearColor EffectColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Art")
	float EffectScale = 1.0f;

	/**
	 * As a status body, which wins when its unit holds several statuses with bodies: the highest, as a brief burst's body
	 * over one its unit holds all the while in some ground. Ties go by status ID.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Art")
	int32 Priority = 0;

	/**
	 * Its legs, each held to the ground under it while its foot is planted (ADR-069): the inverse kinematics that keep
	 * feet on uneven ground. Empty for a body that does not stand on feet.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Art")
	TArray<FVeyraLimbChain> FootChains;

	/**
	 * An arm whose hand holds a weapon the other hand carries (a rifle's fore-end), kept on it however the other hand
	 * moves: its chain, and the bone that carries the weapon (OffHandAnchor). Unset for a body with no such hold.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Art")
	FVeyraLimbChain OffHand;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Art")
	FName OffHandAnchor;

	/** Its loose parts' chains (ADR-069), and the capsules they hang outside. Empty for a body with nothing loose. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Art")
	TArray<FVeyraSpringChainArt> SpringChains;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Art")
	TArray<FVeyraSpringColliderArt> SpringColliders;

	/** The animation Clip, or null. */
	UAnimSequence* Find(EVeyraVanguardClip Clip) const;

	/** How long each of its animations plays at its own speed. */
	FVeyraVanguardClipLengths Lengths() const;

	/** Every problem with the body, each prefixed by Label. Empty when usable. */
	TArray<FString> Validate(const FString& Label) const;
};

/**
 * A Vanguard's generated bodies: its own, and those it wears in its own's place while it holds a status. A rider is on
 * foot until its ride's status puts it on its mount (ADR-064 §1). Which body shows follows the replicated statuses;
 * nothing here decides them.
 */
USTRUCT(BlueprintType)
struct VEYRAUI_API FVeyraVanguardArt : public FVeyraVanguardBody
{
	GENERATED_BODY()

	/**
	 * The bodies it wears while it holds a status, by status ID (Abilities.json statuses), and while a stance's set is in
	 * its slots, by the stance ability's ID (Abilities.json stance; ADR-031 §3).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Art")
	TMap<FName, FVeyraVanguardBody> StatusBodies;

	/** The body to wear: of the status bodies whose status or stance Holds says it holds, the highest Priority's (then the first by ID), else its own. */
	const FVeyraVanguardBody& BodyFor(TFunctionRef<bool(FName)> Holds) const;
};

/**
 * The Vanguards' generated bodies by Vanguard ID (ADR-064 §3; ADR-006 §6: Data Assets hold asset references keyed by
 * stable IDs). Game/Scripts/ImportVanguardBodies.py writes it from the body kit's manifest. Presentation only: no
 * gameplay value lives here, and a Vanguard without art keeps its grey-box body.
 */
UCLASS(BlueprintType)
class VEYRAUI_API UVeyraVanguardArtSet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Art")
	TMap<FName, FVeyraVanguardArt> Art;

	/** Companions' bodies by companion ID (Abilities.json companions), as Nix's: the other half of a Vanguard's pair. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Art")
	TMap<FName, FVeyraVanguardArt> CompanionArt;

	/** The bodies for the Vanguard Id, or null. */
	const FVeyraVanguardArt* Find(FName Id) const { return Art.Find(Id); }

	/** The bodies for the companion Id, or null. */
	const FVeyraVanguardArt* FindCompanion(FName Id) const { return CompanionArt.Find(Id); }

	/** Every problem with the set, as "Id: message". Empty when usable. */
	TArray<FString> Validate() const;
};
