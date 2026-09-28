// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "VeyraMatchTypes.h"

const TCHAR* LexToString(EVeyraOrderRejection Rejection)
{
	switch (Rejection)
	{
	case EVeyraOrderRejection::None:
		return TEXT("None");
	case EVeyraOrderRejection::TooFrequent:
		return TEXT("TooFrequent");
	case EVeyraOrderRejection::WrongPhase:
		return TEXT("WrongPhase");
	case EVeyraOrderRejection::Paused:
		return TEXT("Paused");
	case EVeyraOrderRejection::NoVanguard:
		return TEXT("NoVanguard");
	case EVeyraOrderRejection::InvalidOrder:
		return TEXT("InvalidOrder");
	case EVeyraOrderRejection::Unreachable:
		return TEXT("Unreachable");
	case EVeyraOrderRejection::CannotAttack:
		return TEXT("CannotAttack");
	case EVeyraOrderRejection::CrowdControlled:
		return TEXT("CrowdControlled");
	}
	return TEXT("Unknown");
}

const TCHAR* LexToString(EVeyraEndCustomMatchRefusal Refusal)
{
	switch (Refusal)
	{
	case EVeyraEndCustomMatchRefusal::None:
		return TEXT("None");
	case EVeyraEndCustomMatchRefusal::NotCustomMatch:
		return TEXT("NotCustomMatch");
	case EVeyraEndCustomMatchRefusal::NotHost:
		return TEXT("NotHost");
	case EVeyraEndCustomMatchRefusal::AlreadyEnded:
		return TEXT("AlreadyEnded");
	}
	return TEXT("Unknown");
}
