// Copyright © 2026 Wayfinder Studios. All rights reserved.

// The developer command for the skill showcase's stage (ADR-074 §2): its server puts the stage back as it began
// between one recorded cast and the next.

#include "DevCommands/VeyraDevCommands.h"

#include "Engine/World.h"
#include "Showcase/VeyraShowcaseSubsystem.h"
#include "VeyraPlayerController.h"

namespace VeyraDevShowcaseCommands
{
	const TCHAR* const Category = TEXT("Showcase");

	FString RunResetShowcase(AVeyraPlayerController& Requester, TConstArrayView<FString> /*Args*/)
	{
		UVeyraShowcaseSubsystem* Stage = Requester.GetWorld()->GetSubsystem<UVeyraShowcaseSubsystem>();
		return Stage ? Stage->Reset(Requester) : FString(TEXT("This match is not the skill showcase's stage (ADR-074 §2)."));
	}
}

void VeyraDevCommands::AddShowcaseCommands(TArray<FVeyraDevCommand>& Out)
{
	using namespace VeyraDevShowcaseCommands;
	Out.Add(FVeyraDevCommand::Server(TEXT("ResetShowcase"), Category, TEXT(""),
		TEXT("On the skill showcase's stage: your Vanguard and the dummy are whole again at their starts, and your cooldowns are clear."),
		&RunResetShowcase));
}
