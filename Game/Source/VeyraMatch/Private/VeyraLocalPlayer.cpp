// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "VeyraLocalPlayer.h"

#include "Tuning/VeyraTuning.h"
#include "VeyraJoinRules.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
	TFunction<FString(const ULocalPlayer&)>& TestTicketProvider()
	{
		static TFunction<FString(const ULocalPlayer&)> Provider;
		return Provider;
	}
}

void UVeyraLocalPlayer::SetTestTicketProvider(TFunction<FString(const ULocalPlayer&)> Provider)
{
	TestTicketProvider() = MoveTemp(Provider);
}
#endif

FString UVeyraLocalPlayer::GetGameLoginOptions() const
{
	FString Options = VeyraJoinRules::MakeTuningHashOption(VeyraTuning::GetCompositeHash());
	FString Ticket = JoinTicket;
#if WITH_DEV_AUTOMATION_TESTS
	if (Ticket.IsEmpty() && TestTicketProvider())
	{
		Ticket = TestTicketProvider()(*this);
	}
#endif
	if (!Ticket.IsEmpty())
	{
		Options += TEXT("?") + VeyraJoinRules::MakeTicketOption(Ticket);
	}
	return Options;
}
