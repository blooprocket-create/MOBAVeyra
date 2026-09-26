// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Misc/CoreDelegates.h"
#include "Modules/ModuleManager.h"

namespace
{
	/**
	 * Loads the engine modules that would otherwise load only after the first map. By then Iris is
	 * replicating, and any module that loads makes it rebuild its polymorphic type tables (GAS
	 * target data, effect contexts) and warn that clients and servers could disagree. Loading these
	 * first keeps that warning for a real late load (ADR-006 §5).
	 */
	void LoadEngineModulesBeforeTheFirstMap()
	{
		static const TCHAR* const ModuleNames[] = {
#if WITH_AUTOMATION_WORKER
			// FEngineLoop::Init loads these after starting the engine, which loads the first map.
			TEXT("AutomationWorker"),
#endif
#if WITH_ENGINE && !UE_BUILD_SHIPPING
			TEXT("AutomationController"),
#endif
			// Loaded on demand: server performance counters on the first network tick, and voice when
			// a client connects.
			TEXT("PerfCounters"),
			TEXT("Voice"),
		};

		FModuleManager& Modules = FModuleManager::Get();
		for (const TCHAR* ModuleName : ModuleNames)
		{
			if (Modules.ModuleExists(ModuleName))
			{
				Modules.LoadModule(ModuleName);
			}
		}
	}

	/** Composition root (ADR-006 §3): module startup and default-class wiring only. */
	class FVeyraModule : public FDefaultGameModuleImpl
	{
	public:
		virtual void StartupModule() override
		{
			// After the engine initializes, before it starts and loads the first map.
			PostEngineInitHandle = FCoreDelegates::GetOnPostEngineInit().AddStatic(&LoadEngineModulesBeforeTheFirstMap);
		}

		virtual void ShutdownModule() override
		{
			FCoreDelegates::GetOnPostEngineInit().Remove(PostEngineInitHandle);
		}

	private:
		FDelegateHandle PostEngineInitHandle;
	};
}

IMPLEMENT_PRIMARY_GAME_MODULE(FVeyraModule, Veyra, "Veyra");
