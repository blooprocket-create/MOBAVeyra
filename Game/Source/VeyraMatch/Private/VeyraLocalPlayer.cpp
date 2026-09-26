// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "VeyraLocalPlayer.h"

#include "Tuning/VeyraTuning.h"
#include "VeyraJoinRules.h"

FString UVeyraLocalPlayer::GetGameLoginOptions() const
{
	return VeyraJoinRules::MakeTuningHashOption(VeyraTuning::GetCompositeHash());
}
