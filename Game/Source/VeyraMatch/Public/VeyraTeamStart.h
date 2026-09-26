// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "GameFramework/PlayerStart.h"
#include "Teams/VeyraTeam.h"

#include "VeyraTeamStart.generated.h"

/** Where a side's Vanguards enter the map and return after death: the fountain (Combat Bible §18). */
UCLASS()
class VEYRAMATCH_API AVeyraTeamStart : public APlayerStart, public IVeyraTeamMember
{
	GENERATED_BODY()

public:
	AVeyraTeamStart(const FObjectInitializer& ObjectInitializer);

	virtual EVeyraTeam GetVeyraTeam() const override { return Team; }

	/** For starts spawned at runtime, such as a test map's; placed starts set Team in the editor. */
	void SetVeyraTeam(EVeyraTeam NewTeam) { Team = NewTeam; }

private:
	UPROPERTY(EditAnywhere, Category = "Team")
	EVeyraTeam Team = EVeyraTeam::None;
};
