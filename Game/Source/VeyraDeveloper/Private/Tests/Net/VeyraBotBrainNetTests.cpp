// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"

#if ENABLE_PIE_NETWORK_TEST

#include "AbilitySystemComponent.h"
#include "Attributes/VeyraVitalsSet.h"
#include "Brain/VeyraBotBrainComponent.h"
#include "Gold/VeyraGoldComponent.h"
#include "Inventory/VeyraInventoryComponent.h"
#include "Progression/VeyraProgressionComponent.h"
#include "Recall/VeyraRecallComponent.h"
#include "Shop/VeyraShopSubsystem.h"
#include "Tests/Net/VeyraMatchNetTestHelpers.h"
#include "Tests/Net/VeyraNetTestHelpers.h"
#include "Tuning/VeyraBotsTuningSubsystem.h"
#include "VeyraCombatVerbs.h"
#include "VeyraGameMode.h"
#include "VeyraPlayerController.h"
#include "VeyraPlayerState.h"
#include "VeyraTeamStart.h"
#include "VeyraVanguardController.h"

namespace VeyraNetTests
{
	/** Bots tuning a test sets up, with an entry for the test Vanguard. Get() returns it while this object lives. */
	struct FScopedBotsTuning
	{
		FVeyraBotsTuning Tuning;

		FScopedBotsTuning()
			: Tuning(UVeyraBotsTuningSubsystem::Get())
		{
			FVeyraBotVanguardTuning& Test = Tuning.Vanguards.Add(TestVanguardId());
			Test.Build = { FVeyraContentId::FromText(TEXT("iron_grip")).GetValue() };
			Test.SkillPriority = { EVeyraBotSkill::Q, EVeyraBotSkill::W, EVeyraBotSkill::E };
			Test.Abilities.Add(TestVanguardAbilityQ(), EVeyraBotAbilityUse::Damage);
			UVeyraBotsTuningSubsystem::SetTestOverride(&Tuning);
		}

		~FScopedBotsTuning()
		{
			UVeyraBotsTuningSubsystem::SetTestOverride(nullptr);
		}

		UE_NONCOPYABLE(FScopedBotsTuning);
	};

