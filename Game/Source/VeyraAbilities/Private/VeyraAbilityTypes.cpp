// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "VeyraAbilityTypes.h"

const TCHAR* LexToString(EVeyraCastRejection Rejection)
{
	switch (Rejection)
	{
	case EVeyraCastRejection::None:
		return TEXT("None");
	case EVeyraCastRejection::UnknownAbility:
		return TEXT("UnknownAbility");
	case EVeyraCastRejection::CasterDead:
		return TEXT("CasterDead");
	case EVeyraCastRejection::CrowdControlled:
		return TEXT("CrowdControlled");
	case EVeyraCastRejection::NotLearned:
		return TEXT("NotLearned");
	case EVeyraCastRejection::Busy:
		return TEXT("Busy");
	case EVeyraCastRejection::OnCooldown:
		return TEXT("OnCooldown");
	case EVeyraCastRejection::InsufficientResource:
		return TEXT("InsufficientResource");
	case EVeyraCastRejection::InvalidTarget:
		return TEXT("InvalidTarget");
	case EVeyraCastRejection::TargetDead:
		return TEXT("TargetDead");
	case EVeyraCastRejection::NotHostile:
		return TEXT("NotHostile");
	case EVeyraCastRejection::OutOfRange:
		return TEXT("OutOfRange");
	case EVeyraCastRejection::InvalidLocation:
		return TEXT("InvalidLocation");
	case EVeyraCastRejection::WrongPhase:
		return TEXT("WrongPhase");
	case EVeyraCastRejection::Paused:
		return TEXT("Paused");
	case EVeyraCastRejection::ActivationFailed:
		return TEXT("ActivationFailed");
	case EVeyraCastRejection::Locked:
		return TEXT("Locked");
	case EVeyraCastRejection::NotVisible:
		return TEXT("NotVisible");
	case EVeyraCastRejection::NoCompanion:
		return TEXT("NoCompanion");
	case EVeyraCastRejection::HeldBack:
		return TEXT("HeldBack");
	case EVeyraCastRejection::Projected:
		return TEXT("Projected");
	}
	return TEXT("Unknown");
}
