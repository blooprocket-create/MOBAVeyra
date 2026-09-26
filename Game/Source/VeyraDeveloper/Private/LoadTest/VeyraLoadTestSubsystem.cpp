// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "LoadTest/VeyraLoadTestSubsystem.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "LoadTest/VeyraLaneStandIn.h"
#include "VeyraGameMode.h"
#include "VeyraGameState.h"
#include "VeyraPlayerState.h"
#include "VeyraTeamStart.h"
#include "VeyraVanguardController.h"

DEFINE_LOG_CATEGORY_STATIC(LogVeyraLoadTest, Log, All);

namespace
{
	// Load-test fixture values, not tuning: three lanes this far apart, walked between these
	// fractions of the way from one side's start to the other's, at this speed; how often bots pick
	// a new destination; and a fixed seed so runs are comparable.
	constexpr int32 LaneCount = 3;
	constexpr double LaneSpacing = 500.0;
	constexpr double LaneEndFraction = 0.1;
	constexpr float StandInSpeed = 325.0f;
	constexpr double WanderIntervalSeconds = 3.0;
	constexpr int32 RandomSeed = 1;
	constexpr int32 SideCount = 2;

	const TCHAR* const BotsOption = TEXT("VeyraLoadBots=");
	const TCHAR* const StandInsOption = TEXT("VeyraLoadStandIns=");
	const TCHAR* const StandInHzOption = TEXT("VeyraLoadStandInHz=");

	int32 ReadCount(const UWorld& World, const TCHAR* Option)
	{
		const TCHAR* Value = World.URL.GetOption(Option, nullptr);
		return Value ? FMath::Max(0, FCString::Atoi(Value)) : 0;
	}
}

bool UVeyraLoadTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
}

void UVeyraLoadTestSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	BotCount = ReadCount(InWorld, BotsOption);
	StandInCount = ReadCount(InWorld, StandInsOption);
	StandInUpdateHz = ReadCount(InWorld, StandInHzOption);
	Random.Initialize(RandomSeed);
	if (IsTickable())
	{
		UE_LOG(LogVeyraLoadTest, Display, TEXT("VeyraLoadTest: will add %d bot(s) and %d lane stand-in(s) when preparation begins, stand-ins updating at %s."),
			BotCount, StandInCount, StandInUpdateHz > 0 ? *FString::Printf(TEXT("%d Hz"), StandInUpdateHz) : TEXT("the engine's default rate"));
	}
}

void UVeyraLoadTestSubsystem::Tick(float /*DeltaTime*/)
{
	UWorld* World = GetWorld();
	const AVeyraGameState* GameState = World ? World->GetGameState<AVeyraGameState>() : nullptr;
	if (!GameState || GameState->GetPhase() < EVeyraMatchPhase::Preparation)
	{
		return;
	}
	if (!bPopulated)
	{
		bPopulated = Populate(*World);
		return;
	}
	if (GameState->GetPhase() == EVeyraMatchPhase::Live && World->GetTimeSeconds() >= NextWanderTime)
	{
		NextWanderTime = World->GetTimeSeconds() + WanderIntervalSeconds;
		Wander(*World);
	}
}

bool UVeyraLoadTestSubsystem::Populate(UWorld& World)
{
	int32 StartsFound = 0;
	for (TActorIterator<AVeyraTeamStart> It(&World); It; ++It)
	{
		(It->GetVeyraTeam() == EVeyraTeam::A ? StartA : StartB) = It->GetActorLocation();
		++StartsFound;
	}
	AVeyraGameMode* GameMode = World.GetAuthGameMode<AVeyraGameMode>();
	if (!GameMode || StartsFound < SideCount)
	{
		UE_LOG(LogVeyraLoadTest, Error, TEXT("VeyraLoadTest: needs the Veyra game mode and a start for each side."));
		BotCount = 0;
		StandInCount = 0;
		return true;
	}

	for (int32 Index = 0; Index < BotCount; ++Index)
	{
		if (AVeyraPlayerState* Bot = GameMode->AddBotParticipant(FString::Printf(TEXT("LoadBot%d"), Index + 1)))
		{
			Bots.Add(Bot);
		}
	}

	// Lanes run from one side's start to the other's, offset sideways. Each stand-in starts at an
	// even spacing along its lane and walks toward the far side, as the two sides' waves would.
	const FVector Along = StartB - StartA;
	const FVector Across = FVector(-Along.Y, Along.X, 0.0).GetSafeNormal();
	const FVector LaneA = StartA + Along * LaneEndFraction;
	const FVector LaneB = StartA + Along * (1.0 - LaneEndFraction);
	const int32 PerLane = FMath::DivideAndRoundUp(StandInCount, LaneCount);
	int32 Spawned = 0;
	for (int32 Index = 0; Index < StandInCount; ++Index)
	{
		const int32 Lane = Index % LaneCount;
		const int32 Slot = Index / LaneCount;
		const FVector Offset = Across * LaneSpacing * (Lane - (LaneCount - 1) / 2.0);
		const FVector From = (Slot % SideCount == 0 ? LaneA : LaneB) + Offset;
		const FVector To = (Slot % SideCount == 0 ? LaneB : LaneA) + Offset;
		const FVector Location = FMath::Lerp(From, To, (Slot + 0.5) / PerLane);

		FActorSpawnParameters Parameters;
		Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		if (AVeyraLaneStandIn* StandIn = World.SpawnActor<AVeyraLaneStandIn>(Location, FRotator::ZeroRotator, Parameters))
		{
			if (StandInUpdateHz > 0)
			{
				StandIn->SetNetUpdateFrequency(static_cast<float>(StandInUpdateHz));
			}
			StandIn->Walk(From, To, StandInSpeed);
			++Spawned;
		}
	}
	UE_LOG(LogVeyraLoadTest, Display, TEXT("VeyraLoadTest: added %d bot(s) and %d lane stand-in(s)."), Bots.Num(), Spawned);
	return true;
}

void UVeyraLoadTestSubsystem::Wander(UWorld& /*World*/)
{
	const FVector Along = StartB - StartA;
	const FVector Across = FVector(-Along.Y, Along.X, 0.0).GetSafeNormal();
	for (const AVeyraPlayerState* Bot : Bots)
	{
		AVeyraVanguardController* Controller = Bot ? Bot->GetVanguardController() : nullptr;
		if (Controller && Controller->GetPawn())
		{
			const FVector Destination = StartA + Along * Random.FRandRange(LaneEndFraction, 1.0 - LaneEndFraction)
				+ Across * LaneSpacing * Random.FRandRange(-1.0f, 1.0f);
			Controller->MoveToDestination(Destination);
		}
	}
}

TStatId UVeyraLoadTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UVeyraLoadTestSubsystem, STATGROUP_Tickables);
}
