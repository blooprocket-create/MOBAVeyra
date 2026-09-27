// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Join/VeyraMatchAssignment.h"

const TCHAR* LexToString(EVeyraMatchEndReason Reason)
{
	switch (Reason)
	{
	case EVeyraMatchEndReason::DeveloperRequest:
		return TEXT("developer request");
	case EVeyraMatchEndReason::Abandoned:
		return TEXT("abandoned");
	}
	return TEXT("unknown");
}
