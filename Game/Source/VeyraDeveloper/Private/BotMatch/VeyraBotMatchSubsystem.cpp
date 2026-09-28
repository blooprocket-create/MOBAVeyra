// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "BotMatch/VeyraBotMatchSubsystem.h"

#include "AbilitySystemComponent.h"
#include "Attributes/VeyraVitalsSet.h"
#include "Brain/VeyraBotBrainComponent.h"
#include "Engine/World.h"
#include "Gold/VeyraGoldComponent.h"
#include "Inventory/VeyraInventoryComponent.h"
#include "Progression/VeyraProgressionComponent.h"
#include "Structures/VeyraStructure.h"
#include "Tuning/VeyraBotsTuningSubsystem.h"
#include "VeyraBattlegroundSubsystem.h"
#include "VeyraGameMode.h"
#include "VeyraGameState.h"
#include "VeyraPlayerState.h"
#include "VeyraVanguardController.h"

DEFINE_LOG_CATEGORY_STATIC(LogVeyraBotMatch, Log, All);

namespace
{
	const TCHAR* const PlayingBotsOption = TEXT("VeyraPlayingBots=");
	const TCHAR* const BotDifficultyOption = TEXT("VeyraBotDifficulty=");
	// Harness settings, not tuning: how often the match's state is logged, in seconds of match clock.
	constexpr double BotMatchReportSeconds = 60.0;
}

bool UVeyraBotMatchSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return !UE_BUILD_SHIPPING && IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
}

void UVeyraBotMatchSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	const TCHAR* Count = InWorld.URL.GetOption(PlayingBotsOption, nullptr);
	BotCount = Count ? FMath::Max(0, FCString::Atoi(Count)) : 0;
	const TCHAR* Named = InWorld.URL.GetOption(BotDifficultyOption, nullptr);
	Difficulty = Named && FCString::Stricmp(Named, TEXT("Intermediate")) == 0 ? EVeyraBotDifficulty::Intermediate : EVeyraBotDifficulty::Beginner;
	if (BotCount > 0)
	{
		UE_LOG(LogVeyraBotMatch, Display, TEXT("VeyraBotMatch: will seat %d %s bot(s) when preparation begins."), BotCount, LexToString(Difficulty));
	}
}

void UVeyraBotMatchSubsystem::Tick(float /*DeltaTime*/)
{
	UWorld* World = GetWorld();
	const AVeyraGameState* GameState = World ? World->GetGameState<AVeyraGameState>() : nullptr;
	if (!GameState || GameState->GetPhase() < EVeyraMatchPhase::Preparation)
	{
		return;
	}
	if (!bSeated)
	{
		bSeated = true;
		Seat(*World);
		return;
	}
	if (GameState->GetPhase() == EVeyraMatchPhase::Live && GameState->GetMatchClockSeconds() >= NextReportAt)
	{
		NextReportAt = GameState->GetMatchClockSeconds() + BotMatchReportSeconds;
		Report(*World);
	}
}

void UVeyraBotMatchSubsystem::Seat(UWorld& World)
{
	AVeyraGameMode* GameMode = World.GetAuthGameMode<AVeyraGameMode>();
	const AVeyraGameState* GameState = World.GetGameState<AVeyraGameState>();
	if (!GameMode || !GameState)
	{
		return;
	}
	// The released Vanguards bots know, in a fixed order, taken in turn.
	TArray<FVeyraContentId> Vanguards;
	UVeyraBotsTuningSubsystem::Get().Vanguards.GenerateKeyArray(Vanguards);
	Vanguards.Sort([](const FVeyraContentId& A, const FVeyraContentId& B) { return A.ToString() < B.ToString(); });
	int32 Seated = 0;
	for (int32 Index = 0; Index < BotCount && !Vanguards.IsEmpty(); ++Index)
	{
		int32 OnA = 0;
		int32 OnB = 0;
		for (const APlayerState* Member : GameState->PlayerArray)
		{
			const AVeyraPlayerState* Participant = Cast<AVeyraPlayerState>(Member);
			OnA += Participant && Participant->GetVeyraTeam() == EVeyraTeam::A ? 1 : 0;
			OnB += Participant && Participant->GetVeyraTeam() == EVeyraTeam::B ? 1 : 0;
		}
		const EVeyraTeam Side = OnA < OnB ? EVeyraTeam::A : EVeyraTeam::B;
		Seated += GameMode->AddPlayingBot(FString::Printf(TEXT("Bot%d"), Index + 1), Side, Vanguards[Index % Vanguards.Num()], Difficulty) ? 1 : 0;
	}
	UE_LOG(LogVeyraBotMatch, Display, TEXT("VeyraBotMatch: seated %d of %d playing bot(s)."), Seated, BotCount);
}

