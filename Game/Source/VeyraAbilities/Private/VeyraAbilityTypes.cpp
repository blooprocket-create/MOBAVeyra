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
	case EVeyraCastRejection::WrongPhase:
		return TEXT("WrongPhase");
	case EVeyraCastRejection::Paused:
		return TEXT("Paused");
	case EVeyraCastRejection::ActivationFailed:
		return TEXT("ActivationFailed");
	}
	return TEXT("Unknown");
}
