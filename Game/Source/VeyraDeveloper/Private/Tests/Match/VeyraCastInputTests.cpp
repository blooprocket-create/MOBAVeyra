// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER

#include "Input/VeyraCastInput.h"
#include "Input/VeyraControlPreferences.h"
#include "VeyraSettingsStore.h"
#include "VeyraSettingsSubsystem.h"

namespace VeyraCastInputTests
{
	bool Is(const FVeyraCastOutcome& Outcome, EVeyraCastStep Step, EVeyraAbilitySlot Slot)
	{
		return Outcome.Step == Step && Outcome.Slot == Slot;
	}

	// Veyra.Match.CastInput.*: when a key, its release, a click or a cancel casts (Settings Bible §1.2, §1.7; ADR-041 §1).
	TEST_CLASS(CastInput, "Veyra.Match")
	{
		TEST_METHOD(AQuickCastCastsOnThePress)
		{
			FVeyraCastInput Input;
			ASSERT_THAT(IsTrue(Is(Input.Press(EVeyraAbilitySlot::Q, EVeyraCastMode::Quick, false), EVeyraCastStep::CastNow, EVeyraAbilitySlot::Q)));
			ASSERT_THAT(IsFalse(Input.GetIndicator().IsSet()));
			ASSERT_THAT(IsTrue(Input.Release(EVeyraAbilitySlot::Q).Step == EVeyraCastStep::Nothing, TEXT("its release casts nothing more")));
		}

		TEST_METHOD(AQuickCastWithIndicatorShowsWhileHeldAndCastsOnTheRelease)
		{
			FVeyraCastInput Input;
			ASSERT_THAT(IsTrue(Is(Input.Press(EVeyraAbilitySlot::W, EVeyraCastMode::QuickWithIndicator, false), EVeyraCastStep::Show, EVeyraAbilitySlot::W)));
			ASSERT_THAT(IsTrue(Input.GetIndicator().IsSet() && Input.GetIndicator()->Slot == EVeyraAbilitySlot::W));
			ASSERT_THAT(IsTrue(Input.Release(EVeyraAbilitySlot::E).Step == EVeyraCastStep::Nothing, TEXT("another key's release")));
			ASSERT_THAT(IsTrue(Is(Input.Release(EVeyraAbilitySlot::W), EVeyraCastStep::CastNow, EVeyraAbilitySlot::W)));
			ASSERT_THAT(IsFalse(Input.GetIndicator().IsSet()));
		}

		TEST_METHOD(ANormalCastWaitsForTheClick)
		{
			FVeyraCastInput Input;
			ASSERT_THAT(IsTrue(Is(Input.Press(EVeyraAbilitySlot::E, EVeyraCastMode::Normal, false), EVeyraCastStep::Show, EVeyraAbilitySlot::E)));
			ASSERT_THAT(IsTrue(Input.Release(EVeyraAbilitySlot::E).Step == EVeyraCastStep::Nothing, TEXT("the release leaves it waiting")));
			ASSERT_THAT(IsTrue(Input.Press(EVeyraAbilitySlot::E, EVeyraCastMode::Normal, false).Step == EVeyraCastStep::Nothing, TEXT("a second press keeps it waiting")));
			ASSERT_THAT(IsTrue(Input.GetIndicator().IsSet()));
			ASSERT_THAT(IsTrue(Is(Input.Confirm(), EVeyraCastStep::CastNow, EVeyraAbilitySlot::E)));
			ASSERT_THAT(IsTrue(Input.Confirm().Step == EVeyraCastStep::Nothing, TEXT("a click with nothing waiting casts nothing")));
		}

		TEST_METHOD(ACancelHidesAWaitingCast)
		{
			FVeyraCastInput Input;
			Input.Press(EVeyraAbilitySlot::R, EVeyraCastMode::Normal, false);
			ASSERT_THAT(IsTrue(Is(Input.Cancel(), EVeyraCastStep::Hide, EVeyraAbilitySlot::R)));
			ASSERT_THAT(IsTrue(Input.Confirm().Step == EVeyraCastStep::Nothing));
			ASSERT_THAT(IsTrue(Input.Cancel().Step == EVeyraCastStep::Nothing, TEXT("Escape with nothing waiting is the menu's")));
		}

		TEST_METHOD(AnotherKeyMovesTheIndicatorAndAQuickCastReplacesIt)
		{
			FVeyraCastInput Input;
			Input.Press(EVeyraAbilitySlot::Q, EVeyraCastMode::Normal, false);
			ASSERT_THAT(IsTrue(Is(Input.Press(EVeyraAbilitySlot::Spell1, EVeyraCastMode::Normal, false), EVeyraCastStep::Show, EVeyraAbilitySlot::Spell1)));
			ASSERT_THAT(IsTrue(Input.GetIndicator()->Slot == EVeyraAbilitySlot::Spell1));
			ASSERT_THAT(IsTrue(Is(Input.Press(EVeyraAbilitySlot::Item2, EVeyraCastMode::Quick, false), EVeyraCastStep::CastNow, EVeyraAbilitySlot::Item2)));
			ASSERT_THAT(IsFalse(Input.GetIndicator().IsSet(), TEXT("the waiting cast gave way")));
		}

		TEST_METHOD(ShowCastRangePreviewsWithoutCasting)
		{
			FVeyraCastInput Input;
			ASSERT_THAT(IsTrue(Is(Input.Press(EVeyraAbilitySlot::Q, EVeyraCastMode::Quick, /*bPreview*/ true), EVeyraCastStep::Show, EVeyraAbilitySlot::Q)));
			ASSERT_THAT(IsTrue(Input.GetIndicator()->bPreviewOnly));
			ASSERT_THAT(IsTrue(Input.Release(EVeyraAbilitySlot::Q).Step == EVeyraCastStep::Nothing));
			ASSERT_THAT(IsTrue(Input.Confirm().Step == EVeyraCastStep::Nothing, TEXT("a click never casts a preview")));
			ASSERT_THAT(IsTrue(Is(Input.EndPreview(), EVeyraCastStep::Hide, EVeyraAbilitySlot::Q)));
			// The next cast uses the player's mode.
			ASSERT_THAT(IsTrue(Is(Input.Press(EVeyraAbilitySlot::Q, EVeyraCastMode::Quick, false), EVeyraCastStep::CastNow, EVeyraAbilitySlot::Q)));
		}

		TEST_METHOD(EndingAPreviewLeavesAWaitingCast)
		{
			FVeyraCastInput Input;
			Input.Press(EVeyraAbilitySlot::W, EVeyraCastMode::Normal, false);
			ASSERT_THAT(IsTrue(Input.EndPreview().Step == EVeyraCastStep::Nothing));
			ASSERT_THAT(IsTrue(Input.GetIndicator().IsSet()));
		}

		TEST_METHOD(AWaitingCastGivesUpOnceItsAbilityCannotBeCast)
		{
			// ADR-041 §1: the ability becoming unavailable cancels its waiting cast, so a later click casts nothing.
			const FVeyraContentId Ability = FVeyraContentId::FromText(TEXT("test_bolt")).GetValue();
			const FVeyraContentId Override = FVeyraContentId::FromText(TEXT("test_bolt_recast")).GetValue();
			const FVeyraSlotNow Ready{ /*bCasterAlive*/ true, Ability, /*bLocked*/ false, /*CooldownSeconds*/ 0.0 };
			const auto Changed = [&Ready](TFunctionRef<void(FVeyraSlotNow&)> Change) {
				FVeyraSlotNow Now = Ready;
				Change(Now);
				return Now;
			};
			const TArray<FVeyraSlotNow> Unavailable = {
				Changed([](FVeyraSlotNow& Now) { Now.bCasterAlive = false; }),
				Changed([](FVeyraSlotNow& Now) { Now.CooldownSeconds = 4.0; }),
				Changed([&Override](FVeyraSlotNow& Now) { Now.Ability = Override; }),
				Changed([](FVeyraSlotNow& Now) { Now.Ability = FVeyraContentId(); }),
				Changed([](FVeyraSlotNow& Now) { Now.bLocked = true; }),
			};
			for (const EVeyraCastMode Mode : { EVeyraCastMode::Normal, EVeyraCastMode::QuickWithIndicator })
			{
				for (const FVeyraSlotNow& Now : Unavailable)
				{
					FVeyraCastInput Input;
					Input.Press(EVeyraAbilitySlot::E, Mode, false, Ability);
					ASSERT_THAT(IsTrue(Input.Recheck(Ready).Step == EVeyraCastStep::Nothing, TEXT("still castable, still waiting")));
					ASSERT_THAT(IsTrue(Is(Input.Recheck(Now), EVeyraCastStep::Hide, EVeyraAbilitySlot::E)));
					ASSERT_THAT(IsTrue(Input.Confirm().Step == EVeyraCastStep::Nothing && Input.Release(EVeyraAbilitySlot::E).Step == EVeyraCastStep::Nothing,
						TEXT("nothing casts after it")));
				}
			}
		}

		TEST_METHOD(APreviewIgnoresWhetherItsAbilityCanBeCast)
		{
			const FVeyraContentId Ability = FVeyraContentId::FromText(TEXT("test_bolt")).GetValue();
			FVeyraCastInput Input;
			Input.Press(EVeyraAbilitySlot::Q, EVeyraCastMode::Normal, /*bPreview*/ true, Ability);
			ASSERT_THAT(IsTrue(Input.Recheck(FVeyraSlotNow{ true, Ability, false, 4.0 }).Step == EVeyraCastStep::Nothing, TEXT("Show Cast Range shows a cooling ability")));
			ASSERT_THAT(IsTrue(Input.GetIndicator().IsSet()));
			ASSERT_THAT(IsTrue(FVeyraCastInput().Recheck(FVeyraSlotNow()).Step == EVeyraCastStep::Nothing, TEXT("nothing waiting")));
		}

		TEST_METHOD(EveryModeHasItsName)
		{
			for (const EVeyraCastMode Mode : { EVeyraCastMode::Quick, EVeyraCastMode::QuickWithIndicator, EVeyraCastMode::Normal })
			{
				ASSERT_THAT(IsTrue(VeyraCastModes::Parse(VeyraCastModes::NameOf(Mode)) == Mode));
			}
			ASSERT_THAT(IsFalse(VeyraCastModes::Parse(TEXT("Smart")).IsSet()));
		}
	};

