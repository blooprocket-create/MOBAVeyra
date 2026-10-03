// Copyright © 2026 Wayfinder Studios. All rights reserved.
#pragma once
#include "CoreMinimal.h"
class UWorld;
class FJsonObject;
struct FVeyraWorldTuning;
namespace VeyraWorldBuild
{
    bool River(UWorld& World, const FVeyraWorldTuning& Tuning, const FJsonObject& Style, FString& Error);
    bool Dressing(UWorld& World, const FVeyraWorldTuning& Tuning, const FJsonObject& Style, FString& Error);
    void ReviewScene(UWorld& World, const FVeyraWorldTuning& Tuning, const FJsonObject& Style);
    bool Generate(UWorld& World, FString& Error);
    bool Landscape(UWorld& World, const FVeyraWorldTuning& Tuning, const FJsonObject& Style, FString& Error);
}
