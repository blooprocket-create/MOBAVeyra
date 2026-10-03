// Copyright © 2026 Wayfinder Studios. All rights reserved.
#include "VeyraWorldBuild.h"
#include "Dom/JsonObject.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Tuning/VeyraWorldTuningSubsystem.h"

namespace VeyraWorldBuild
{
bool Generate(UWorld& World, FString& Error)
{
    const auto Plugin = IPluginManager::Get().FindPlugin(TEXT("VeyraWorldTools"));
    FString Json;
    TSharedPtr<FJsonObject> Style;
    if (!Plugin || !FFileHelper::LoadFileToString(Json, *(Plugin->GetBaseDir() / TEXT("Config/CrucibleStyle.json")))
        || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Style) || !Style.IsValid())
    {
        Error = TEXT("Cannot load CrucibleStyle.json.");
        return false;
    }
    const auto& Tuning = UVeyraWorldTuningSubsystem::Get();
    if (!Landscape(World, Tuning, *Style, Error) || !River(World, Tuning, *Style, Error) || !Dressing(World, Tuning, *Style, Error)) { return false; }
    ReviewScene(World, Tuning, *Style);
    return true;
}
}
