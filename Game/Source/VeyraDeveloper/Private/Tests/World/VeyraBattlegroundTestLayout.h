// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Tuning/VeyraWorldTuning.h"
#include "Tuning/VeyraWorldTuningSubsystem.h"

namespace VeyraWorldTests
{
	/**
	 * A battleground small enough for tests (ADR-011 §15): one mid lane with every structure kind
	 * close together, Team B's the rotation of Team A's. Fixture values, not tuning.
	 */
	inline FVeyraBattlegroundLayout CompactBattleground()
	{
		FVeyraBattlegroundLayout Layout;
		Layout.HalfExtent = 3000.0;
		Layout.Surface = UVeyraWorldTuningSubsystem::Get().Layout.Surface;
		Layout.Terrain = UVeyraWorldTuningSubsystem::Get().Layout.Terrain;
		// A straight river from the centre off the floor, its rotation the other way; no islands.
		Layout.River = UVeyraWorldTuningSubsystem::Get().Layout.River;
		Layout.River.Main = { { 0.0, 0.0, 400.0 }, { 3200.0, -3200.0, 400.0 } };
		Layout.River.Islands.Reset();
		FVeyraLaneLayout& Lane = Layout.Lanes.AddDefaulted_GetRef();
		Lane.Lane = EVeyraLane::Mid;
		Lane.Points = { { -1500.0, -1500.0 }, { 1500.0, 1500.0 } };
		Lane.Width = 400.0;
		Lane.InhibitorDistance = 0.0;
		Lane.SpireDistances = { 400.0, 700.0, 1000.0 };
		// Clear of the inhibitor behind it and the inner Spire ahead.
		Lane.FluxbornSpawnDistance = 220.0;
		Layout.Base.PrimeWell = { -2400.0, -2400.0 };
		Layout.Base.BaseTowers = { { -1800.0, -2300.0 }, { -2300.0, -1800.0 } };
		Layout.Base.Fountain = { -2750.0, -2750.0 };
		Layout.Base.PadRadius = 800.0;
		Layout.Base.FountainRadius = 300.0;
		return Layout;
	}

	/** Real collision ground for tests that previously relied on implicit Z=0. Fixture dimensions only. */
	inline void SpawnCompactGround(UWorld& World)
	{
		const double HalfExtent = CompactBattleground().HalfExtent;
		AActor* Floor = World.SpawnActor<AActor>();
		UBoxComponent* Box = NewObject<UBoxComponent>(Floor);
		Floor->SetRootComponent(Box);
		Box->SetBoxExtent(FVector(HalfExtent, HalfExtent, 50.0));
		Box->SetCollisionObjectType(ECC_WorldStatic);
		Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Box->SetCollisionResponseToAllChannels(ECR_Block);
		// These rule fixtures query ground but intentionally place combatants at arbitrary heights.
		Box->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		Box->RegisterComponent();
		Floor->SetActorLocation(FVector(0.0, 0.0, -50.0));
	}

	/** World tuning a test may change, starting from the committed one. Get() returns it while this object lives. */
	struct FScopedWorldTuning
	{
		FVeyraWorldTuning Tuning;

		FScopedWorldTuning()
			: Tuning(UVeyraWorldTuningSubsystem::Get())
		{
			UVeyraWorldTuningSubsystem::SetTestOverride(&Tuning);
		}

		~FScopedWorldTuning()
		{
			UVeyraWorldTuningSubsystem::SetTestOverride(nullptr);
		}

		UE_NONCOPYABLE(FScopedWorldTuning);
	};
}
