// Copyright © 2026 Wayfinder Studios. All rights reserved.

// The developer commands for the typist's Gold, XP and items. Gold arrives as Developer Gold, which
// the statistics never count as earned (ADR-017 §9); items come from the shop's own rules.

#include "DevCommands/VeyraDevCommands.h"

#include "Engine/World.h"
#include "Gold/VeyraGoldComponent.h"
#include "Inventory/VeyraInventoryRules.h"
#include "Misc/OutputDevice.h"
#include "Progression/VeyraProgressionComponent.h"
#include "Progression/VeyraProgressionRules.h"
#include "Progression/VeyraProgressionTuningSubsystem.h"
#include "Shop/VeyraShopSubsystem.h"
#include "Tuning/VeyraItemsTuningSubsystem.h"
#include "VeyraGameMode.h"
#include "VeyraPlayerController.h"
#include "VeyraPlayerState.h"

namespace VeyraDevEconomyCommands
{
	const TCHAR* const EconomyCategory = TEXT("Economy");
	const TCHAR* const ItemsCategory = TEXT("Items");

	/** The requester's progression, if the match lets it change now: as it lets ranks (ADR-008 §6). */
	UVeyraProgressionComponent* ChangeableProgression(const AVeyraPlayerController& Requester)
	{
		const AVeyraGameMode* GameMode = Requester.GetWorld()->GetAuthGameMode<AVeyraGameMode>();
		const AVeyraPlayerState* Participant = VeyraDevCommands::ParticipantOf(Requester);
		if (!GameMode || !Participant || GameMode->CheckRankUpAllowed() != EVeyraOrderRejection::None)
		{
			return nullptr;
		}
		UVeyraProgressionComponent* Progression = Participant->FindComponentByClass<UVeyraProgressionComponent>();
		return Progression && Progression->IsInitialized() ? Progression : nullptr;
	}

	const TCHAR* const NoProgression = TEXT("XP changes only in preparation or a live match, not paused, once your Vanguard is chosen.");

	FString RunGold(AVeyraPlayerController& Requester, TConstArrayView<FString> Args)
	{
		const TOptional<double> Amount = VeyraDevCommands::ParseNumber(Args, 0);
		if (!Amount.IsSet() || Amount.GetValue() <= 0.0 || Args.Num() != 1)
		{
			return VeyraDevCommands::UsageReply(TEXT("Gold"));
		}
		const AVeyraPlayerState* Participant = VeyraDevCommands::ParticipantOf(Requester);
		UVeyraGoldComponent* Gold = Participant ? Participant->FindComponentByClass<UVeyraGoldComponent>() : nullptr;
		if (!Gold || !Gold->Grant(Amount.GetValue(), EVeyraGoldReason::Developer))
		{
			return TEXT("You have no Gold to add to yet.");
		}
		return FString::Printf(TEXT("Gave you %.0f Gold: you have %.0f."), Amount.GetValue(), Gold->GetGold());
	}

	FString RunGrantXp(AVeyraPlayerController& Requester, TConstArrayView<FString> Args)
	{
		const TOptional<double> Amount = VeyraDevCommands::ParseNumber(Args, 0);
		if (!Amount.IsSet() || Amount.GetValue() <= 0.0 || Args.Num() != 1)
		{
			return VeyraDevCommands::UsageReply(TEXT("GrantXp"));
		}
		UVeyraProgressionComponent* Progression = ChangeableProgression(Requester);
		if (!Progression)
		{
			return NoProgression;
		}
		const int32 Before = Progression->GetLevel();
		Progression->AddExperience(Amount.GetValue());
		return FString::Printf(TEXT("Took %.0f XP: level %d -> %d."), Amount.GetValue(), Before, Progression->GetLevel());
	}

	FString RunGrantLevels(AVeyraPlayerController& Requester, TConstArrayView<FString> Args)
	{
		const TOptional<int32> Levels = VeyraDevCommands::ParseWhole(Args, 0);
		if (!Levels.IsSet() || Levels.GetValue() <= 0 || Args.Num() != 1)
		{
			return VeyraDevCommands::UsageReply(TEXT("GrantLevels"));
		}
		UVeyraProgressionComponent* Progression = ChangeableProgression(Requester);
		if (!Progression)
		{
			return NoProgression;
		}
		// The XP from here to the level Levels above this one, or to the cap.
		const FVeyraProgressionTuning& Tuning = UVeyraProgressionTuningSubsystem::Get();
		const int32 Before = Progression->GetLevel();
		const int32 Target = FMath::Min(Before + Levels.GetValue(), Tuning.MaxLevel);
		double Needed = -Progression->GetExperience();
		for (int32 Level = Before; Level < Target; ++Level)
		{
			Needed += VeyraProgression::ExperienceToNextLevel(Level, Tuning);
		}
		if (Needed > 0.0)
		{
			Progression->AddExperience(Needed);
		}
		return FString::Printf(TEXT("Level %d -> %d."), Before, Progression->GetLevel());
	}

