// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Engine/LocalPlayer.h"
#include "Templates/Function.h"

#include "VeyraLocalPlayer.generated.h"

/**
 * The local player on a Veyra client. When joining a server it reports the client's tuning hash
 * (ADR-006 §6) and, for an assigned match, its join ticket (ADR-007 §4). The engine asks for these
 * login options only after the server answers, so the ticket never appears in the travel URL.
 */
UCLASS()
class VEYRAMATCH_API UVeyraLocalPlayer : public ULocalPlayer
{
	GENERATED_BODY()

public:
	virtual FString GetGameLoginOptions() const override;

	/** The join ticket for the match server this player joins next. It is never logged. */
	void SetJoinTicket(const FString& Ticket) { JoinTicket = Ticket; }
	void ClearJoinTicket() { JoinTicket.Reset(); }

#if WITH_DEV_AUTOMATION_TESTS
	/**
	 * Tests: supplies the join ticket of every player without one. In-process play sessions create
	 * their clients themselves, so a test cannot hand each client its ticket directly. Pass null to
	 * stop.
	 */
	static void SetTestTicketProvider(TFunction<FString(const ULocalPlayer&)> Provider);
#endif

private:
	FString JoinTicket;
};
