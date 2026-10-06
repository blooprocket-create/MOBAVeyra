// Copyright © 2026 Wayfinder Studios. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Features/IModularFeature.h"

class UWorld;

/** Optional editor implementation of world generation. Gameplay never queries this service. */
class IVeyraWorldAuthoring : public IModularFeature
{
public:
	static FName FeatureName() { return TEXT("VeyraWorldAuthoring"); }
	virtual bool Generate(UWorld& World, FString& Error) = 0;
};
