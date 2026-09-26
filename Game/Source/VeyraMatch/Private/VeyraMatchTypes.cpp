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
	}
	return TEXT("Unknown");
}
