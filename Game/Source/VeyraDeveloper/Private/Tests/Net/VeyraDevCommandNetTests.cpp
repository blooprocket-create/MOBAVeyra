// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"

#if ENABLE_PIE_NETWORK_TEST

#include "AbilitySystemComponent.h"
#include "Attributes/VeyraVitalsSet.h"
#include "Cooldowns/VeyraCooldownComponent.h"
#include "DevCommands/VeyraDevCommands.h"
#include "Engine/Engine.h"
#include "Gold/VeyraGoldComponent.h"
#include "Inventory/VeyraInventoryComponent.h"
#include "Life/VeyraLifeComponent.h"
#include "Targeting/VeyraVisibility.h"
#include "Tests/Net/VeyraMatchNetTestHelpers.h"
#include "Tests/Net/VeyraNetTestHelpers.h"
#include "Tuning/VeyraItemsTuningSubsystem.h"
#include "VeyraPlayerState.h"
#include "VeyraTeamFluxSubsystem.h"
#include "VeyraVanguardCharacter.h"

namespace VeyraNetTests
{
	// Veyra.Net.DevCommandsInMatch.*: a Veyra.Dev.* command typed on a client runs on the server for
	// that client's own participant, and the server's reply reaches the client (PROJECT_STRUCTURE.md
	// "VeyraDeveloper").
	NETWORK_TEST_CLASS(DevCommandsInMatch, "Veyra.Net")
	{
		struct FState : public FBasePIENetworkComponentState
		{
		};

		FPIENetworkComponent<FState> Network{ TestRunner, TestCommandBuilder, bInitializing };
		TUniquePtr<FScopedExpectedPlayers> ExpectedPlayers;
		TUniquePtr<FScopedMatchTuning> Tuning;
		FVeyraGreyboxLayout Layout;

		// Fixture values: the amounts the commands are typed with, a respawn far longer than any test,
		// a cooldown to clear, and how far the teleport goes toward the map's centre.
		static constexpr double ShortPreparationSeconds = 0.1;
		static constexpr double GoldGift = 500.0;
		static constexpr double Hit = 100.0;
		static constexpr double LongRespawnSeconds = 600.0;
		static constexpr double LongCooldownSeconds = 60.0;
		static constexpr double TeleportStep = 300.0;
		static constexpr double ArrivedWithin = 50.0;
		static constexpr double RevealSeconds = 60.0;

		int32 EnemyId = INDEX_NONE;
		double GoldBefore = 0.0;
		double HealthAfterHit = 0.0;
		double FluxBefore = 0.0;
		FVector TeleportTarget = FVector::ZeroVector;
		FVeyraContentId GivenItem;

		BEFORE_EACH()
		{
			IgnoreKnownIrisWarnings(*TestRunner);
			ASSERT_THAT(IsTrue(VeyraGreybox::LoadLayout(Layout).IsEmpty()));
			Tuning = MakeUnique<FScopedMatchTuning>();
			Tuning->Tuning.Phases.PreparationSeconds = ShortPreparationSeconds;
			Tuning->Tuning.Respawn.SecondsByLevel = { LongRespawnSeconds };
			Tuning->Tuning.Respawn.Elapsed = FVeyraRespawnElapsedTuning();
			ExpectedPlayers = MakeUnique<FScopedExpectedPlayers>(MatchClientCount);
			BuildMatchNetwork(Network);
		}

		AFTER_EACH()
		{
			Tuning.Reset();
			ExpectedPlayers.Reset();
		}

		static AVeyraPlayerState& ServerParticipant(FState& State)
		{
			return *ServerControllerOf(State, 0)->GetPlayerState<AVeyraPlayerState>();
		}

		static double ServerHealth(FState& State)
		{
			return ServerParticipant(State).GetAbilitySystemComponent()->GetNumericAttribute(UVeyraVitalsSet::GetHealthAttribute());
		}

		static double ServerMaxHealth(FState& State)
		{
			return ServerParticipant(State).GetAbilitySystemComponent()->GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute());
		}

		static bool ServerAlive(FState& State)
		{
			const AVeyraPlayerState& Participant = ServerParticipant(State);
			return Participant.GetPawn() != nullptr && Participant.FindComponentByClass<UVeyraLifeComponent>()->IsAlive();
		}

		/** Types Line into the first client's console, as its player would. */
		static void Type(FState& State, const TCHAR* Line)
		{
			GEngine->Exec(State.World, Line);
		}

		static bool HasReplies(FState& State, int32 Count)
		{
			return LocalControllerOf(State.World)->GetDeveloperCommandReplyCount() >= Count;
		}

