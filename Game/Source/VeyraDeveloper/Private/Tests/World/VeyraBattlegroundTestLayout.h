// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Movement/VeyraUnitCollision.h"
#include "Terrain/VeyraGround.h"
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
		// The committed floor's size: what the committed tuning places that a test does not override, as the Flux Wells,
		// still has ground under it.
		Layout.HalfExtent = UVeyraWorldTuningSubsystem::Get().Layout.HalfExtent;
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
		Layout.Base.FountainRadius = 300.0;
		// The keep's floor round the base, open to the lane; no masses, walls or rim, so nothing blocks the fixture.
		Layout.Keep.Floor = { { -3000.0, -3000.0 }, { -1650.0, -3000.0 }, { -1650.0, -1650.0 }, { -3000.0, -1650.0 } };
		return Layout;
	}

	/** Real collision ground for tests that previously relied on implicit Z=0. Fixture dimensions only. */
	inline UBoxComponent* SpawnCompactGround(UWorld& World)
	{
		const double HalfExtent = CompactBattleground().HalfExtent;
		AActor* Floor = World.SpawnActor<AActor>();
		UBoxComponent* Box = NewObject<UBoxComponent>(Floor);
		Floor->SetRootComponent(Box);
		Box->SetBoxExtent(FVector(HalfExtent, HalfExtent, 50.0));
		VeyraGround::MakeGround(*Box);
		// These rule fixtures query ground but intentionally place combatants at arbitrary heights.
		VeyraUnitCollision::SetResponseToUnits(*Box, ECR_Ignore);
		Box->RegisterComponent();
		Floor->SetActorLocation(FVector(0.0, 0.0, -50.0));
		return Box;
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