	// Veyra.Match.ControlPreferences.*: each slot's casting mode, as the player set it (ADR-041 §5).
	TEST_CLASS(ControlPreferences, "Veyra.Match")
	{
		/** Loaded before each test: CQTest builds its classes while registering them, which in a packaged client is before the engine starts. */
		FVeyraSettingsRegistry Registry;

		BEFORE_EACH()
		{
			UVeyraSettingsSubsystem::LoadRegistry(Registry);
		}

		TEST_METHOD(EveryCastingModeSettingIsAChoiceOfTheThreeModes)
		{
			for (const FVeyraContentId& Id : VeyraControlPreferences::CastModeSettings())
			{
				const FVeyraChoiceSetting* Choice = Registry.Choices.Find(Id);
				ASSERT_THAT(IsNotNull(Choice, Id.ToString()));
				ASSERT_THAT(AreEqual(3, Choice->Options.Num()));
				for (const FString& Option : Choice->Options)
				{
					ASSERT_THAT(IsTrue(VeyraCastModes::Parse(Option).IsSet(), Option));
				}
			}
		}

		TEST_METHOD(EverySlotCastsQuickByDefault)
		{
			const FVeyraSettingsStore Defaults(Registry);
			const FVeyraControlPreferences Untouched = VeyraControlPreferences::Resolve(&Defaults);
			const FVeyraControlPreferences Plain = VeyraControlPreferences::Resolve(nullptr);
			for (const EVeyraAbilitySlot Slot : { EVeyraAbilitySlot::Q, EVeyraAbilitySlot::R, EVeyraAbilitySlot::Spell2, EVeyraAbilitySlot::Item5, EVeyraAbilitySlot::VisionTool })
			{
				ASSERT_THAT(IsTrue(Untouched.CastModeOf(Slot) == EVeyraCastMode::Quick && Plain.CastModeOf(Slot) == EVeyraCastMode::Quick));
			}
		}

		TEST_METHOD(SmartSelfCastAndTargetVanguardsOnlyFollowTheirSettings)
		{
			const FVeyraSettingsStore Defaults(Registry);
			const FVeyraControlPreferences Untouched = VeyraControlPreferences::Resolve(&Defaults);
			ASSERT_THAT(IsTrue(Untouched.SmartSelfCast.IsEmpty() && !Untouched.bTargetVanguardsToggles, TEXT("Off and Hold by default")));
			FVeyraSettingsStore Store(Registry);
			Store.Set(*VeyraControlPreferences::SmartSelfCast(EVeyraAbilitySlot::E), VeyraSettings::On());
			Store.Set(VeyraControlPreferences::TargetVanguardsMode(), TEXT("Toggle"));
			ASSERT_THAT(IsTrue(Untouched.AttackMoveTarget == EVeyraAttackMoveTarget::ClosestToVanguard));
			Store.Set(VeyraControlPreferences::AttackMoveTarget(), TEXT("ClosestToCursor"));
			const FVeyraControlPreferences Set = VeyraControlPreferences::Resolve(&Store);
			ASSERT_THAT(IsTrue(Set.AttackMoveTarget == EVeyraAttackMoveTarget::ClosestToCursor));
			ASSERT_THAT(IsTrue(Set.SmartSelfCast.Num() == 1 && Set.SmartSelfCast.Contains(EVeyraAbilitySlot::E) && Set.bTargetVanguardsToggles));
			ASSERT_THAT(IsNull(VeyraControlPreferences::SmartSelfCast(EVeyraAbilitySlot::Spell1), TEXT("a kit slot's alone")));
		}

		TEST_METHOD(EachSlotFollowsItsOwnSetting)
		{
			FVeyraSettingsStore Store(Registry);
			Store.Set(VeyraControlPreferences::CastMode(EVeyraAbilitySlot::W), VeyraCastModes::NameOf(EVeyraCastMode::Normal));
			Store.Set(VeyraControlPreferences::CastMode(EVeyraAbilitySlot::Item1), VeyraCastModes::NameOf(EVeyraCastMode::QuickWithIndicator));
			const FVeyraControlPreferences Set = VeyraControlPreferences::Resolve(&Store);
			ASSERT_THAT(IsTrue(Set.CastModeOf(EVeyraAbilitySlot::W) == EVeyraCastMode::Normal));
			ASSERT_THAT(IsTrue(Set.CastModeOf(EVeyraAbilitySlot::Q) == EVeyraCastMode::Quick, TEXT("Q keeps its own")));
			// One setting for every item slot and the vision tool.
			ASSERT_THAT(IsTrue(Set.CastModeOf(EVeyraAbilitySlot::Item4) == EVeyraCastMode::QuickWithIndicator));
			ASSERT_THAT(IsTrue(Set.CastModeOf(EVeyraAbilitySlot::VisionTool) == EVeyraCastMode::QuickWithIndicator));
		}
	};
}

#endif
