// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "VeyraLocalPlayer.h"

#include "Content/VeyraContentId.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
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
#if !UE_BUILD_SHIPPING
	// A development client may ask for its Vanguard with -VeyraVanguard=<id> (ADR-008 §8).
	FString Vanguard;
	if (FParse::Value(FCommandLine::Get(), TEXT("-VeyraVanguard="), Vanguard) && FVeyraContentId::FromText(Vanguard).IsSet())
	{
		Options += FString::Printf(TEXT("?%s=%s"), VeyraJoinRules::VanguardOption, *Vanguard);
	}
#endif
	return Options;
}
