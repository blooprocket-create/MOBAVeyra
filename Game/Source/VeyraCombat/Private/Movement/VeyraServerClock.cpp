// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Movement/VeyraServerClock.h"

#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Tuning/VeyraCombatTuningSubsystem.h"

void FVeyraClockEstimate::AddExchange(double AskedAt, double ServerTime, double ReceivedAt, const FVeyraMotionTuning& Tuning)
{
	const double RoundTrip = ReceivedAt - AskedAt;
	if (!FMath::IsFinite(RoundTrip) || !FMath::IsFinite(ServerTime) || RoundTrip < 0.0)
	{
		return;
	}
	// The server read its clock about halfway through the round trip.
	Latest.Add({ RoundTrip, ServerTime + RoundTrip / 2.0 - ReceivedAt });
	while (Latest.Num() > Tuning.ClockExchangesKept)
	{
		Latest.RemoveAt(0);
	}
	// The quickest waited least on its way; of equally quick ones, the latest knows the clock best.
	const FExchange* Trusted = &Latest[0];
	for (const FExchange& Exchange : Latest)
	{
		if (Exchange.RoundTrip <= Trusted->RoundTrip)
		{
			Trusted = &Exchange;
		}
	}
	TrustedOffset = Trusted->Offset;
	TrustedRoundTrip = Trusted->RoundTrip;
	// The first estimate, or one far from the trusted exchange, is taken at once.
	if (!bHasEstimate || FMath::Abs(TrustedOffset - Offset) > Tuning.ClockSnapSeconds)
	{
		Offset = TrustedOffset;
	}
	bHasEstimate = true;
}

void FVeyraClockEstimate::Advance(double DeltaSeconds, const FVeyraMotionTuning& Tuning)
{
	if (bHasEstimate && DeltaSeconds > 0.0)
	{
		Offset = FMath::FInterpConstantTo(Offset, TrustedOffset, DeltaSeconds, Tuning.ClockSlewRate);
	}
}

UVeyraServerClock* UVeyraServerClock::Get(const UWorld* World)
{
	return World ? World->GetSubsystem<UVeyraServerClock>() : nullptr;
}

bool UVeyraServerClock::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

bool UVeyraServerClock::IsServerClock() const
{
	const UWorld* World = GetWorld();
	return World && World->GetNetMode() != NM_Client;
}

double UVeyraServerClock::Now() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.0;
	}
	if (IsServerClock())
	{
		return World->GetTimeSeconds();
	}
	if (Estimate.HasEstimate())
	{
		return World->GetTimeSeconds() + Estimate.GetOffset();
	}
	// Before its first exchange, the game state's coarser reading stands in.
	const AGameStateBase* GameState = World->GetGameState();
	return GameState ? GameState->GetServerWorldTimeSeconds() : World->GetTimeSeconds();
}

bool UVeyraServerClock::IsKnown() const
{
	return IsServerClock() || Estimate.HasEstimate();
}

double UVeyraServerClock::GetRoundTrip() const
{
	return IsServerClock() ? 0.0 : Estimate.GetRoundTrip();
}

void UVeyraServerClock::AddExchange(double AskedAt, double ServerTime)
{
	const UWorld* World = GetWorld();
	// A paused world's clock stands still on both machines, so an exchange then times nothing.
	if (World && !World->IsPaused() && !IsServerClock())
	{
		Estimate.AddExchange(AskedAt, ServerTime, World->GetTimeSeconds(), UVeyraCombatTuningSubsystem::Get().Motion);
	}
}

void UVeyraServerClock::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!IsServerClock())
	{
		Estimate.Advance(DeltaTime, UVeyraCombatTuningSubsystem::Get().Motion);
	}
}

TStatId UVeyraServerClock::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UVeyraServerClock, STATGROUP_Tickables);
}
