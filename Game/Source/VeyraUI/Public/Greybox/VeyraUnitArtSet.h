// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Engine/DataAsset.h"

#include "VeyraUnitArtSet.generated.h"

class UStaticMesh;

/** A unit's art, drawn in place of its body: intact, and fallen (a structure's wreck, or a collapsed body). */
USTRUCT(BlueprintType)
struct FVeyraUnitArt
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Art")
	TObjectPtr<UStaticMesh> Intact;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Art")
	TObjectPtr<UStaticMesh> Fallen;
};

/**
 * One art kit's meshes by the stable IDs of what they dress (ADR-006 §6: Data Assets hold asset references keyed
 * by stable IDs), such as a structure kind's ("laneSpire") or a Fluxborn's ("strider"), and the material slot
 * whose Flux glows in the unit's colour. Game/Scripts/BuildUnitArtSets.py writes each from its kit's manifest.
 * Presentation only: no gameplay value lives here.
 */
UCLASS(BlueprintType)
class VEYRAUI_API UVeyraUnitArtSet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Art")
	TMap<FName, FVeyraUnitArt> Art;

	/** The meshes' material slot whose Flux glows in the unit's colour, and the vector parameter that colours it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Art")
	FName FluxSlot;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Art")
	FName FluxParameter;

	/** The art for Id, or null. */
	const FVeyraUnitArt* Find(FName Id) const { return Art.Find(Id); }

	/** Every problem with the set, as "Id: message"; Required names the IDs it must hold. Empty when usable. */
	TArray<FString> Validate(TConstArrayView<FName> Required) const;
};