	FString RunGiveItem(AVeyraPlayerController& Requester, TConstArrayView<FString> Args)
	{
		if (Args.Num() != 1)
		{
			return VeyraDevCommands::UsageReply(TEXT("GiveItem"));
		}
		const TOptional<FVeyraContentId> Item = FVeyraContentId::FromText(Args[0].ToLower());
		if (!Item.IsSet() || !UVeyraItemsTuningSubsystem::FindItem(Item.GetValue()))
		{
			return FString::Printf(TEXT("There is no item \"%s\"; Veyra.Dev.Items lists them."), *Args[0]);
		}
		AVeyraPlayerState* Participant = VeyraDevCommands::ParticipantOf(Requester);
		UVeyraShopSubsystem* Shop = Requester.GetWorld()->GetSubsystem<UVeyraShopSubsystem>();
		if (!Participant || !Shop)
		{
			return TEXT("You have no inventory yet.");
		}
		const EVeyraShopRefusal Refusal = Shop->GrantItem(*Participant, Item.GetValue());
		return Refusal == EVeyraShopRefusal::None ? FString::Printf(TEXT("Gave you %s."), *Item->ToString())
												  : FString::Printf(TEXT("No %s: %s."), *Item->ToString(), LexToString(Refusal));
	}

	void RunItems(UWorld* /*World*/, TConstArrayView<FString> Args, FOutputDevice& Output)
	{
		const FString Filter = Args.IsEmpty() ? FString() : Args[0];
		TArray<TPair<FVeyraContentId, const FVeyraItemDefinition*>> Listed;
		for (const TPair<FVeyraContentId, FVeyraItemDefinition>& Entry : UVeyraItemsTuningSubsystem::Get().Items)
		{
			if (Filter.IsEmpty() || Entry.Key.ToString().Contains(Filter))
			{
				Listed.Emplace(Entry.Key, &Entry.Value);
			}
		}
		Listed.Sort([](const TPair<FVeyraContentId, const FVeyraItemDefinition*>& A, const TPair<FVeyraContentId, const FVeyraItemDefinition*>& B) {
			return A.Value->Tier != B.Value->Tier ? A.Value->Tier < B.Value->Tier : A.Key.ToString() < B.Key.ToString();
		});
		Output.Logf(TEXT("%d item(s), for Veyra.Dev.GiveItem <item-id>:"), Listed.Num());
		for (const TPair<FVeyraContentId, const FVeyraItemDefinition*>& Entry : Listed)
		{
			Output.Logf(TEXT("  tier %d  %s  (%s)"), Entry.Value->Tier, *Entry.Key.ToString(),
				*StaticEnum<EVeyraItemCategory>()->GetNameStringByValue(static_cast<int64>(Entry.Value->Category)));
		}
	}
}

void VeyraDevCommands::AddEconomyCommands(TArray<FVeyraDevCommand>& Out)
{
	using namespace VeyraDevEconomyCommands;
	Out.Add(FVeyraDevCommand::Server(TEXT("Gold"), EconomyCategory, TEXT("<amount>"), TEXT("Gives you this much Gold."), &RunGold));
	Out.Add(FVeyraDevCommand::Server(TEXT("GrantXp"), EconomyCategory, TEXT("<amount>"), TEXT("Gives your Vanguard this much XP."), &RunGrantXp));
	Out.Add(FVeyraDevCommand::Server(TEXT("GrantLevels"), EconomyCategory, TEXT("<levels>"),
		TEXT("Gives your Vanguard the XP for this many more levels, up to the cap."), &RunGrantLevels));
	Out.Add(FVeyraDevCommand::Server(TEXT("GiveItem"), ItemsCategory, TEXT("<item-id>"),
		TEXT("Puts the item in your inventory now, for free, by the shop's slot and recipe rules."), &RunGiveItem));
	Out.Add(FVeyraDevCommand::Local(TEXT("Items"), ItemsCategory, TEXT("[filter]"), TEXT("Lists the item ids GiveItem takes, by tier."), &RunItems));
}
