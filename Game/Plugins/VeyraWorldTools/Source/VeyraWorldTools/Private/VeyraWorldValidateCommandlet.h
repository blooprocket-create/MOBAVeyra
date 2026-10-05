// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Commandlets/Commandlet.h"

#include "VeyraWorldValidateCommandlet.generated.h"

/**
 * Validates the generated battleground (ADR-040; World Validation Standard gates 6, 7 and 8). It loads the map, spawns the
 * server's own structures and walls, builds the navigation the server builds, and measures each team's equivalent routes,
 * the terrain's half-turn symmetry, where every anchor stands, and that generated presentation has no collision. The
 * profile is Config/CrucibleValidation.json; the report goes to its "report" path. Any finding fails the run.
 * Game/Scripts/ValidateBattleground.ps1 runs it.
 */
UCLASS()
class UVeyraWorldValidateCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UVeyraWorldValidateCommandlet();

	virtual int32 Main(const FString& Params) override;
};
