// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "UObject/Interface.h"

#include "VeyraTeam.generated.h"

/** The two sides of a match, named as the Battleground Bible names them (§15, "Team A" and "Team B"). */
UENUM()
enum class EVeyraTeam : uint8
{
	/** On neither side: a participant not yet assigned, or a neutral unit. */
	None,
	A,
	B,
};

UINTERFACE(MinimalAPI, NotBlueprintable)
class UVeyraTeamMember : public UInterface
{
	GENERATED_BODY()
};

/** Anything that belongs to a side: a participant's PlayerState, its Vanguard, later owned entities. */
class IVeyraTeamMember
{
	GENERATED_BODY()

public:
	virtual EVeyraTeam GetVeyraTeam() const = 0;
};

namespace VeyraTeams
{
	/** The side of any object that declares one, and None for everything else. */
	VEYRACORE_API EVeyraTeam TeamOf(const UObject* Object);
}
