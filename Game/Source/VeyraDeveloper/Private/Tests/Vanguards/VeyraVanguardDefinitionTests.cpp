// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Absorption/VeyraDamageAbsorptionComponent.h"
#include "Attacks/VeyraBasicAttackComponent.h"
#include "Attributes/VeyraVitalsSet.h"
#include "Components/ActorTestSpawner.h"
#include "CQTest.h"
#include "Loadout/VeyraAbilityLoadoutComponent.h"
#include "Passives/VeyraDeepFoundationPassive.h"
#include "Progression/VeyraProgressionComponent.h"
#include "Progression/VeyraProgressionTuningSubsystem.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "Tuning/VeyraVanguardsTuningSubsystem.h"
#include "VeyraPlayerState.h"
#include "VeyraVanguards.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraVanguardsTests
{
	inline FVeyraContentId VanguardTestId(const TCHAR* Text)
	{
		return FVeyraContentId::FromText(Text).GetValue();
	}

	// Veyra.Vanguards.Definitions.*: Vanguards.json, its checks against the Abilities and Progression
	// tuning, the passive registry, and preparing a participant as its Vanguard (ADR-008 §2, §5).
	TEST_CLASS(Definitions, "Veyra.Vanguards")
	{
		FActorTestSpawner Spawner;

		static TArray<FString> ValidateAgainstCommitted(const FVeyraVanguardsTuning& Tuning)
		{
			const FVeyraProgressionTuning& Progression = UVeyraProgressionTuningSubsystem::Get();
			return VeyraVanguardRules::Validate(Tuning, UVeyraAbilitiesTuningSubsystem::Get(), Progression.BasicAbilityMaxRank, Progression.UltimateMaxRank);
		}

		static bool Mentions(const TArray<FString>& Problems, const TCHAR* Pointer)
		{
			return Problems.ContainsByPredicate([Pointer](const FString& Problem) { return Problem.StartsWith(Pointer); });
		}

		TEST_METHOD(TheCommittedFileLoads)
		{
			const UVeyraVanguardsTuningSubsystem* Subsystem = GEngine->GetEngineSubsystem<UVeyraVanguardsTuningSubsystem>();
			ASSERT_THAT(IsTrue(Subsystem && Subsystem->IsLoaded()));
			ASSERT_THAT(IsNotNull(UVeyraVanguardsTuningSubsystem::FindVanguard(VanguardTestId(TEXT("cairn")))));
			ASSERT_THAT(IsNotNull(UVeyraVanguardsTuningSubsystem::FindVanguard(VanguardTestId(TEXT("test_vanguard")))));
			const TArray<FString> Problems = ValidateAgainstCommitted(UVeyraVanguardsTuningSubsystem::Get());
			ASSERT_THAT(IsTrue(Problems.IsEmpty(), FString::Join(Problems, TEXT(" | "))));
		}

		TEST_METHOD(TheReleasedKitsArePlayableAndTheTestVanguardIsNot)
		{
			// Only Playable Vanguards are released: the backend's catalog lists them, and a Shipping
			// match server hosts nothing else (ADR-010 §6).
			for (const TCHAR* Released : { TEXT("cairn"), TEXT("qazharr"), TEXT("oriel"), TEXT("bryn"), TEXT("kade"), TEXT("vera"), TEXT("mimzi"),
					 TEXT("patch"), TEXT("gorraveth"), TEXT("raska"), TEXT("moro"), TEXT("korruk"), TEXT("mavra") })
			{
				const FVeyraVanguardDefinition* Definition = UVeyraVanguardsTuningSubsystem::FindVanguard(VanguardTestId(Released));
				ASSERT_THAT(IsTrue(Definition && Definition->Availability == EVeyraVanguardAvailability::Playable, Released));
			}
			const FVeyraVanguardDefinition* TestVanguard = UVeyraVanguardsTuningSubsystem::FindVanguard(VanguardTestId(TEXT("test_vanguard")));
			ASSERT_THAT(IsTrue(TestVanguard && TestVanguard->Availability == EVeyraVanguardAvailability::Developer));
		}

		TEST_METHOD(ValidationCatchesWhatTheSchemaCannot)
		{
			FVeyraVanguardsTuning Broken = UVeyraVanguardsTuningSubsystem::Get();
			FVeyraVanguardDefinition& Cairn = Broken.Vanguards.FindChecked(VanguardTestId(TEXT("cairn")));
			// A basic ability with five ranks in the ultimate's slot, and in two slots at once.
			Cairn.Abilities.R = Cairn.Abilities.Q;
			Cairn.Abilities.W = { VanguardTestId(TEXT("no_such_ability")) };
			Cairn.Passive = { VanguardTestId(TEXT("no_such_passive")) };
			Cairn.Body.CapsuleHalfHeight = Cairn.Body.CapsuleRadius / 2.0;
			Broken.DeepFoundation.FindChecked(VanguardTestId(TEXT("cairn_deep_foundation"))).Shield.AmountByRank = { 1.0, 2.0 };
			const TArray<FString> Problems = ValidateAgainstCommitted(Broken);
			const FString All = FString::Join(Problems, TEXT(" | "));
			ASSERT_THAT(IsTrue(Mentions(Problems, TEXT("/vanguards/cairn/abilities/r/0: in this slot,")), All));
			ASSERT_THAT(IsTrue(Mentions(Problems, TEXT("/vanguards/cairn/abilities/r/0: is already in another slot")), All));
			ASSERT_THAT(IsTrue(Mentions(Problems, TEXT("/vanguards/cairn/abilities/w/0: names ability")), All));
			ASSERT_THAT(IsTrue(Mentions(Problems, TEXT("/vanguards/cairn/passive/0:")), All));
			ASSERT_THAT(IsTrue(Mentions(Problems, TEXT("/vanguards/cairn/body/capsuleHalfHeight:")), All));
			ASSERT_THAT(IsTrue(Mentions(Problems, TEXT("/deepFoundation/cairn_deep_foundation/shield/amountByRank:")), All));
		}

		TEST_METHOD(DeadReckoningNeedsAStep)
		{
			FVeyraVanguardsTuning Tuning = UVeyraVanguardsTuningSubsystem::Get();
			Tuning.MovingTarget.FindChecked(VanguardTestId(TEXT("kade_moving_target"))).DeadReckoning.StepUnits = 0.0;
			ASSERT_THAT(IsTrue(Mentions(ValidateAgainstCommitted(Tuning), TEXT("/movingTarget/kade_moving_target/deadReckoning"))));
		}

		TEST_METHOD(EachPassiveRunsByTheMapThatDefinesIt)
		{
			ASSERT_THAT(IsTrue(VeyraVanguards::PassiveClassFor(VanguardTestId(TEXT("cairn_deep_foundation"))) == UVeyraDeepFoundationPassive::StaticClass()));
			ASSERT_THAT(IsTrue(VeyraVanguards::PassiveClassFor(VanguardTestId(TEXT("no_such_passive"))) == nullptr));
		}

		TEST_METHOD(PreparingAParticipantMakesItTheVanguard)
		{
			const FVeyraContentId CairnId = VanguardTestId(TEXT("cairn"));
			const FVeyraVanguardDefinition& Cairn = *UVeyraVanguardsTuningSubsystem::FindVanguard(CairnId);
			AVeyraPlayerState& Participant = Spawner.SpawnActor<AVeyraPlayerState>();
			UAbilitySystemComponent& AbilitySystem = *Participant.GetAbilitySystemComponent();

			const FVeyraPreparedVanguard Prepared = VeyraVanguards::PrepareCombatant(AbilitySystem, CairnId);
			ASSERT_THAT(IsTrue(Prepared.bPrepared));
			ASSERT_THAT(IsTrue(AbilitySystem.GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute()) == Cairn.BaseStats.MaxHealth));
			const UVeyraAbilityLoadoutComponent* Loadout = Participant.FindComponentByClass<UVeyraAbilityLoadoutComponent>();
			ASSERT_THAT(IsTrue(Loadout->FindSlot(EVeyraAbilitySlot::Q)->Ability == Cairn.Abilities.Q[0]));
			ASSERT_THAT(IsTrue(Loadout->FindSlot(EVeyraAbilitySlot::R)->Ability == Cairn.Abilities.R[0]));
			const UVeyraProgressionComponent* Progression = Participant.FindComponentByClass<UVeyraProgressionComponent>();
			ASSERT_THAT(IsTrue(Progression->GetLevel() == 1 && Progression->GetUnspentSkillPoints() > 0 && Progression->GetRank(EVeyraAbilitySlot::Q) == 0,
				TEXT("level 1, with the first rank left to the player")));
			ASSERT_THAT(IsTrue(Participant.FindComponentByClass<UVeyraBasicAttackComponent>()->GetProfile().Range == Cairn.BasicAttack.Range));
			ASSERT_THAT(IsTrue(Prepared.Passive && Prepared.Passive->IsA<UVeyraDeepFoundationPassive>() && Prepared.Passive->GetOwnerAbilitySystem() == &AbilitySystem));
		}

		TEST_METHOD(AnUnknownVanguardIsRefused)
		{
			TestRunner->AddExpectedMessagePlain(TEXT("Vanguards.json does not define it"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, 1);
			AVeyraPlayerState& Participant = Spawner.SpawnActor<AVeyraPlayerState>();
			ASSERT_THAT(IsFalse(VeyraVanguards::PrepareCombatant(*Participant.GetAbilitySystemComponent(), VanguardTestId(TEXT("no_such_vanguard"))).bPrepared));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
