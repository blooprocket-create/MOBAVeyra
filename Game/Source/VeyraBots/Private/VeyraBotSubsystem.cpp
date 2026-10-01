// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "VeyraBotSubsystem.h"

#include "Bots/VeyraMatchEvents.h"
#include "Brain/VeyraBotBrainComponent.h"
#include "Brain/VeyraBotRoles.h"
#include "Engine/World.h"
#include "Tuning/VeyraBotsTuningSubsystem.h"
#include "VeyraBotsLog.h"
#include "VeyraGameMode.h"
#include "VeyraPlayerState.h"
#include "VeyraVanguardController.h"

void UVeyraBotSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UVeyraMatchEvents* Events = Collection.InitializeDependency<UVeyraMatchEvents>();
	if (Events)
	{
		BotAddedHandle = Events->OnBotAdded.AddUObject(this, &UVeyraBotSubsystem::OnBotAdded);
	}
}

void UVeyraBotSubsystem::Deinitialize()
{
	if (UVeyraMatchEvents* Events = GetWorld() ? GetWorld()->GetSubsystem<UVeyraMatchEvents>() : nullptr)
	{
		Events->OnBotAdded.Remove(BotAddedHandle);
	}
	Seated.Reset();
	Super::Deinitialize();
}

void UVeyraBotSubsystem::OnBotAdded(AVeyraPlayerState& Bot, const FVeyraBotSeat& Seat)
{
	AVeyraVanguardController* Controller = Bot.GetVanguardController();
	const TArray<FVeyraBotSeatTuning>& Seats = UVeyraBotsTuningSubsystem::Get().Seats;
	if (!Controller || Seats.IsEmpty())
	{
		return;
	}
	UVeyraBotBrainComponent* Brain = NewObject<UVeyraBotBrainComponent>(Controller);
	Brain->RegisterComponent();
	Seated.FindOrAdd(Seat.Side).Add(FSeated{ &Bot, Brain, Seat.Vanguard, Seat.Difficulty, Seat.Seat });
	Deal(Seat.Side);
}

void UVeyraBotSubsystem::Deal(EVeyraTeam Side)
{
	const FVeyraBotsTuning& Tuning = UVeyraBotsTuningSubsystem::Get();
	const TArray<FVeyraBotSeatTuning>& Seats = Tuning.Seats;
	TArray<FSeated>* Team = Seated.Find(Side);
	if (!Team || Seats.IsEmpty())
	{
		return;
	}
	// Only those not yet in play: a bot whose Vanguard has spawned keeps its Flux Spells and its place.
	Team->RemoveAll([](const FSeated& Each) { return !Each.Bot.IsValid() || !Each.Brain.IsValid(); });
	TArray<const FSeated*> Waiting;
	for (const FSeated& Each : *Team)
	{
		if (!Each.Bot->GetPawn())
		{
			Waiting.Add(&Each);
		}
	}
	// Later seats than the list wrap around it.
	TArray<EVeyraBotRole> Places;
	TArray<TArray<EVeyraBotRole>> Preferences;
	for (const FSeated* Each : Waiting)
	{
		Places.Add(Seats[Each->Seat % Seats.Num()].Role);
		const FVeyraBotVanguardTuning* Vanguard = Tuning.Vanguards.Find(Each->Vanguard);
		Preferences.Add(Vanguard ? Vanguard->Roles : TArray<EVeyraBotRole>());
	}
	const TArray<int32> PlaceOf = VeyraBotRoles::Deal(Places, Preferences);
	AVeyraGameMode* GameMode = GetWorld()->GetAuthGameMode<AVeyraGameMode>();
	for (int32 Index = 0; Index < Waiting.Num(); ++Index)
	{
		const FSeated& Each = *Waiting[Index];
		const int32 PlaceSeat = Waiting[PlaceOf.IsValidIndex(Index) && PlaceOf[Index] != INDEX_NONE ? PlaceOf[Index] : Index]->Seat % Seats.Num();
		const FVeyraBotSeatTuning& Place = Seats[PlaceSeat];
		AVeyraPlayerState& Bot = *Each.Bot;
		// It takes its place's Flux Spells, as a player takes theirs from champion select (ADR-015 §8).
		if (GameMode)
		{
			GameMode->EquipStartingFluxSpells(Bot, Place.FluxSpells);
		}
		const bool bWards = Tuning.Warding.Seats.Contains(PlaceSeat);
		Each.Brain->Configure(Bot, Place.Role, Each.Difficulty, bWards, static_cast<int32>(HashCombine(GetTypeHash(Bot.GetPlayerId()), GetTypeHash(Each.Seat))));
		UE_LOG(LogVeyraBots, Log, TEXT("%s plays %s as a %s bot."), *Bot.GetPlayerName(), *StaticEnum<EVeyraBotRole>()->GetNameStringByValue(static_cast<int64>(Place.Role)),
			LexToString(Each.Difficulty));
	}
}
