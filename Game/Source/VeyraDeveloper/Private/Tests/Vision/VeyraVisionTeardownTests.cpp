// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/ActorTestSpawner.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Iris/ReplicationSystem/ReplicationSystem.h"
#include "Net/Iris/ReplicationSystem/ReplicationSystemUtil.h"
#include "VeyraVisionSubsystem.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraVisionTests
{
	// Veyra.Vision.GateTeardown.*: a game engine shuts the world's net driver down, and Iris's replication
	// system with it, before it cleans the world up and its subsystems deinitialize (UGameEngine::PreExit).
	// Play In Editor does it the other way round, so only a hosted server met this order: Vision stops
	// after the fog gate's groups are gone, and must not reach into the destroyed system.
	TEST_CLASS(GateTeardown, "Veyra.Vision")
	{
		FActorTestSpawner Spawner;

		AFTER_EACH()
		{
			// A test that stopped early leaves the world listening.
			GEngine->ShutdownWorldNetDriver(&Spawner.GetWorld());
		}

		TEST_METHOD(VisionStopsAfterItsWorldsNetDriverShutDown)
		{
			UWorld& World = Spawner.GetWorld();
			FURL Url;
			ASSERT_THAT(IsTrue(World.Listen(Url), TEXT("the world listens, as a match server's does")));
			UReplicationSystem* Replication = UE::Net::FReplicationSystemUtil::GetReplicationSystem(&World);
			ASSERT_THAT(IsNotNull(Replication, TEXT("Iris replicates it")));
			UVeyraVisionSubsystem& Vision = *World.GetSubsystem<UVeyraVisionSubsystem>();
			Vision.Start();
			ASSERT_THAT(IsTrue(Replication->FindGroup(TEXT("Veyra.Side.A")).IsValid() && Replication->FindGroup(TEXT("Veyra.Side.B")).IsValid(),
				TEXT("the fog gate is up")));

			// UGameEngine::PreExit's order: the net driver goes first, and Iris with it.
			GEngine->ShutdownWorldNetDriver(&World);
			ASSERT_THAT(IsNull(UE::Net::FReplicationSystemUtil::GetReplicationSystem(&World)));
			// A pass that comes after gates nothing; stopping, as the world's cleanup does, forgets the groups.
			Vision.UpdateNow();
			Vision.Stop();
			ASSERT_THAT(IsFalse(Vision.IsStarted()));

			// Started again without a net driver, it works vision out ungated.
			Vision.Start();
			ASSERT_THAT(IsTrue(Vision.IsStarted()));
			Vision.Stop();
		}
	};
}

#endif
