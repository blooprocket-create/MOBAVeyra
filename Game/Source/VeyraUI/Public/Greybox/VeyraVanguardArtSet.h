// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Engine/DataAsset.h"
#include "Greybox/VeyraVanguardAnimation.h"

#include "VeyraVanguardArtSet.generated.h"

class UAnimSequence;
class USkeletalMesh;

/** A Vanguard's generated body (ADR-064 §3): its skeletal mesh, its animations, and what they are fitted to. */
USTRUCT(BlueprintType)
struct VEYRAUI_API FVeyraVanguardArt
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

	/** The animation Clip, or null. */
	UAnimSequence* Find(EVeyraVanguardClip Clip) const;

	/** How long each of its animations plays at its own speed. */
	FVeyraVanguardClipLengths Lengths() const;
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

	/** The body for the Vanguard Id, or null. */
	const FVeyraVanguardArt* Find(FName Id) const { return Art.Find(Id); }

	/** Every problem with the set, as "Id: message". Empty when usable. */
	TArray<FString> Validate() const;
};
