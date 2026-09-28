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
	case EVeyraMatchEndReason::HostEnded:
		return TEXT("host ended");
	case EVeyraMatchEndReason::PrimeWellDestroyed:
		return TEXT("prime well destroyed");
	}
	return TEXT("unknown");
}

namespace VeyraMatchResults
{
bool IsWinnerConsistent(EVeyraMatchEndReason Reason, EVeyraTeam Winner)
{
	const bool bHasWinner = Winner == EVeyraTeam::A || Winner == EVeyraTeam::B;
	return (Reason == EVeyraMatchEndReason::PrimeWellDestroyed) == bHasWinner;
}
}
