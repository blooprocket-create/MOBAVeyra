// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "DevCommands/VeyraDevCommands.h"
#include "Modules/ModuleManager.h"

class FVeyraDeveloperModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		// The Veyra.Dev.* commands, and the server's end of their route.
		VeyraDevCommands::Register();
	}

	virtual void ShutdownModule() override
	{
		VeyraDevCommands::Unregister();
	}
};

IMPLEMENT_MODULE(FVeyraDeveloperModule, VeyraDeveloper);
