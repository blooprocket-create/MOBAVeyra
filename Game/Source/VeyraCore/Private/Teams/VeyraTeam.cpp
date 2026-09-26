// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Teams/VeyraTeam.h"

namespace VeyraTeams
{
EVeyraTeam TeamOf(const UObject* Object)
{
	const IVeyraTeamMember* Member = Cast<IVeyraTeamMember>(Object);
	return Member ? Member->GetVeyraTeam() : EVeyraTeam::None;
}
}
