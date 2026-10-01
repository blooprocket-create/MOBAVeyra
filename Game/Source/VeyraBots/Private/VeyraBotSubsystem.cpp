// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "VeyraBotSubsystem.h"

#include "Bots/VeyraMatchEvents.h"
#include "Brain/VeyraBotBrainComponent.h"
#include "Brain/VeyraBotRoles.h"
#include "Engine/World.h"
#include "Tuning/VeyraBotsTuningSubsystem.h"
#include "VeyraBotsLog.h"
#include "VeyraGameMode.h"
#include "VeyraGameState.h"
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
	FSeated& Entry = Seated.FindOrAdd(Seat.Side).Add_GetRef(FSeated{ &Bot, Brain, Seat.Vanguard, Seat.Difficulty, Seat.Seat });
	// Its own seat's place first, so even a bot seated in play has one; later seats than the list wrap around it.
	Place(Entry, Seat.Seat % Seats.Num());
	Brain->RegisterComponent();
	// Then its team deals its places again, by the roles their Vanguards play (ADR-038 §5).
	Deal(Seat.Side);
}

void UVeyraBotSubsystem::Place(FSeated& Each, int32 PlaceSeat)
{
	const FVeyraBotsTuning& Tuning = UVeyraBotsTuningSubsystem::Get();
	const FVeyraBotSeatTuning& Spot = Tuning.Seats[PlaceSeat];
	AVeyraPlayerState& Bot = *Each.Bot;
	// It takes its place's Flux Spells, as a player takes theirs from champion select (ADR-015 §8).
	if (AVeyraGameMode* GameMode = GetWorld()->GetAuthGameMode<AVeyraGameMode>())
	{
		GameMode->EquipStartingFluxSpells(Bot, Spot.FluxSpells);
	}
	const bool bWards = Tuning.Warding.Seats.Contains(PlaceSeat);
	Each.Brain->Configure(Bot, Spot.Role, Each.Difficulty, bWards, static_cast<int32>(HashCombine(GetTypeHash(Bot.GetPlayerId()), GetTypeHash(Each.Seat))));
	Each.PlaceSeat = PlaceSeat;
	UE_LOG(LogVeyraBots, Log, TEXT("%s plays %s as a %s bot."), *Bot.GetPlayerName(), *StaticEnum<EVeyraBotRole>()->GetNameStringByValue(static_cast<int64>(Spot.Role)),
		LexToString(Each.Difficulty));
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
	// Only those not yet in play. Before the match goes live none is, though each spawns at the fountain as it is
	// seated in preparation; after, a bot whose Vanguard has spawned keeps its Flux Spells and its place.
	Team->RemoveAll([](const FSeated& Each) { return !Each.Bot.IsValid() || !Each.Brain.IsValid(); });
	const AVeyraGameState* GameState = GetWorld()->GetGameState<AVeyraGameState>();
	const bool bBeforeLive = !GameState || GameState->GetPhase() < EVeyraMatchPhase::Live;
	TArray<FSeated*> Waiting;
	for (FSeated& Each : *Team)
	{
		if (bBeforeLive || !Each.Bot->GetPawn())
		{
			Waiting.Add(&Each);
		}
	}
	TArray<EVeyraBotRole> Places;
	TArray<TArray<EVeyraBotRole>> Preferences;
	for (const FSeated* Each : Waiting)
	{
		Places.Add(Seats[Each->Seat % Seats.Num()].Role);
		const FVeyraBotVanguardTuning* Vanguard = Tuning.Vanguards.Find(Each->Vanguard);
		Preferences.Add(Vanguard ? Vanguard->Roles : TArray<EVeyraBotRole>());
	}
	const TArray<int32> PlaceOf = VeyraBotRoles::Deal(Places, Preferences);
	for (int32 Index = 0; Index < Waiting.Num(); ++Index)
	{
		FSeated& Each = *Waiting[Index];
		const int32 Dealt = PlaceOf.IsValidIndex(Index) && PlaceOf[Index] != INDEX_NONE ? PlaceOf[Index] : Index;
		const int32 PlaceSeat = Waiting[Dealt]->Seat % Seats.Num();
		if (PlaceSeat != Each.PlaceSeat)
		{
			Place(Each, PlaceSeat);
		}
	}
}
