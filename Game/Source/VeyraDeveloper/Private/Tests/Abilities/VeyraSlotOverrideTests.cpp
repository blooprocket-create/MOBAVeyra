// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Cooldowns/VeyraCooldownComponent.h"
#include "CQTest.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "TimerManager.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraAbilitiesTests
{
	// Veyra.Abilities.SlotOverrides.*: what a slot holds for a while instead of its own ability, and the
	// cast events (ADR-018 §1, §3).
	TEST_CLASS(SlotOverrides, "Veyra.Abilities")
	{
		// Fixture values, independent of the committed Abilities.json.
		static constexpr double Reach = 300.0;
		static constexpr double LongSeconds = 60.0;
		static constexpr double WindowSeconds = 1.0;
		static constexpr double FollowUpCooldown = 2.0;
		static constexpr double VariantCooldown = 5.0;
		static constexpr float WorldStep = 0.1f;

		FActorTestSpawner Spawner;
		FVeyraAbilitiesTuning Tuning;
		AVeyraVanguardCharacter* Caster = nullptr;
		AVeyraVanguardCharacter* Enemy = nullptr;
		UVeyraAbilityLoadoutComponent* Loadout = nullptr;

		/** An instant area around the caster that applies Status to every enemy within reach. */
		FVeyraAreaAbilityTuning AreaApplying(const TCHAR* Status, double CooldownSeconds) const
		{
			FVeyraAreaAbilityTuning Area;
			Area.Cast = InstantCast(0.0, CooldownSeconds, 0.0);
			Area.Origin = EVeyraAreaOrigin::Caster;
			FVeyraAreaZoneTuning& Zone = Area.Zones.AddDefaulted_GetRef();
			Zone.Shape = CircleOf(Reach);
			Zone.Effects.Statuses.Add(ArchetypeTestId(Status));
			return Area;
		}

		static FVeyraRecastTuning WindowFor(const TCHAR* FollowUp, EVeyraRecastExpiry OnExpiry)
		{
			FVeyraRecastTuning Window;
			Window.Ability = ArchetypeTestId(FollowUp);
			Window.WindowSeconds = WindowSeconds;
			Window.OnExpiry = OnExpiry;
			return Window;
		}

		BEFORE_EACH()
		{
			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_slow")), StatusOf(EVeyraStatusKind::Slow, 0.3, LongSeconds));
			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_stun")), StatusOf(EVeyraStatusKind::Stun, 0.0, LongSeconds));
			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_haste")), StatusOf(EVeyraStatusKind::MoveSpeed, 0.2, LongSeconds));

			// A lunge whose slot then holds a follow-up for a while (Breakneck, Bear Hug).
			FVeyraAreaAbilityTuning Lunge = AreaApplying(TEXT("test_slow"), LongSeconds);
			Lunge.Cast.RecastWindow.Add(WindowFor(TEXT("test_follow_up"), EVeyraRecastExpiry::Lapse));
			Tuning.Area.Add(ArchetypeTestId(TEXT("test_lunge")), Lunge);
			Tuning.Area.Add(ArchetypeTestId(TEXT("test_follow_up")), AreaApplying(TEXT("test_stun"), FollowUpCooldown));

			// One whose follow-up casts itself if its window runs out (Last Exit).
			FVeyraAreaAbilityTuning Exit = AreaApplying(TEXT("test_slow"), LongSeconds);
			Exit.Cast.RecastWindow.Add(WindowFor(TEXT("test_last_exit"), EVeyraRecastExpiry::Cast));
			Tuning.Area.Add(ArchetypeTestId(TEXT("test_exit")), Exit);
			Tuning.Area.Add(ArchetypeTestId(TEXT("test_last_exit")), AreaApplying(TEXT("test_stun"), FollowUpCooldown));

			Tuning.Area.Add(ArchetypeTestId(TEXT("test_variant")), AreaApplying(TEXT("test_stun"), VariantCooldown));

			FVeyraSelfBuffAbilityTuning Haste;
			Haste.Cast = InstantCast(0.0, LongSeconds, 0.0);
			Haste.Statuses.Add(ArchetypeTestId(TEXT("test_haste")));
			Tuning.SelfBuff.Add(ArchetypeTestId(TEXT("test_self_haste")), Haste);
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(&Tuning);

			FArchetypeTestWorld World{ Spawner };
			Caster = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			Enemy = &World.Spawn(EVeyraTeam::B, FVector(Reach / 2.0, 0.0, 0.0));
			Loadout = Caster->GetPlayerState()->FindComponentByClass<UVeyraAbilityLoadoutComponent>();
		}

		AFTER_EACH()
		{
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(nullptr);
		}

		/** Moves world time on by Seconds in small steps, timers included. */
		void AdvanceWorld(double Seconds)
		{
			UWorld& World = Spawner.GetWorld();
			const double Until = World.GetTimeSeconds() + Seconds;
			while (World.GetTimeSeconds() < Until)
			{
				World.Tick(LEVELTICK_TimeOnly, WorldStep);
				++GFrameCounter;
				World.GetTimerManager().Tick(WorldStep);
			}
		}

		EVeyraCastRejection CastSlot(EVeyraAbilitySlot Slot) const
		{
			return FArchetypeTestWorld::CastAt(*Caster, Slot, FVector(Reach / 2.0, 0.0, 0.0));
		}

		bool Holds(EVeyraAbilitySlot Slot, const TCHAR* Ability) const
		{
			const FVeyraLoadoutEntry* Entry = Loadout->FindSlot(Slot);
			return Entry && Entry->Ability == ArchetypeTestId(Ability);
		}

		double CooldownOf(const TCHAR* Ability) const
		{
			return Caster->GetPlayerState()->FindComponentByClass<UVeyraCooldownComponent>()->GetRemainingSecondsNow(ArchetypeTestId(Ability));
		}

		static FVeyraOverrideSpec VariantSpec(const TCHAR* Ability, EVeyraOverrideUse Use, FName Group = NAME_None)
		{
			FVeyraOverrideSpec Spec;
			Spec.Ability = ArchetypeTestId(Ability);
			Spec.Use = Use;
			Spec.Group = Group;
			return Spec;
		}

		TEST_METHOD(ARecastWindowHoldsItsFollowUpUntilItIsCast)
		{
			FArchetypeTestWorld World{ Spawner };
			ASSERT_THAT(IsTrue(World.Learn(*Caster, EVeyraAbilitySlot::Q, ArchetypeTestId(TEXT("test_lunge")))));
			ASSERT_THAT(IsTrue(CastSlot(EVeyraAbilitySlot::Q) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(World.Has(*Enemy, TEXT("test_slow")) && !World.Has(*Enemy, TEXT("test_stun"))));
			ASSERT_THAT(IsTrue(Holds(EVeyraAbilitySlot::Q, TEXT("test_follow_up")), TEXT("the slot holds the follow-up")));

			// The follow-up has its own cooldown, and its slot's rank: the lunge's cooldown does not stop it.
			ASSERT_THAT(IsTrue(CastSlot(EVeyraAbilitySlot::Q) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(World.Has(*Enemy, TEXT("test_stun"))));
			ASSERT_THAT(IsTrue(Holds(EVeyraAbilitySlot::Q, TEXT("test_lunge")) && !Loadout->IsOverridden(EVeyraAbilitySlot::Q), TEXT("used once, it reverts")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(CooldownOf(TEXT("test_follow_up")), FollowUpCooldown, 1e-3)));
			ASSERT_THAT(IsTrue(CastSlot(EVeyraAbilitySlot::Q) == EVeyraCastRejection::OnCooldown, TEXT("the lunge cools down from its own cast")));
		}

		TEST_METHOD(AnUnusedWindowLapses)
		{
			FArchetypeTestWorld World{ Spawner };
			ASSERT_THAT(IsTrue(World.Learn(*Caster, EVeyraAbilitySlot::Q, ArchetypeTestId(TEXT("test_lunge")))));
			ASSERT_THAT(IsTrue(CastSlot(EVeyraAbilitySlot::Q) == EVeyraCastRejection::None));
			AdvanceWorld(WindowSeconds / 2.0);
			ASSERT_THAT(IsTrue(Holds(EVeyraAbilitySlot::Q, TEXT("test_follow_up"))));
			AdvanceWorld(WindowSeconds);
			ASSERT_THAT(IsTrue(Holds(EVeyraAbilitySlot::Q, TEXT("test_lunge")) && !Loadout->IsOverridden(EVeyraAbilitySlot::Q)));
			ASSERT_THAT(IsFalse(World.Has(*Enemy, TEXT("test_stun")), TEXT("a lapsed follow-up does nothing")));
		}

		TEST_METHOD(AnExpiringFollowUpCanCastItself)
		{
			FArchetypeTestWorld World{ Spawner };
			ASSERT_THAT(IsTrue(World.Learn(*Caster, EVeyraAbilitySlot::Q, ArchetypeTestId(TEXT("test_exit")))));
			ASSERT_THAT(IsTrue(CastSlot(EVeyraAbilitySlot::Q) == EVeyraCastRejection::None));
			ASSERT_THAT(IsFalse(World.Has(*Enemy, TEXT("test_stun"))));
			AdvanceWorld(WindowSeconds + WorldStep * 2.0);
			ASSERT_THAT(IsTrue(World.Has(*Enemy, TEXT("test_stun")), TEXT("its payoff is guaranteed")));
			ASSERT_THAT(IsTrue(Holds(EVeyraAbilitySlot::Q, TEXT("test_exit")) && !Loadout->IsOverridden(EVeyraAbilitySlot::Q)));
			ASSERT_THAT(IsTrue(CooldownOf(TEXT("test_last_exit")) > 0.0));
		}

		TEST_METHOD(AVariantSharesItsSlotsRankAndKeepsItsOwnCooldown)
		{
			FArchetypeTestWorld World{ Spawner };
			UAbilitySystemComponent& AbilitySystem = *Caster->GetAbilitySystemComponent();
			ASSERT_THAT(IsTrue(World.Learn(*Caster, EVeyraAbilitySlot::Q, ArchetypeTestId(TEXT("test_lunge")))));
			// W has no rank, so neither has anything that holds it.
			ASSERT_THAT(IsTrue(Loadout->Override(AbilitySystem, EVeyraAbilitySlot::W, VariantSpec(TEXT("test_follow_up"), EVeyraOverrideUse::WhileActive))));
			ASSERT_THAT(IsTrue(CastSlot(EVeyraAbilitySlot::W) == EVeyraCastRejection::NotLearned));

			ASSERT_THAT(IsTrue(Loadout->Override(AbilitySystem, EVeyraAbilitySlot::Q, VariantSpec(TEXT("test_variant"), EVeyraOverrideUse::WhileActive))));
			ASSERT_THAT(IsTrue(CastSlot(EVeyraAbilitySlot::Q) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(Holds(EVeyraAbilitySlot::Q, TEXT("test_variant")), TEXT("a variant lasts until its state ends")));
			ASSERT_THAT(IsTrue(CastSlot(EVeyraAbilitySlot::Q) == EVeyraCastRejection::OnCooldown));
			ASSERT_THAT(IsTrue(CooldownOf(TEXT("test_lunge")) == 0.0));

			Loadout->EndOverride(AbilitySystem, EVeyraAbilitySlot::Q);
			ASSERT_THAT(IsTrue(Holds(EVeyraAbilitySlot::Q, TEXT("test_lunge"))));
			ASSERT_THAT(IsTrue(CastSlot(EVeyraAbilitySlot::Q) == EVeyraCastRejection::None, TEXT("the slot's own ability kept its own cooldown")));
		}

		TEST_METHOD(ASharedVariantCoolsDownAsItsSlot)
		{
			FArchetypeTestWorld World{ Spawner };
			UAbilitySystemComponent& AbilitySystem = *Caster->GetAbilitySystemComponent();
			ASSERT_THAT(IsTrue(World.Learn(*Caster, EVeyraAbilitySlot::Q, ArchetypeTestId(TEXT("test_lunge")))));
			FVeyraOverrideSpec Shared = VariantSpec(TEXT("test_variant"), EVeyraOverrideUse::WhileActive);
			Shared.bSharesCooldown = true;
			ASSERT_THAT(IsTrue(Loadout->Override(AbilitySystem, EVeyraAbilitySlot::Q, Shared)));
			ASSERT_THAT(IsTrue(Loadout->CooldownIdOf(ArchetypeTestId(TEXT("test_variant"))) == ArchetypeTestId(TEXT("test_lunge"))));
			ASSERT_THAT(IsTrue(CastSlot(EVeyraAbilitySlot::Q) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(CooldownOf(TEXT("test_lunge")), VariantCooldown, 1e-3), TEXT("one cooldown for both")));
			Loadout->EndOverride(AbilitySystem, EVeyraAbilitySlot::Q);
			ASSERT_THAT(IsTrue(CastSlot(EVeyraAbilitySlot::Q) == EVeyraCastRejection::OnCooldown));
		}

		TEST_METHOD(AUsedOnceGroupEndsTogether)
		{
			FArchetypeTestWorld World{ Spawner };
			UAbilitySystemComponent& AbilitySystem = *Caster->GetAbilitySystemComponent();
			ASSERT_THAT(IsTrue(World.Learn(*Caster, EVeyraAbilitySlot::Q, ArchetypeTestId(TEXT("test_lunge")))));
			ASSERT_THAT(IsTrue(World.Equip(*Caster, EVeyraAbilitySlot::W, ArchetypeTestId(TEXT("test_self_haste")))));
			const FName Redlined(TEXT("test_redlined"));
			const auto OverrideBoth = [&] {
				return Loadout->Override(AbilitySystem, EVeyraAbilitySlot::Q, VariantSpec(TEXT("test_variant"), EVeyraOverrideUse::Once, Redlined))
					&& Loadout->Override(AbilitySystem, EVeyraAbilitySlot::W, VariantSpec(TEXT("test_follow_up"), EVeyraOverrideUse::Once, Redlined));
			};
			// The next cast of either runs its override, and spends both (Raska's Redlined).
			ASSERT_THAT(IsTrue(OverrideBoth()));
			ASSERT_THAT(IsTrue(CastSlot(EVeyraAbilitySlot::Q) == EVeyraCastRejection::None));
			ASSERT_THAT(IsFalse(Loadout->IsOverridden(EVeyraAbilitySlot::Q) || Loadout->IsOverridden(EVeyraAbilitySlot::W)));
			ASSERT_THAT(IsTrue(Holds(EVeyraAbilitySlot::W, TEXT("test_self_haste"))));

			ASSERT_THAT(IsTrue(OverrideBoth()));
			Loadout->EndGroup(AbilitySystem, Redlined);
			ASSERT_THAT(IsFalse(Loadout->IsOverridden(EVeyraAbilitySlot::Q) || Loadout->IsOverridden(EVeyraAbilitySlot::W)));
		}

		TEST_METHOD(CastEventsSayWhetherTheCastIsOffensive)
		{
			FArchetypeTestWorld World{ Spawner };
			ASSERT_THAT(IsTrue(World.Learn(*Caster, EVeyraAbilitySlot::Q, ArchetypeTestId(TEXT("test_lunge")))));
			UVeyraCombatEventSubsystem* Events = Spawner.GetWorld().GetSubsystem<UVeyraCombatEventSubsystem>();
			TArray<FVeyraCastEvent> Started;
			TArray<FVeyraCastEvent> Committed;
			Events->OnCastStarted.AddLambda([&Started](const FVeyraCastEvent& Event) { Started.Add(Event); });
			Events->OnCastCommitted.AddLambda([&Committed](const FVeyraCastEvent& Event) { Committed.Add(Event); });

			ASSERT_THAT(IsTrue(CastSlot(EVeyraAbilitySlot::Q) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(Started.Num() == 1 && Committed.Num() == 1));
			ASSERT_THAT(IsTrue(Committed[0].Ability == ArchetypeTestId(TEXT("test_lunge")) && Committed[0].bOffensive));
			ASSERT_THAT(IsTrue(Committed[0].Caster.Get() == Caster->GetAbilitySystemComponent()));

			// Its follow-up is spent, so a self-buff can hold the slot: it acts on its caster alone.
			Loadout->EndOverride(*Caster->GetAbilitySystemComponent(), EVeyraAbilitySlot::Q);
			ASSERT_THAT(IsTrue(Loadout->Override(*Caster->GetAbilitySystemComponent(), EVeyraAbilitySlot::Q, VariantSpec(TEXT("test_self_haste"), EVeyraOverrideUse::Once))));
			ASSERT_THAT(IsTrue(CastSlot(EVeyraAbilitySlot::Q) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(Committed.Num() == 2 && Committed[1].Ability == ArchetypeTestId(TEXT("test_self_haste")) && !Committed[1].bOffensive));
			ASSERT_THAT(IsTrue(Started.Num() == 2 && !Started[1].bOffensive));
		}

		TEST_METHOD(ValidationChecksTheRecastWindow)
		{
			constexpr int32 RankCounts[] = { 5, 3 };
			ASSERT_THAT(IsTrue(VeyraAbilityRules::Validate(Tuning, RankCounts).IsEmpty(), FString::Join(VeyraAbilityRules::Validate(Tuning, RankCounts), TEXT(" | "))));
			FVeyraAbilitiesTuning Broken = Tuning;
			FVeyraCastTuning& Lunge = Broken.Area.FindChecked(ArchetypeTestId(TEXT("test_lunge"))).Cast;
			Lunge.RecastWindow[0].Ability = ArchetypeTestId(TEXT("no_such_ability"));
			Lunge.RecastWindow[0].WindowSeconds = 0.0;
			Lunge.RecastWindow.Add(WindowFor(TEXT("test_follow_up"), EVeyraRecastExpiry::Lapse));
			const FString All = FString::Join(VeyraAbilityRules::Validate(Broken, RankCounts), TEXT(" | "));
			ASSERT_THAT(IsTrue(All.Contains(TEXT("/area/test_lunge/cast/recastWindow:")), All));
			ASSERT_THAT(IsTrue(All.Contains(TEXT("/area/test_lunge/cast/recastWindow/0/ability:")), All));
			ASSERT_THAT(IsTrue(All.Contains(TEXT("/area/test_lunge/cast/recastWindow/0/windowSeconds:")), All));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
