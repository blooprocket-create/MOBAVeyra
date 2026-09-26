// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Stats/VeyraNetStatsSubsystem.h"

#include "Engine/DemoNetDriver.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/PlatformTime.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

DEFINE_LOG_CATEGORY_STATIC(LogVeyraNetStats, Log, All);

namespace
{
	const TCHAR* const IntervalOption = TEXT("VeyraNetStats=");
	constexpr double BytesPerKilobyte = 1024.0;
	constexpr double MillisecondsPerSecond = 1000.0;
}

bool UVeyraNetStatsSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// Whether it is switched on is only known from the map URL, once the world begins play.
	return IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
}

void UVeyraNetStatsSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	const TCHAR* FromUrl = InWorld.URL.GetOption(IntervalOption, nullptr);
	double Seconds = FromUrl ? FCString::Atod(FromUrl) : 0.0;
	if (!FromUrl)
	{
		FParse::Value(FCommandLine::Get(), IntervalOption, Seconds);
	}
	if (Seconds > 0.0)
	{
		IntervalSeconds = Seconds;
		WindowStartRealTime = FPlatformTime::Seconds();
		UE_LOG(LogVeyraNetStats, Display, TEXT("VeyraNetStats: logging every %g s."), IntervalSeconds);
	}
}

void UVeyraNetStatsSubsystem::Tick(float /*DeltaTime*/)
{
	// Frame time minus the wait for the next tick: the work the server did this frame.
	const double Busy = FMath::Max(0.0, FApp::GetDeltaTime() - FApp::GetIdleTime());
	++Frames;
	BusySecondsSum += Busy;
	BusySecondsMax = FMath::Max(BusySecondsMax, Busy);

	const double Now = FPlatformTime::Seconds();
	if (Now - WindowStartRealTime >= IntervalSeconds)
	{
		Report(Now - WindowStartRealTime);
		WindowStartRealTime = Now;
		Frames = 0;
		BusySecondsSum = 0.0;
		BusySecondsMax = 0.0;
	}
}

void UVeyraNetStatsSubsystem::Report(double WindowSeconds)
{
	const UWorld* World = GetWorld();
	const UNetDriver* Game = World ? World->GetNetDriver() : nullptr;
	const UDemoNetDriver* Replay = World ? World->GetDemoNetDriver() : nullptr;

	// Summed from the connections: each updates its own rates every second, while the drivers'
	// totals stay at zero here.
	int32 Clients = 0;
	int64 GameOut = 0;
	int64 GameIn = 0;
	int32 ClientOutMax = 0;
	if (Game)
	{
		for (const UNetConnection* Connection : Game->ClientConnections)
		{
			if (Connection)
			{
				++Clients;
				GameOut += Connection->OutBytesPerSecond;
				GameIn += Connection->InBytesPerSecond;
				ClientOutMax = FMath::Max(ClientOutMax, Connection->OutBytesPerSecond);
			}
		}
	}
	int64 ReplayOut = 0;
	if (Replay)
	{
		for (const UNetConnection* Connection : Replay->ClientConnections)
		{
			ReplayOut += Connection ? Connection->OutBytesPerSecond : 0;
		}
	}

	int32 ReplicatedActors = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		ReplicatedActors += It->GetIsReplicated() ? 1 : 0;
	}

	UE_LOG(LogVeyraNetStats, Display,
		TEXT("VeyraNetStats: window=%.1fs frames=%d busyAvgMs=%.2f busyMaxMs=%.2f clients=%d gameOutKBps=%.2f perClientOutKBps=%.2f maxClientOutKBps=%.2f gameInKBps=%.2f replayRecording=%d replayOutKBps=%.2f replicatedActors=%d"),
		WindowSeconds, Frames,
		Frames > 0 ? BusySecondsSum / Frames * MillisecondsPerSecond : 0.0, BusySecondsMax * MillisecondsPerSecond,
		Clients, GameOut / BytesPerKilobyte, Clients > 0 ? GameOut / BytesPerKilobyte / Clients : 0.0, ClientOutMax / BytesPerKilobyte,
		GameIn / BytesPerKilobyte, Replay && Replay->IsRecording() ? 1 : 0, ReplayOut / BytesPerKilobyte, ReplicatedActors);
}

TStatId UVeyraNetStatsSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UVeyraNetStatsSubsystem, STATGROUP_Tickables);
}
