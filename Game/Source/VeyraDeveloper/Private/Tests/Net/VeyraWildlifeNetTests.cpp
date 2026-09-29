// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"

#if ENABLE_PIE_NETWORK_TEST

#include "AbilitySystemComponent.h"
#include "EngineUtils.h"
#include "Tests/Net/VeyraBattlegroundNetTestHelpers.h"
#include "Tests/Net/VeyraNetTestHelpers.h"
#include "Tests/World/VeyraBattlegroundTestLayout.h"
#include "VeyraCombatVerbs.h"
#include "VeyraPlayerState.h"
#include "Wildlife/VeyraJungleSubsystem.h"
#include "Wildlife/VeyraWildlife.h"

namespace VeyraNetTests
{
	// Veyra.Net.WildlifeCamps.*: a camp spawns as the match goes live, every client sees its creatures,
	// and it fights back when a Vanguard strikes it (Battleground Bible §8, §17; ADR-014 §1, §2).
	NETWORK_TEST_CLASS(WildlifeCamps, "Veyra.Net")
	{
		struct FState : public FBasePIENetworkComponentState
		{
		};

		FPIENetworkComponent<FState> Network{ TestRunner, TestCommandBuilder, bInitializing };
		TUniquePtr<FScopedExpectedPlayers> ExpectedPlayers;
		TUniquePtr<FScopedMatchTuning> Tuning;
		TUniquePtr<VeyraWorldTests::FScopedWorldTuning> WorldTuning;
		FVeyraGreyboxLayout Greybox;

		// Fixture values: a short preparation, and a camp of three on Team A's half of the compact
		// battleground that spawns as the match goes live.
		static constexpr double ShortPreparationSeconds = 0.1;
		static constexpr int32 Pack = 3;
		static constexpr double CampX = 800.0;
		static constexpr double CampY = -1600.0;
		static constexpr double Leash = 400.0;
		static constexpr double Spacing = 150.0;
		static constexpr double Beside = 60.0;
		static constexpr double Scratch = 10.0;

		BEFORE_EACH()
		{
			IgnoreKnownIrisWarnings(*TestRunner);
			ASSERT_THAT(IsTrue(VeyraGreybox::LoadLayout(Greybox).IsEmpty()));
			Tuning = MakeUnique<FScopedMatchTuning>();
			Tuning->Tuning.Phases.PreparationSeconds = ShortPreparationSeconds;
			WorldTuning = MakeUnique<VeyraWorldTests::FScopedWorldTuning>();
			FVeyraCampTuning Camp;
			Camp.Species = FVeyraContentId::FromText(TEXT("skittermaw")).GetValue();
			Camp.Count = Pack;
			Camp.Center = { CampX, CampY };
			Camp.SpawnSeconds = 0.0;
			Camp.RespawnSeconds = 60.0;
			Camp.LeashRadius = Leash;
			Camp.Spacing = Spacing;
			WorldTuning->Tuning.Wildlife.Camps = { Camp };
			ExpectedPlayers = MakeUnique<FScopedExpectedPlayers>(MatchClientCount);
			BuildMatchNetwork(Network);
		}

		AFTER_EACH()
		{
			WorldTuning.Reset();
			Tuning.Reset();
			ExpectedPlayers.Reset();
		}

		static int32 SeenCreatures(const UWorld* World)
		{
			int32 Seen = 0;
			for (TActorIterator<AVeyraWildlife> It(World); It; ++It)
			{
				Seen += It->GetSpecies().IsValid() && !It->IsActorBeingDestroyed() ? 1 : 0;
			}
			return Seen;
		}

		TEST_METHOD(ACampSpawnsEveryClientSeesItAndItFightsBack)
		{
			StartBattleground(Network, Greybox, EVeyraMatchPhase::Live)
				.UntilServer(TEXT("Both halves' camps spawn as the match goes live"), [](FState& State) {
					const UVeyraJungleSubsystem* Jungle = State.World->GetSubsystem<UVeyraJungleSubsystem>();
					const TArray<FVeyraCampState> Camps = Jungle->GetCamps();
					return Camps.Num() == 2 && Camps[0].Alive == Pack && Camps[1].Alive == Pack;
				})
				.UntilClients(TEXT("Every client sees the creatures, with their species"), [](FState& State) { return SeenCreatures(State.World) == Pack * 2; })
				.ThenServer(TEXT("A Vanguard steps beside a creature and strikes it"), [this](FState& State) {
					AVeyraWildlife* Creature = State.World->GetSubsystem<UVeyraJungleSubsystem>()->GetCreatures(0)[0];
					AVeyraPlayerState& Hunter = *ServerControllerOf(State, 0)->GetPlayerState<AVeyraPlayerState>();
					APawn* Body = Hunter.GetPawn();
					const FVector Spot = Creature->GetActorLocation() + FVector(Creature->GetSimpleCollisionRadius() + Body->GetSimpleCollisionRadius() + Beside, 0.0, 0.0);
					Body->TeleportTo(FVector(Spot.X, Spot.Y, Body->GetActorLocation().Z), Body->GetActorRotation());
					FVeyraRawDamageEvent Damage;
					Damage.Components.Add({ EVeyraDamageType::TrueDamage, Scratch });
					ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*Hunter.GetAbilitySystemComponent(), *Creature->GetAbilitySystemComponent(), Damage)));
				})
				.UntilServer(TEXT("The camp answers: the Vanguard is hurt"), [](FState& State) {
					const AVeyraPlayerState& Hunter = *ServerControllerOf(State, 0)->GetPlayerState<AVeyraPlayerState>();
					return VeyraCombat::GetMissingHealth(*Hunter.GetAbilitySystemComponent()) > 0.0;
				});
		}
	};
}

#endif // ENABLE_PIE_NETWORK_TEST
