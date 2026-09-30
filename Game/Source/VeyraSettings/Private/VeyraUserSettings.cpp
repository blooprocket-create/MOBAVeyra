// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "VeyraUserSettings.h"

#include "Engine/Engine.h"

UVeyraUserSettings* UVeyraUserSettings::Get()
{
	return GEngine ? Cast<UVeyraUserSettings>(GEngine->GetGameUserSettings()) : nullptr;
}
