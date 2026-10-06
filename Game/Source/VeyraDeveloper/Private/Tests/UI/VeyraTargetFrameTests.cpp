// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER && WITH_VEYRA_UI

#include "AbilitySystemComponent.h"
#include "Attributes/VeyraResourceSet.h"
#include "Attributes/VeyraVitalsSet.h"
#include "Components/ActorTestSpawner.h"
#include "Cooldowns/VeyraCooldownComponent.h"
#include "Greybox/VeyraGreyboxSettings.h"
#include "Hud/VeyraHudLayout.h"
#include "Hud/VeyraTargetFrameModel.h"
#include "Loadout/VeyraAbilityLoadoutComponent.h"
#include "Progression/VeyraProgressionComponent.h"
#include "Shop/VeyraShopSubsystem.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "Text/VeyraContentText.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "VeyraPlayerState.h"

namespace VeyraTargetFrameTests
{
	using namespace VeyraAbilitiesTests;

	// Veyra.UI.TargetFrame.*: the selected unit's frame (ADR-066 §3–§4), read from what every machine receives.
	TEST_CLASS(TargetFrame, "Veyra.UI")
	{
		// Fixture values: a long cooldown, its seconds' slack, and the server time it is read at.
		static constexpr double SpellSeconds = 300.0;
		static constexpr double Slack = 1.0;

		FActorTestSpawner Spawner;

		/** An enemy Vanguard playing a committed kit, at Level 1, with an empty inventory. */
		AVeyraVanguardCharacter& SpawnEnemy()
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector::ZeroVector);
			AVeyraPlayerState& Participant = *Enemy.GetPlayerState<AVeyraPlayerState>();
			Participant.SetVanguardId(FVeyraContentId::FromText(TEXT("cairn")).GetValue());
			Participant.SetPlayerName(TEXT("Rival"));
			Participant.FindComponentByClass<UVeyraProgressionComponent>()->Initialize(FVeyraStatGrowth(), 0.0);
			UVeyraShopSubsystem::InitializeInventory(Participant);
			return Enemy;
		}

		TEST_METHOD(AVanguardsFrameShowsItsBarsItemsAndFluxSpells)
		{
			AVeyraVanguardCharacter& Enemy = SpawnEnemy();
			AVeyraPlayerState& Participant = *Enemy.GetPlayerState<AVeyraPlayerState>();
			UAbilitySystemComponent& Abilities = *Enemy.GetAbilitySystemComponent();
			const FVeyraContentId Spell = UVeyraAbilitiesTuningSubsystem::Get().FluxSpells.Roster[0];
			UVeyraAbilityLoadoutComponent& Loadout = *Participant.FindComponentByClass<UVeyraAbilityLoadoutComponent>();
			ASSERT_THAT(IsTrue(Loadout.Grant(Abilities, EVeyraAbilitySlot::Spell1, Spell)));
			Loadout.SetUnlockedSpellSlots(1);
			const double Now = Spawner.GetWorld().GetTimeSeconds();
			Participant.FindComponentByClass<UVeyraCooldownComponent>()->StartCooldown(Spell, SpellSeconds, EVeyraCooldownHaste::Fixed);

			const TOptional<FVeyraTargetFrame> Frame = VeyraTargetFrame::Describe(Enemy, EVeyraTeam::A, Now);
			ASSERT_THAT(IsTrue(Frame.IsSet()));
			ASSERT_THAT(IsTrue(Frame->Kind == EVeyraUnitKind::Vanguard && Frame->Side == EVeyraTeam::B && Frame->bAlive));
			ASSERT_THAT(IsTrue(Frame->Name == VeyraContentText::VanguardName(Participant.GetVanguardId()).ToString() && Frame->Detail == TEXT("Rival") && Frame->Level == 1));
			ASSERT_THAT(IsTrue(Frame->Vitals.Health == Abilities.GetNumericAttribute(UVeyraVitalsSet::GetHealthAttribute())
				&& Frame->Vitals.MaxResource == Abilities.GetNumericAttribute(UVeyraResourceSet::GetMaxResourceAttribute())));
			ASSERT_THAT(IsTrue(Frame->Items.Num() == 6 && !Frame->Items[0].IsValid(), TEXT("its six slots, empty")));
			// Its Flux Spells: the first unlocked and cooling, the second locked and empty (ADR-066 §4).
			ASSERT_THAT(AreEqual(2, Frame->Spells.Num()));
			ASSERT_THAT(IsTrue(Frame->Spells[0].Spell == Spell && !Frame->Spells[0].bLocked));
			ASSERT_THAT(IsNear(Frame->Spells[0].CooldownSeconds, SpellSeconds, Slack));
			ASSERT_THAT(IsNear(Frame->Spells[0].CooldownTotal, SpellSeconds, Slack));
			ASSERT_THAT(IsTrue(!Frame->Spells[1].Spell.IsValid() && Frame->Spells[1].bLocked));
			// Ready again once the cooldown has passed.
			const TOptional<FVeyraTargetFrame> Later = VeyraTargetFrame::Describe(Enemy, EVeyraTeam::A, Now + SpellSeconds + Slack);
			ASSERT_THAT(IsTrue(Later.IsSet() && Later->Spells[0].CooldownSeconds == 0.0 && Later->Spells[0].CooldownTotal == 0.0));
		}

		TEST_METHOD(OnlyAFluxSpellsCooldownIsSharedWithEveryMachine)
		{
			AVeyraVanguardCharacter& Enemy = SpawnEnemy();
			AVeyraPlayerState& Participant = *Enemy.GetPlayerState<AVeyraPlayerState>();
			UVeyraCooldownComponent& Cooldowns = *Participant.FindComponentByClass<UVeyraCooldownComponent>();
			const FVeyraContentId Spell = UVeyraAbilitiesTuningSubsystem::Get().FluxSpells.Roster[0];
			const FVeyraContentId Other = FVeyraContentId::FromText(TEXT("an_ability")).GetValue();
			ASSERT_THAT(IsTrue(Participant.FindComponentByClass<UVeyraAbilityLoadoutComponent>()->Grant(*Enemy.GetAbilitySystemComponent(), EVeyraAbilitySlot::Spell1, Spell)));
			Cooldowns.StartCooldown(Spell, SpellSeconds, EVeyraCooldownHaste::Fixed);
			Cooldowns.StartCooldown(Other, SpellSeconds);
			const double Now = Spawner.GetWorld().GetTimeSeconds();
			ASSERT_THAT(IsTrue(Cooldowns.GetSharedRemainingSeconds(Spell, Now) > 0.0));
			ASSERT_THAT(IsTrue(Cooldowns.GetSharedRemainingSeconds(Other, Now) == 0.0 && Cooldowns.GetRemainingSeconds(Other, Now) > 0.0,
				TEXT("an ability's own cooldown stays its owner's")));
			// A spell the slot no longer holds stops being shared.
			Participant.FindComponentByClass<UVeyraAbilityLoadoutComponent>()->Clear(*Enemy.GetAbilitySystemComponent(), EVeyraAbilitySlot::Spell1);
			ASSERT_THAT(IsTrue(Cooldowns.GetSharedRemainingSeconds(Spell, Now) == 0.0));
		}

		TEST_METHOD(AFrameStandsUnderTeamFluxInsideTheSafeArea)
		{
			const UVeyraGreyboxSettings& Settings = *GetDefault<UVeyraGreyboxSettings>();
			const FVeyraHudArrangement Layout = VeyraHudLayout::Arrange(FVector2D(1920.0, 1080.0), Settings, FVeyraInterfacePreferences(), 4);
			ASSERT_THAT(IsTrue(Layout.TargetFrame.bIsValid));
			ASSERT_THAT(IsTrue(Layout.TargetFrame.Min.X >= Layout.Inset.X && Layout.TargetFrame.Min.Y > Layout.Inset.Y));
			ASSERT_THAT(IsNear(Layout.TargetFrame.GetSize().X, static_cast<double>(Settings.TargetFrameWidth * Layout.TeamPanels), 1e-3));
			ASSERT_THAT(IsTrue(Layout.TargetFrame.Max.Y < Layout.DeckTopLeft.Y, TEXT("clear of the deck")));
		}

		TEST_METHOD(AKindWithoutTextIsNamedByItsWords)
		{
			ASSERT_THAT(AreEqual(FString(TEXT("Spark Breaker")), VeyraTargetFrame::Words(TEXT("spark_breaker"))));
			ASSERT_THAT(AreEqual(FString(TEXT("Strider")), VeyraTargetFrame::Words(TEXT("strider"))));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER && WITH_VEYRA_UI
