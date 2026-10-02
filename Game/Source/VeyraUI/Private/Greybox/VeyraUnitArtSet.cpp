// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Greybox/VeyraUnitArtSet.h"

#include "Engine/StaticMesh.h"

TArray<FString> UVeyraUnitArtSet::Validate(TConstArrayView<FName> Required) const
{
	TArray<FString> Problems;
	for (const FName Id : Required)
	{
		const FVeyraUnitArt* Entry = Art.Find(Id);
		if (!Entry || !Entry->Intact || !Entry->Fallen)
		{
			Problems.Add(FString::Printf(TEXT("%s: needs an intact and a fallen mesh."), *Id.ToString()));
		}
	}
	if (FluxSlot.IsNone() || FluxParameter.IsNone())
	{
		Problems.Add(TEXT("FluxSlot: the Flux slot and its parameter must be named."));
	}
	return Problems;
}
