// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"

#if ENABLE_PIE_NETWORK_TEST

#include "Components/PrimitiveComponent.h"
#include "EngineUtils.h"
#include "Layout/VeyraLayout.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "Terrain/VeyraTerrainWall.h"
#include "Tests/Net/VeyraBattlegroundNetTestHelpers.h"
#include "Tests/Net/VeyraNetTestHelpers.h"
#include "Tuning/VeyraWorldTuningSubsystem.h"

namespace VeyraNetTests
{
	// Veyra.Net.BattlegroundWalls.*: the committed battleground's walls stand solid on every machine, and
	// the server's paths go round them to every lane, camp and Flux Well (ADR-043 §2).
	NETWORK_TEST_CLASS(BattlegroundWalls, "Veyra.Net")
	{
		struct FState : public FBasePIENetworkComponentState
		{
		};

		FPIENetworkComponent<FState> Network{ TestRunner, TestCommandBuilder, bInitializing };
		TUniquePtr<FScopedExpectedPlayers> ExpectedPlayers;
		FVeyraGreyboxLayout Greybox;

		// Fixture values: how long the server may take to build the whole battleground's navigation, how
		// far a probed point may be moved onto it, and how far beyond a wall's faces its sides are probed.
		static constexpr double NavigationBuildSeconds = 120.0;
		static constexpr double ProjectionReach = 300.0;
		static constexpr double BesideWall = 200.0;

		BEFORE_EACH()
		{
			IgnoreKnownIrisWarnings(*TestRunner);
			ASSERT_THAT(IsTrue(VeyraGreybox::LoadLayout(Greybox).IsEmpty()));
			ExpectedPlayers = MakeUnique<FScopedExpectedPlayers>(MatchClientCount);
			BuildMatchNetwork(Network);
		}

		AFTER_EACH()
		{
			ExpectedPlayers.Reset();
		}

		static const FVeyraBattlegroundLayout& Layout()
		{
			return UVeyraWorldTuningSubsystem::Get().Layout;
		}

		/** A complete path between two points of the floor, and its length; unset where none joins them. */
		static TOptional<double> PathLength(UWorld* World, const FVector2D& From, const FVector2D& To)
		{
			UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
			FNavLocation Start;
			FNavLocation End;
			const FVector Reach(ProjectionReach);
			if (!Navigation || !Navigation->ProjectPointToNavigation(FVector(From, 0.0), Start, Reach) || !Navigation->ProjectPointToNavigation(FVector(To, 0.0), End, Reach))
			{
				return {};
			}
			const UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(World, Start.Location, End.Location);
			if (!Path || !Path->IsValid() || Path->IsPartial())
			{
				return {};
			}
			return Path->GetPathLength();
		}

		TEST_METHOD(TheWallsStandForEveryoneAndPathsGoRoundThem)
		{
			const int32 WallCount = Layout().Walls.Num() * 2;
			ASSERT_THAT(IsTrue(WallCount > 0));
			Network
				.ThenServer(TEXT("Build the committed battleground on the server"), [this](FState& State) {
					VeyraBattlegroundBuilder::SpawnRuntimeBattleground(*State.World, Layout(), Greybox, /*bServer*/ true);
					State.World->GetSubsystem<UVeyraBattlegroundSubsystem>()->SpawnStructures(Layout());
				})
				.ThenClients(TEXT("Build the floor on each client"), [this](FState& State) {
					VeyraBattlegroundBuilder::SpawnRuntimeBattleground(*State.World, Layout(), Greybox, /*bServer*/ false);
				})
				.UntilClients(TEXT("Every client has every wall, solid"), [WallCount](FState& State) {
					int32 Solid = 0;
					for (TActorIterator<AVeyraTerrainWall> It(State.World); It; ++It)
					{
						const UPrimitiveComponent* Body = Cast<UPrimitiveComponent>(It->GetRootComponent());
						Solid += Body && Body->GetCollisionEnabled() != ECollisionEnabled::NoCollision ? 1 : 0;
					}
					return Solid == WallCount;
				})
				.UntilServer(TEXT("The server's navigation is built round the walls"), [](FState& State) {
					UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(State.World);
					return Navigation && Navigation->GetDefaultNavDataInstance() && !Navigation->IsNavigationBuildInProgress()
						&& PathLength(State.World, VeyraLayout::Fountain(Layout(), EVeyraTeam::A), VeyraLayout::Fountain(Layout(), EVeyraTeam::B)).IsSet();
				}, FTimespan::FromSeconds(NavigationBuildSeconds))
				.ThenServer(TEXT("Paths reach every lane's ends, camp and Flux Well from Team A's fountain, and go round a wall"), [this](FState& State) {
					const FVector2D Fountain = VeyraLayout::Fountain(Layout(), EVeyraTeam::A);
					TArray<TPair<FString, FVector2D>> Targets;
					for (const FVeyraLaneLayout& Lane : Layout().Lanes)
					{
						Targets.Add({ UEnum::GetValueAsString(Lane.Lane) + TEXT(" start"), VeyraLayout::ToVector(Lane.Points[0]) });
						Targets.Add({ UEnum::GetValueAsString(Lane.Lane) + TEXT(" end"), VeyraLayout::ToVector(Lane.Points.Last()) });
					}
					for (const FVeyraCampTuning& Camp : UVeyraWorldTuningSubsystem::Get().Wildlife.Camps)
					{
						for (const EVeyraTeam Team : { EVeyraTeam::A, EVeyraTeam::B })
						{
							Targets.Add({ Camp.Species.ToString(), VeyraLayout::ForTeam(VeyraLayout::ToVector(Camp.Center), Team) });
						}
					}
					for (const FVeyraMapPoint& Site : UVeyraWorldTuningSubsystem::Get().FluxWells.Sites)
					{
						Targets.Add({ TEXT("a Flux Well"), VeyraLayout::ToVector(Site) });
					}
					for (const TPair<FString, FVector2D>& Target : Targets)
					{
						ASSERT_THAT(IsTrue(PathLength(State.World, Fountain, Target.Value).IsSet(), *FString::Printf(TEXT("a path to %s"), *Target.Key)));
					}

					// From one face of a wall to the other is farther by path than in a straight line.
					const FVeyraTerrainBox Wall = VeyraLayout::Walls(Layout())[0];
					const FVector2D Front = Wall.Centre + Wall.Facing * (Wall.Thickness / 2.0 + BesideWall);
					const FVector2D Back = Wall.Centre - Wall.Facing * (Wall.Thickness / 2.0 + BesideWall);
					const TOptional<double> Round = PathLength(State.World, Front, Back);
					ASSERT_THAT(IsTrue(Round.IsSet() && Round.GetValue() > FVector2D::Distance(Front, Back) + BesideWall,
						*FString::Printf(TEXT("round the wall: %g"), Round.Get(0.0))));
				});
		}
	};
}

#endif // ENABLE_PIE_NETWORK_TEST
