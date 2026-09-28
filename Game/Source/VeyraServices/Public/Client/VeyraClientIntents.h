// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Client/VeyraClientFlowTypes.h"
#include "Delegates/Delegate.h"

/**
 * What presentation may do with the client-state coordinator (ADR-010 §2): observe its snapshot and
 * ask through intents. Whether an intent is allowed now is CanIssue's to say; the coordinator and the
 * backend decide the outcome. Each intent returns false when it was refused outright.
 */
class IVeyraClientIntents
{
public:
	virtual ~IVeyraClientIntents() = default;

	virtual const FVeyraClientSnapshot& GetSnapshot() const = 0;

	/** Broadcast after every change to the snapshot. */
	virtual FSimpleMulticastDelegate& OnChanged() = 0;

	virtual bool CanIssue(EVeyraClientIntent Intent) const = 0;

	/** Seconds left on the select's pick timer, by the backend's clock as last read. */
	virtual double GetRemainingPickSeconds() const = 0;

	virtual bool ChooseStarter(const FString& VanguardId) = 0;
	virtual bool StartPractice() = 0;
	virtual bool HoverVanguard(const FString& VanguardId) = 0;
	virtual bool LockVanguard(const FString& VanguardId) = 0;
	virtual bool Reconnect() = 0;
	virtual bool ContinueFromResults() = 0;
	virtual bool Retry() = 0;
	virtual bool Quit() = 0;
};
