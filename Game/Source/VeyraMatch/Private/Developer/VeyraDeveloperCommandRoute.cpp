// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Developer/VeyraDeveloperCommandRoute.h"

namespace VeyraDeveloperCommandRoute
{
	namespace
	{
		FHandler& InstalledHandler()
		{
			static FHandler Handler;
			return Handler;
		}
	}

	void SetHandler(FHandler Handler)
	{
		InstalledHandler() = MoveTemp(Handler);
	}

	FString Run(AVeyraPlayerController& Requester, const FString& Command, const TArray<FString>& Args)
	{
		const FHandler& Handler = InstalledHandler();
		return Handler ? Handler(Requester, Command, Args) : FString(TEXT("This server has no developer commands."));
	}
}
