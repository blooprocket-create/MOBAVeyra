// Copyright © 2026 Wayfinder Studios. All rights reserved.
#include "Layout/VeyraWorldAuthoring.h"
#include "Features/IModularFeatures.h"
#include "Modules/ModuleManager.h"
#include "VeyraWorldBuild.h"

class FVeyraWorldToolsModule final : public IModuleInterface, public IVeyraWorldAuthoring
{
public:
    virtual void StartupModule() override { IModularFeatures::Get().RegisterModularFeature(FeatureName(), this); }
    virtual void ShutdownModule() override { IModularFeatures::Get().UnregisterModularFeature(FeatureName(), this); }
    virtual bool Generate(UWorld& World, FString& Error) override { return VeyraWorldBuild::Generate(World, Error); }
};
IMPLEMENT_MODULE(FVeyraWorldToolsModule, VeyraWorldTools)
