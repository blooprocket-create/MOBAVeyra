// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Engine/LocalPlayer.h"

#include "VeyraLocalPlayer.generated.h"

/** The local player on a Veyra client. It reports the client's tuning hash when joining a server (ADR-006 §6). */
UCLASS()
class VEYRAMATCH_API UVeyraLocalPlayer : public ULocalPlayer
{
	GENERATED_BODY()

public:
	virtual FString GetGameLoginOptions() const override;
};