void UVeyraBotMatchSubsystem::Report(UWorld& World) const
{
	const AVeyraGameState* GameState = World.GetGameState<AVeyraGameState>();
	const UVeyraBattlegroundSubsystem* Battleground = World.GetSubsystem<UVeyraBattlegroundSubsystem>();
	int32 StandingA = 0;
	int32 StandingB = 0;
	for (const AVeyraStructure* Structure : Battleground ? Battleground->GetStructures() : TArray<TObjectPtr<AVeyraStructure>>())
	{
		if (Structure && !Structure->IsDestroyed())
		{
			(Structure->GetVeyraTeam() == EVeyraTeam::A ? StandingA : StandingB) += 1;
		}
	}
	FString Participants;
	for (const APlayerState* Member : GameState->PlayerArray)
	{
		const AVeyraPlayerState* Participant = Cast<AVeyraPlayerState>(Member);
		const UVeyraProgressionComponent* Progression = Participant ? Participant->FindComponentByClass<UVeyraProgressionComponent>() : nullptr;
		const UVeyraInventoryComponent* Inventory = Participant ? Participant->FindComponentByClass<UVeyraInventoryComponent>() : nullptr;
		if (!Progression || !Inventory)
		{
			continue;
		}
		int32 Items = 0;
		for (const FVeyraInventorySlot& Slot : Inventory->GetSlots())
		{
			Items += Slot.IsEmpty() ? 0 : 1;
		}
		const UVeyraGoldComponent* Gold = Participant->FindComponentByClass<UVeyraGoldComponent>();
		Participants += FString::Printf(TEXT(" %s(L%d, %d items, %.0f Gold"), *Participant->GetPlayerName(), Progression->GetLevel(), Items, Gold ? Gold->GetGold() : 0.0);
		// For a bot, its Health and what it is doing, so a stalled one shows why.
		const AVeyraVanguardController* Controller = Participant->GetVanguardController();
		const UVeyraBotBrainComponent* Brain = Controller ? Controller->FindComponentByClass<UVeyraBotBrainComponent>() : nullptr;
		const UAbilitySystemComponent* AbilitySystem = Participant->GetAbilitySystemComponent();
		if (Brain && AbilitySystem)
		{
			const double MaxHealth = AbilitySystem->GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute());
			const double Health = MaxHealth > 0.0 ? AbilitySystem->GetNumericAttribute(UVeyraVitalsSet::GetHealthAttribute()) / MaxHealth : 0.0;
			Participants += FString::Printf(TEXT(", %.0f%% Health, %s"), Health * 100.0, Brain->GetLastIntent().Reason);
		}
		Participants += TEXT(")");
	}
	UE_LOG(LogVeyraBotMatch, Display, TEXT("VeyraBotMatch: at %.0f s, structures standing A %d B %d;%s."), GameState->GetMatchClockSeconds(), StandingA, StandingB,
		*Participants);
}

TStatId UVeyraBotMatchSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UVeyraBotMatchSubsystem, STATGROUP_Tickables);
}