	// Veyra.Net.BotBrains.*: a playing bot fights, retreats and recalls, shops and ranks up through the
	// players' paths (ADR-013 §3, §4), on the server, while its enemy's client watches.
	NETWORK_TEST_CLASS(BotBrains, "Veyra.Net")
	{
		struct FState : public FBasePIENetworkComponentState
		{
		};

		FPIENetworkComponent<FState> Network{ TestRunner, TestCommandBuilder, bInitializing };
		TUniquePtr<FScopedExpectedPlayers> ExpectedPlayers;
		TUniquePtr<FScopedMatchTuning> Tuning;
		TUniquePtr<FScopedBotsTuning> Bots;
		FVeyraGreyboxLayout Layout;

		// Fixture values: quick, sure bots, and room to stand apart.
		static constexpr double ShortPreparationSeconds = 0.1;
		static constexpr double QuickThinkSeconds = 0.1;
		static constexpr double Beside = 200.0;
		static constexpr double TowardTheMiddle = 1200.0;
		static constexpr double HurtFraction = 0.2;
		static constexpr double PlentyOfGold = 1000.0;

		TWeakObjectPtr<AVeyraPlayerState> Bot;
		double HumanHealthBefore = 0.0;

		BEFORE_EACH()
		{
			IgnoreKnownIrisWarnings(*TestRunner);
			ASSERT_THAT(IsTrue(VeyraGreybox::LoadLayout(Layout).IsEmpty()));
			Tuning = MakeUnique<FScopedMatchTuning>();
			Tuning->Tuning.Phases.PreparationSeconds = ShortPreparationSeconds;
			Bots = MakeUnique<FScopedBotsTuning>();
			for (FVeyraBotDifficultyTuning* Difficulty : { &Bots->Tuning.Difficulties.Beginner, &Bots->Tuning.Difficulties.Intermediate })
			{
				Difficulty->ThinkSeconds = QuickThinkSeconds;
				Difficulty->ReactionSeconds = 0.0;
				Difficulty->CastChance = 0.0;
				Difficulty->FightHealthMargin = -1.0;
			}
			ExpectedPlayers = MakeUnique<FScopedExpectedPlayers>(MatchClientCount);
			BuildMatchNetwork(Network);
		}

		AFTER_EACH()
		{
			Bots.Reset();
			Tuning.Reset();
			ExpectedPlayers.Reset();
		}

		static AVeyraPlayerState& Human(FState& State)
		{
			return *ServerControllerOf(State, 0)->GetPlayerState<AVeyraPlayerState>();
		}

		static double HealthOf(const AVeyraPlayerState& Participant)
		{
			return Participant.GetAbilitySystemComponent()->GetNumericAttribute(UVeyraVitalsSet::GetHealthAttribute());
		}

		/** Seats a playing bot on the human's enemy side. */
		void SeatBot(FState& State)
		{
			const EVeyraTeam Enemies = VeyraTeams::Opposing(Human(State).GetVeyraTeam());
			Bot = GameModeOf(State.World)->AddPlayingBot(TEXT("TestBot"), Enemies, TestVanguardId(), EVeyraBotDifficulty::Intermediate);
			ASSERT_THAT(IsTrue(Bot.IsValid() && Bot->GetPawn()));
			ASSERT_THAT(IsNotNull(Bot->GetVanguardController()->FindComponentByClass<UVeyraBotBrainComponent>()));
		}

		TEST_METHOD(APlayingBotFightsAnEnemyVanguardInSight)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Seat a bot beside the human's Vanguard"), [this](FState& State) {
					SeatBot(State);
					const APawn* Target = Human(State).GetPawn();
					Bot->GetPawn()->TeleportTo(Target->GetActorLocation() + FVector(Beside, 0.0, 0.0), Target->GetActorRotation());
					HumanHealthBefore = HealthOf(Human(State));
				})
				.UntilServer(TEXT("The bot attacks the human's Vanguard"), [this](FState& State) {
					return Bot.IsValid() && Bot->GetVanguardController()->GetAttackTarget() == Human(State).GetPawn()
						&& HealthOf(Human(State)) < HumanHealthBefore;
				})
				.UntilClient(TEXT("And the human's client sees the wound"), 0, [this](FState& State) {
					const AVeyraPlayerState* Own = LocalControllerOf(State.World)->GetPlayerState<AVeyraPlayerState>();
					return HealthOf(*Own) < HumanHealthBefore;
				});
		}

		TEST_METHOD(AHurtBotAwayFromDangerRecalls)
		{
			// Nobody is a danger at any distance.
			Bots->Tuning.Senses.SafeRadius = Beside;
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Seat a bot, send it out of its fountain, and hurt it"), [this](FState& State) {
					SeatBot(State);
					APawn* Body = Bot->GetPawn();
					const FVector Home = Body->GetActorLocation();
					Body->TeleportTo(Home + FVector(Home.X > 0.0 ? -TowardTheMiddle : TowardTheMiddle, 0.0, 0.0), Body->GetActorRotation());
					FVeyraRawDamageEvent Wound;
					Wound.Components.Add({ EVeyraDamageType::TrueDamage, HealthOf(*Bot) * (1.0 - HurtFraction) });
					ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*Human(State).GetAbilitySystemComponent(), *Bot->GetAbilitySystemComponent(), Wound)));
				})
				.UntilServer(TEXT("It recalls"), [this](FState&) {
					return Bot.IsValid() && Bot->FindComponentByClass<UVeyraRecallComponent>()->IsRecalling();
				});
		}

		/** How many of the bots' consumable Participant holds. */
		static int32 ConsumablesHeld(const AVeyraPlayerState& Participant)
		{
			const FVeyraContentId& Item = UVeyraBotsTuningSubsystem::Get().Consumables.Item;
			int32 Held = 0;
			for (const FVeyraInventorySlot& Slot : Participant.FindComponentByClass<UVeyraInventoryComponent>()->GetSlots())
			{
				Held += !Slot.IsEmpty() && Slot.Item == Item ? Slot.Count : 0;
			}
			return Held;
		}

		TEST_METHOD(ABotBuysConsumablesAndDrinksOneWhenHurtAwayFromItsFountain)
		{
			// Nothing to build, so its Gold goes on its consumables (ADR-056 §1).
			Bots->Tuning.Vanguards[TestVanguardId()].Build.Reset();
			const int32 Carried = UVeyraBotsTuningSubsystem::GetDifficulty(EVeyraBotDifficulty::Intermediate).ConsumablesCarried;
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Seat a bot at its fountain with the Gold for its consumables"), [this](FState& State) {
					SeatBot(State);
					Bot->FindComponentByClass<UVeyraGoldComponent>()->Grant(PlentyOfGold, EVeyraGoldReason::Developer);
				})
				.UntilServer(TEXT("It buys as many as it carries"), [this, Carried](FState&) { return Bot.IsValid() && ConsumablesHeld(*Bot) == Carried; })
				.ThenServer(TEXT("Send it out of its fountain, and hurt it below its drinking line but above its retreat line"), [this](FState& State) {
					APawn* Body = Bot->GetPawn();
					const FVector Home = Body->GetActorLocation();
					Body->TeleportTo(Home + FVector(Home.X > 0.0 ? -TowardTheMiddle : TowardTheMiddle, 0.0, 0.0), Body->GetActorRotation());
					const double Line = UVeyraBotsTuningSubsystem::Get().Consumables.DrinkHealthFraction;
					const double Retreat = UVeyraBotsTuningSubsystem::GetDifficulty(EVeyraBotDifficulty::Intermediate).RetreatHealthFraction;
					const double MaxHealth = Bot->GetAbilitySystemComponent()->GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute());
					FVeyraRawDamageEvent Wound;
					Wound.Components.Add({ EVeyraDamageType::TrueDamage, HealthOf(*Bot) - MaxHealth * (Line + Retreat) / 2.0 });
					ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*Human(State).GetAbilitySystemComponent(), *Bot->GetAbilitySystemComponent(), Wound)));
				})
				.UntilServer(TEXT("It drinks one"), [this, Carried](FState&) { return Bot.IsValid() && ConsumablesHeld(*Bot) == Carried - 1; });
		}

		TEST_METHOD(AnItemActiveCastThroughTheGameModeEndsItsPurchasesUndo)
		{
			// The bot buys nothing of its own, so the only purchase to undo is the test's.
			Bots->Tuning.Vanguards[TestVanguardId()].Build.Reset();
			for (FVeyraBotDifficultyTuning* Difficulty : { &Bots->Tuning.Difficulties.Beginner, &Bots->Tuning.Difficulties.Intermediate })
			{
				Difficulty->ConsumablesCarried = 0;
			}
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Buy a bot a Razorwheel at its fountain, cast its Active as the bot's orders do, and try to undo"), [this](FState& State) {
					SeatBot(State);
					UVeyraShopSubsystem* Shop = State.World->GetSubsystem<UVeyraShopSubsystem>();
					const FVeyraContentId Wheel = FVeyraContentId::FromText(TEXT("razorwheel")).GetValue();
					Bot->FindComponentByClass<UVeyraGoldComponent>()->Grant(PlentyOfGold * 10.0, EVeyraGoldReason::Developer);
					Shop->SetAtFountain(*Bot, true);
					ASSERT_THAT(IsTrue(Shop->Buy(*Bot, Wheel) == EVeyraShopRefusal::None));
					const int32 Index = Bot->FindComponentByClass<UVeyraInventoryComponent>()->GetSlots().IndexOfByPredicate(
						[&Wheel](const FVeyraInventorySlot& Slot) { return Slot.Item == Wheel; });
					ASSERT_THAT(IsTrue(Index != INDEX_NONE));
					const EVeyraAbilitySlot Slot = static_cast<EVeyraAbilitySlot>(static_cast<int32>(EVeyraAbilitySlot::Item1) + Index);
					ASSERT_THAT(IsTrue(GameModeOf(State.World)->HandleCastOrder(Bot.Get(), Slot, FVeyraCastTarget()) == EVeyraCastRejection::None));
					// The Active gave benefit, so the purchase stays, for a bot as for a player (Item Bible §12; ADR-056 §5).
					ASSERT_THAT(IsTrue(Shop->Undo(*Bot) == EVeyraShopRefusal::NothingToUndo));
				});
		}

		TEST_METHOD(ABotBuysItsBuildAndRanksUpAtItsFountain)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Seat a bot at its fountain"), [this](FState& State) { SeatBot(State); })
				.UntilServer(TEXT("It buys the first item of its build and ranks Q"), [this](FState&) {
					if (!Bot.IsValid())
					{
						return false;
					}
					const FVeyraContentId Grip = FVeyraContentId::FromText(TEXT("iron_grip")).GetValue();
					const bool bBought = Bot->FindComponentByClass<UVeyraInventoryComponent>()->GetSlots().ContainsByPredicate(
						[&Grip](const FVeyraInventorySlot& Slot) { return !Slot.IsEmpty() && Slot.Item == Grip; });
					return bBought && Bot->FindComponentByClass<UVeyraProgressionComponent>()->GetRank(EVeyraAbilitySlot::Q) >= 1;
				});
		}
	};
}

#endif // ENABLE_PIE_NETWORK_TEST
