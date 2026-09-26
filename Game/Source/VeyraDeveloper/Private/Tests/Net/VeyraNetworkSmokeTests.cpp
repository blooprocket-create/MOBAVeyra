// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"

#if ENABLE_PIE_NETWORK_TEST

#include "Engine/GameInstance.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "Tests/Net/VeyraNetTestActor.h"
#include "Tests/Net/VeyraNetTestHelpers.h"

namespace VeyraNetTests
{
	// Veyra.Net.NetworkSmoke.*: a dedicated server and two clients in one editor process, all on
	// Iris (ADR-006 §5, §10). Every Veyra.Net test builds on this setup.
	NETWORK_TEST_CLASS(NetworkSmoke, "Veyra.Net")
	{
		struct FState : public FBasePIENetworkComponentState
		{
			AVeyraNetTestActor* Actor = nullptr;
		};

		FPIENetworkComponent<FState> Network{ TestRunner, TestCommandBuilder, bInitializing };

		BEFORE_ALL()
		{
			LoadOnDemandEngineModules();
		}

		BEFORE_EACH()
		{
			IgnoreLoginViewTargetRpc(*TestRunner);
			FNetworkComponentBuilder<FState>()
				.WithGameInstanceClass(UGameInstance::StaticClass())
				.WithGameMode(AGameModeBase::StaticClass())
				.Build(Network);
		}

		TEST_METHOD(ServerAndClientsReplicateWithIris)
		{
			Network
				.ThenServer([this](FState& State) {
					ASSERT_THAT(AreEqual(State.ClientConnections.Num(), State.ClientCount));
					const UNetDriver* Driver = State.World->GetNetDriver();
					ASSERT_THAT(IsNotNull(Driver));
					ASSERT_THAT(IsTrue(Driver->IsUsingIrisReplication()));
				})
				.ThenClients([this](FState& State) {
					const UNetDriver* Driver = State.World->GetNetDriver();
					ASSERT_THAT(IsNotNull(Driver));
					ASSERT_THAT(IsTrue(Driver->IsUsingIrisReplication()));
				});
		}

		TEST_METHOD(PushModelPropertyReachesEveryClient)
		{
			constexpr int32 SentValue = 7;
			Network.SpawnAndReplicate<AVeyraNetTestActor, &FState::Actor>()
				.ThenServer([](FState& State) { State.Actor->SetValue(SentValue); })
				.UntilClients([](FState& State) { return State.Actor->GetValue() == SentValue; });
		}
	};
}

#endif // ENABLE_PIE_NETWORK_TEST