		TEST_METHOD(GoldArrivesAndEveryCommandReplies)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Note the Gold"), [this](FState& State) {
					GoldBefore = ServerParticipant(State).FindComponentByClass<UVeyraGoldComponent>()->GetGold();
				})
				.ThenClient(TEXT("Ask for Gold"), 0, [](FState& State) { Type(State, *FString::Printf(TEXT("Veyra.Dev.Gold %.0f"), GoldGift)); })
				.UntilServer(TEXT("The server gives it"), [this](FState& State) {
					return ServerParticipant(State).FindComponentByClass<UVeyraGoldComponent>()->GetGold() == GoldBefore + GoldGift;
				})
				.UntilClient(TEXT("And says so"), 0, [](FState& State) {
					return HasReplies(State, 1) && LocalControllerOf(State.World)->GetLastDeveloperCommandReply().StartsWith(TEXT("Gave you"));
				})
				.ThenClient(TEXT("Ask with a word for the amount"), 0, [](FState& State) { Type(State, TEXT("Veyra.Dev.Gold lots")); })
				.UntilClient(TEXT("The server replies with the usage"), 0, [](FState& State) {
					return HasReplies(State, 2) && LocalControllerOf(State.World)->GetLastDeveloperCommandReply().StartsWith(TEXT("Usage: Veyra.Dev.Gold"));
				})
				.ThenClient(TEXT("Ask for a command the server lacks"), 0, [](FState& State) {
					VeyraDevCommands::Request(*LocalControllerOf(State.World), TEXT("NoSuchCommand"));
				})
				.UntilClient(TEXT("The server says it has none"), 0, [](FState& State) {
					return HasReplies(State, 3) && LocalControllerOf(State.World)->GetLastDeveloperCommandReply().Contains(TEXT("no developer command"));
				})
				.ThenServer(TEXT("The Gold is unchanged by both"), [this](FState& State) {
					ASSERT_THAT(IsTrue(ServerParticipant(State).FindComponentByClass<UVeyraGoldComponent>()->GetGold() == GoldBefore + GoldGift));
				});
		}

		TEST_METHOD(DamageGodHealKillAndRespawn)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenClient(TEXT("Take a hit"), 0, [](FState& State) { Type(State, *FString::Printf(TEXT("Veyra.Dev.Damage %.0f"), Hit)); })
				.UntilClient(TEXT("It is answered"), 0, [](FState& State) { return HasReplies(State, 1); })
				.ThenServer(TEXT("The hit landed"), [this](FState& State) {
					// Regeneration may have given a little back since, never the whole hit.
					HealthAfterHit = ServerHealth(State);
					ASSERT_THAT(IsTrue(HealthAfterHit < ServerMaxHealth(State)));
				})
				.ThenClient(TEXT("Turn God on and take another"), 0, [](FState& State) {
					Type(State, TEXT("Veyra.Dev.God on"));
					Type(State, *FString::Printf(TEXT("Veyra.Dev.Damage %.0f"), Hit));
				})
				.UntilClient(TEXT("Both are answered"), 0, [](FState& State) { return HasReplies(State, 3); })
				.ThenServer(TEXT("God blocked the second"), [this](FState& State) { ASSERT_THAT(IsTrue(ServerHealth(State) >= HealthAfterHit)); })
				.ThenClient(TEXT("Turn God off and heal"), 0, [](FState& State) {
					Type(State, TEXT("Veyra.Dev.God"));
					Type(State, TEXT("Veyra.Dev.Heal"));
				})
				.UntilServer(TEXT("Health is full"), [](FState& State) { return ServerHealth(State) == ServerMaxHealth(State); })
				.ThenClient(TEXT("Die"), 0, [](FState& State) { Type(State, TEXT("Veyra.Dev.Kill")); })
				.UntilServer(TEXT("The Vanguard is dead"), [](FState& State) { return !ServerAlive(State); })
				.ThenClient(TEXT("Respawn"), 0, [](FState& State) { Type(State, TEXT("Veyra.Dev.Respawn")); })
				.UntilServer(TEXT("It is back long before its timer"), [](FState& State) { return ServerAlive(State); });
		}

		TEST_METHOD(CooldownsItemsAndTeleport)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Start a cooldown, choose an item and a point"), [this](FState& State) {
					AVeyraPlayerState& Participant = ServerParticipant(State);
					Participant.FindComponentByClass<UVeyraCooldownComponent>()->StartCooldown(TestVanguardAbilityQ(), LongCooldownSeconds);
					// Any first-tier equipment, as the tuning lists it.
					for (const TPair<FVeyraContentId, FVeyraItemDefinition>& Item : UVeyraItemsTuningSubsystem::Get().Items)
					{
						if (Item.Value.Tier == 1 && Item.Value.Category == EVeyraItemCategory::Equipment)
						{
							GivenItem = Item.Key;
							break;
						}
					}
					ASSERT_THAT(IsTrue(GivenItem.IsValid()));
					// Toward the centre, where the floor is open.
					const FVector Start = Participant.GetPawn()->GetActorLocation();
					TeleportTarget = Start - FVector(FMath::Sign(Start.X) * TeleportStep, 0.0, 0.0);
				})
				.ThenClient(TEXT("Clear the cooldowns, ask for the item and teleport"), 0, [this](FState& State) {
					Type(State, TEXT("Veyra.Dev.ResetCooldowns"));
					Type(State, *(TEXT("Veyra.Dev.GiveItem ") + GivenItem.ToString()));
					Type(State, *FString::Printf(TEXT("Veyra.Dev.Teleport %.0f %.0f"), TeleportTarget.X, TeleportTarget.Y));
				})
				.UntilClient(TEXT("All three are answered"), 0, [](FState& State) { return HasReplies(State, 3); })
				.ThenServer(TEXT("The cooldown is gone, the item is held, and the Vanguard stands there"), [this](FState& State) {
					AVeyraPlayerState& Participant = ServerParticipant(State);
					ASSERT_THAT(IsTrue(Participant.FindComponentByClass<UVeyraCooldownComponent>()->GetRemainingSecondsNow(TestVanguardAbilityQ()) == 0.0));
					const TArray<FVeyraInventorySlot>& Slots = Participant.FindComponentByClass<UVeyraInventoryComponent>()->GetSlots();
					ASSERT_THAT(IsTrue(Slots.ContainsByPredicate([this](const FVeyraInventorySlot& Slot) { return Slot.Item == GivenItem; }), *GivenItem.ToString()));
					const double Off = FVector::Dist2D(Participant.GetPawn()->GetActorLocation(), TeleportTarget);
					ASSERT_THAT(IsTrue(Off <= ArrivedWithin, FString::Printf(TEXT("%.0f from the point"), Off)));
				});
		}

		TEST_METHOD(TeamFluxArrivesAsTuned)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Note the team's Flux"), [this](FState& State) {
					FluxBefore = State.World->GetSubsystem<UVeyraTeamFluxSubsystem>()->GetPermanent(ServerParticipant(State).GetVeyraTeam());
				})
				.ThenClient(TEXT("Take a spire's Flux"), 0, [](FState& State) { Type(State, TEXT("Veyra.Dev.TeamFlux LaneSpire")); })
				.UntilServer(TEXT("The team has more permanent Flux"), [this](FState& State) {
					return State.World->GetSubsystem<UVeyraTeamFluxSubsystem>()->GetPermanent(ServerParticipant(State).GetVeyraTeam()) > FluxBefore;
				})
				.ThenClient(TEXT("Name no source"), 0, [](FState& State) { Type(State, TEXT("Veyra.Dev.TeamFlux")); })
				.UntilClient(TEXT("The reply lists the sources"), 0, [](FState& State) {
					return HasReplies(State, 2) && LocalControllerOf(State.World)->GetLastDeveloperCommandReply().Contains(TEXT("LaneSpire"));
				});
		}

		TEST_METHOD(RevealMapShowsTheHiddenEnemy)
		{
			// This test is about fog: the committed sight, not the see-everything fixture.
			Tuning->Vision.Tuning = Tuning->Vision.Committed;
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Part the sides beyond each other's sight"), [this](FState& State) {
					// As the vision tests part them: two and a half sight radii apart, across the centre.
					const double Sight = Tuning->Vision.Tuning.Sight.Vanguard;
					EnemyId = ServerControllerOf(State, 1)->PlayerState->GetPlayerId();
					for (const TPair<int32, double>& Placement : { TPair<int32, double>(0, -Sight), TPair<int32, double>(1, Sight * 1.5) })
					{
						AVeyraVanguardCharacter* Body = ServerControllerOf(State, Placement.Key)->GetVanguard();
						ASSERT_THAT(IsTrue(Body && Body->TeleportTo(FVector(Placement.Value, 0.0, Body->GetActorLocation().Z), Body->GetActorRotation())));
					}
				})
				.UntilServer(TEXT("The first player's side loses the enemy"), [this](FState& State) {
					const AVeyraVanguardCharacter* Enemy = FindVanguard(State.World, EnemyId);
					return Enemy && !VeyraVisibility::IsVisibleToTeam(ServerParticipant(State).GetVeyraTeam(), *Enemy);
				})
				.ThenClient(TEXT("Reveal the map"), 0, [](FState& State) { Type(State, *FString::Printf(TEXT("Veyra.Dev.RevealMap %.0f"), RevealSeconds)); })
				.UntilServer(TEXT("The side sees the enemy"), [this](FState& State) {
					const AVeyraVanguardCharacter* Enemy = FindVanguard(State.World, EnemyId);
					return Enemy && VeyraVisibility::IsVisibleToTeam(ServerParticipant(State).GetVeyraTeam(), *Enemy);
				})
				.UntilClient(TEXT("And the enemy reaches the player through the fog gate"), 0, [this](FState& State) { return FindVanguard(State.World, EnemyId) != nullptr; });
		}
	};
}

#endif // ENABLE_PIE_NETWORK_TEST
