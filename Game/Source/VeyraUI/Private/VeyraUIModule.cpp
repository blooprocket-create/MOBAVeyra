// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Modules/ModuleManager.h"
#include "Text/VeyraContentText.h"
#include "VeyraUILog.h"

DEFINE_LOG_CATEGORY(LogVeyraUI);

/** Registers what players read about Vanguards before any screen needs it. */
class FVeyraUIModule : public FDefaultModuleImpl
{
public:
	virtual void StartupModule() override
	{
		VeyraContentText::Register();
	}
};

IMPLEMENT_MODULE(FVeyraUIModule, VeyraUI);
