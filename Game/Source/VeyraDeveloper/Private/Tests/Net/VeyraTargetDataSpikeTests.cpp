// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Tests/Net/VeyraTargetDataSpike.h"

#include "AbilitySystemComponent.h"
#include "CQTest.h"
#include "Components/PIENetworkComponent.h"

UVeyraTargetDataSpikeAbility::UVeyraTargetDataSpikeAbility()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

#if ENABLE_PIE_NETWORK_TEST

#include "Tests/Net/VeyraMatchNetTestHelpers.h"
#include "Tests/Net/VeyraNetTestHelpers.h"
#include "VeyraPlayerState.h"

namespace VeyraNetTests
{
	// Veyra.Net.TargetData.*: the ADR-006 §5 spike on GAS client target data under Iris.
	//
	// - GAS will not start an ability from a client whose avatar is a simulated proxy. Under ADR-006
	//   §7 a Vanguard is possessed by a server-side controller, so its owning client holds it as a
	//   simulated proxy: no client-started activation works, predicted or not.
	// - Target data made of reflected properties crosses Iris intact when the client sends it through
	//   the ASC's server RPC.
	// - Not kept as a test, because its warning would appear in every development build: Iris ignores
	//   a struct's own NetSerialize. Without an Iris NetSerializer it sends the reflected properties
	//   and warns that it is "generating descriptor for struct ... that has custom serialization"; a
	//   field only NetSerialize wrote arrived as zero (ADR-006 §5 amendment).
	//
	// Client and server share one process here, so they share Iris's type tables. Tables that differ
	// between machines are not covered by this test.
	NETWORK_TEST_CLASS(TargetData, "Veyra.Net")
	{
		struct FState : public FBasePIENetworkComponentState
		{
		};

		/** What the server received. */
		struct FReceived
		{
			int32 Count = 0;
			int32 Entries = 0;
			FVector Point = FVector::ZeroVector;
			int32 PropertyMarker = 0;
		};

		FPIENetworkComponent<FState> Network{ TestRunner, TestCommandBuilder, bInitializing };
		TUniquePtr<FScopedExpectedPlayers> ExpectedPlayers;
		TUniquePtr<FScopedMatchTuning> Tuning;
		FVeyraGreyboxLayout Layout;
		FReceived Received;
		FDelegateHandle ReceivedHandle;

		// Fixture values.
		static constexpr double ShortPreparationSeconds = 0.1;
		static constexpr double SentX = 123.45;
		static constexpr double SentY = -67.89;
		static constexpr double SentZ = 10.0;
		static constexpr int32 SentPropertyMarker = 4242;

		BEFORE_EACH()
		{
			IgnoreLoginViewTargetRpc(*TestRunner);
			ASSERT_THAT(IsTrue(VeyraGreybox::LoadLayout(Layout).IsEmpty()));
			Tuning = MakeUnique<FScopedMatchTuning>();
			Tuning->Tuning.Phases.PreparationSeconds = ShortPreparationSeconds;
			ExpectedPlayers = MakeUnique<FScopedExpectedPlayers>(MatchClientCount);
			BuildMatchNetwork(Network);
		}

		AFTER_EACH()
		{
			Tuning.Reset();
			ExpectedPlayers.Reset();
		}

		static UAbilitySystemComponent* LocalAbilitiesOf(const UWorld* World)
		{
			const AVeyraPlayerController* Controller = LocalControllerOf(World);
			const AVeyraPlayerState* Participant = Controller ? Controller->GetPlayerState<AVeyraPlayerState>() : nullptr;
			return Participant ? Participant->GetAbilitySystemComponent() : nullptr;
		}

		static UAbilitySystemComponent& ServerAbilitiesOf(const FState& State, int32 ClientIndex)
		{
			return *ServerControllerOf(State, ClientIndex)->GetPlayerState<AVeyraPlayerState>()->GetAbilitySystemComponent();
		}

		static FGameplayAbilitySpecHandle SpikeHandleOf(const UAbilitySystemComponent* Abilities)
		{
			const FGameplayAbilitySpec* Spec = Abilities ? Abilities->FindAbilitySpecFromClass(UVeyraTargetDataSpikeAbility::StaticClass()) : nullptr;
			return Spec ? Spec->Handle : FGameplayAbilitySpecHandle();
		}

		static FGameplayAbilityTargetDataHandle MakeTargetData()
		{
			FGameplayAbilityTargetDataHandle Data;
			FVeyraSpikePropertyTargetData* ByProperties = new FVeyraSpikePropertyTargetData();
			ByProperties->Point = FVector_NetQuantize100(FVector(SentX, SentY, SentZ));
			ByProperties->Marker = SentPropertyMarker;
			Data.Add(ByProperties);
			return Data;
		}

		void Record(const FGameplayAbilityTargetDataHandle& Data)
		{
			++Received.Count;
			Received.Entries = Data.Num();
			for (int32 Index = 0; Index < Data.Num(); ++Index)
			{
				const FGameplayAbilityTargetData* Entry = Data.Get(Index);
				const UScriptStruct* Type = Entry ? Entry->GetScriptStruct() : nullptr;
				if (Type == FVeyraSpikePropertyTargetData::StaticStruct())
				{
					const FVeyraSpikePropertyTargetData& ByProperties = static_cast<const FVeyraSpikePropertyTargetData&>(*Entry);
					Received.Point = ByProperties.Point;
					Received.PropertyMarker = ByProperties.Marker;
				}
			}
		}

		FPIENetworkComponent<FState>& GrantSpikeAbility(FPIENetworkComponent<FState>& Chain)
		{
			return Chain
				.ThenServer(TEXT("Grant the spike ability to client 0"), [](FState& State) {
					ServerAbilitiesOf(State, 0).GiveAbility(FGameplayAbilitySpec(UVeyraTargetDataSpikeAbility::StaticClass()));
				})
				.UntilClient(TEXT("Client 0 has the ability"), 0, [](FState& State) { return SpikeHandleOf(LocalAbilitiesOf(State.World)).IsValid(); });
		}

		TEST_METHOD(AClientCannotStartAnAbilityOnAVanguardItDoesNotControl)
		{
			GrantSpikeAbility(StartMatch(Network, Layout, EVeyraMatchPhase::Live))
				.ThenClient(TEXT("Client 0 tries to activate it"), 0, [this](FState& State) {
					UAbilitySystemComponent* Abilities = LocalAbilitiesOf(State.World);
					ASSERT_THAT(IsTrue(Abilities->GetAvatarActor()->GetLocalRole() == ROLE_SimulatedProxy));
					ASSERT_THAT(IsFalse(Abilities->TryActivateAbilityByClass(UVeyraTargetDataSpikeAbility::StaticClass())));
				});
		}

		TEST_METHOD(ClientTargetDataCrossesIrisIntact)
		{
			GrantSpikeAbility(StartMatch(Network, Layout, EVeyraMatchPhase::Live))
				.ThenServer(TEXT("Listen for client 0's target data"), [this](FState& State) {
					UAbilitySystemComponent& Abilities = ServerAbilitiesOf(State, 0);
					ReceivedHandle = Abilities.AbilityTargetDataSetDelegate(SpikeHandleOf(&Abilities), FPredictionKey())
						.AddLambda([this](const FGameplayAbilityTargetDataHandle& Data, FGameplayTag) { Record(Data); });
				})
				.ThenClient(TEXT("Client 0 sends target data"), 0, [](FState& State) {
					UAbilitySystemComponent* Abilities = LocalAbilitiesOf(State.World);
					Abilities->ServerSetReplicatedTargetData(SpikeHandleOf(Abilities), FPredictionKey(), MakeTargetData(), FGameplayTag(), FPredictionKey());
				})
				.UntilServer(TEXT("The server receives it"), [this](FState&) { return Received.Count > 0; })
				.ThenServer(TEXT("What arrived"), [this](FState& State) {
					UAbilitySystemComponent& Abilities = ServerAbilitiesOf(State, 0);
					Abilities.AbilityTargetDataSetDelegate(SpikeHandleOf(&Abilities), FPredictionKey()).Remove(ReceivedHandle);
					ASSERT_THAT(AreEqual(Received.Count, 1));
					ASSERT_THAT(AreEqual(Received.Entries, 1));
					// FVector_NetQuantize100 keeps two decimal places.
					constexpr double QuantizeTolerance = 0.01;
					ASSERT_THAT(IsTrue(Received.Point.Equals(FVector(SentX, SentY, SentZ), QuantizeTolerance)));
					ASSERT_THAT(AreEqual(Received.PropertyMarker, SentPropertyMarker));
				});
		}
	};
}

#endif // ENABLE_PIE_NETWORK_TEST
