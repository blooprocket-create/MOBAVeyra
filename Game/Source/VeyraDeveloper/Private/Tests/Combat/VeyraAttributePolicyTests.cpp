// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Attributes/VeyraAttributePolicy.h"
#include "Attributes/VeyraDefenceSet.h"
#include "Attributes/VeyraOffenceSet.h"
#include "Attributes/VeyraVitalsSet.h"
#include "CQTest.h"
#include "Engine/Engine.h"
#include "GameplayEffect.h"
#include "Tuning/VeyraCombatTuningSubsystem.h"
#include "Tuning/VeyraTuning.h"
#include "UObject/Package.h"
#include "UObject/UObjectIterator.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraCombatTests
{
	// Veyra.Combat.AttributePolicy.*: Combat Bible §41 stacking as a modifier policy (ADR-006 §4).
	TEST_CLASS(AttributePolicy, "Veyra.Combat")
	{
		static UGameplayEffect* NewEffect(EGameplayEffectDurationType Duration, const FGameplayAttribute& Attribute, EGameplayModOp::Type Operation)
		{
			UGameplayEffect* Effect = NewObject<UGameplayEffect>(GetTransientPackage());
			Effect->DurationPolicy = Duration;
			FGameplayModifierInfo& Modifier = Effect->Modifiers.AddDefaulted_GetRef();
			Modifier.Attribute = Attribute;
			Modifier.ModifierOp = Operation;
			return Effect;
		}

		static bool IsAllowed(const UGameplayEffect& Effect)
		{
			return VeyraAttributePolicy::Check(Effect).IsEmpty();
		}

		TEST_METHOD(EveryVeyraAttributeHasARule)
		{
			TArray<FString> Missing;
			for (const UClass* SetClass : { UVeyraVitalsSet::StaticClass(), UVeyraOffenceSet::StaticClass(), UVeyraDefenceSet::StaticClass() })
			{
				for (TFieldIterator<FProperty> It(SetClass); It; ++It)
				{
					if (FGameplayAttribute::IsGameplayAttributeDataProperty(*It) && !VeyraAttributePolicy::RuleFor(FGameplayAttribute(*It)).IsSet())
					{
						Missing.Add(FString::Printf(TEXT("%s.%s"), *SetClass->GetName(), *It->GetName()));
					}
				}
			}
			ASSERT_THAT(IsTrue(Missing.IsEmpty(), FString::Printf(TEXT("No rule for: %s"), *FString::Join(Missing, TEXT(", ")))));
		}

		TEST_METHOD(EveryVeyraEffectDefinitionIsAllowed)
		{
			int32 Checked = 0;
			TArray<FString> Problems;
			for (TObjectIterator<UClass> It; It; ++It)
			{
				if (!It->IsChildOf(UGameplayEffect::StaticClass()) || !It->GetPathName().StartsWith(TEXT("/Script/Veyra")))
				{
					continue;
				}
				++Checked;
				for (const FString& Problem : VeyraAttributePolicy::Check(*It->GetDefaultObject<UGameplayEffect>()))
				{
					Problems.Add(FString::Printf(TEXT("%s: %s"), *It->GetName(), *Problem));
				}
			}
			ASSERT_THAT(IsTrue(Checked > 0, TEXT("Found no Veyra Gameplay Effect classes")));
			ASSERT_THAT(IsTrue(Problems.IsEmpty(), FString::Join(Problems, TEXT(" | "))));
		}

		TEST_METHOD(AllowsFlatAndCompoundModifiersOnAStat)
		{
			ASSERT_THAT(IsTrue(IsAllowed(*NewEffect(EGameplayEffectDurationType::Infinite, UVeyraDefenceSet::GetArmorAttribute(), EGameplayModOp::AddBase))));
			ASSERT_THAT(IsTrue(IsAllowed(*NewEffect(EGameplayEffectDurationType::HasDuration, UVeyraDefenceSet::GetArmorAttribute(), EGameplayModOp::MultiplyCompound))));
			ASSERT_THAT(IsTrue(IsAllowed(*NewEffect(EGameplayEffectDurationType::Infinite, UVeyraOffenceSet::GetPhysicalPenetrationRetainedAttribute(), EGameplayModOp::MultiplyCompound))));
		}

		TEST_METHOD(RejectsAdditivePercentagesOverridesAndFinalAdds)
		{
			for (const EGameplayModOp::Type Operation : { EGameplayModOp::MultiplyAdditive, EGameplayModOp::DivideAdditive, EGameplayModOp::Override, EGameplayModOp::AddFinal })
			{
				ASSERT_THAT(IsFalse(IsAllowed(*NewEffect(EGameplayEffectDurationType::Infinite, UVeyraDefenceSet::GetArmorAttribute(), Operation)),
					FString::Printf(TEXT("%s on Armor was allowed"), *EGameplayModOpToString(Operation))));
			}
		}

		TEST_METHOD(RejectsTheWrongKindOfModifierForAnAttribute)
		{
			ASSERT_THAT(IsFalse(IsAllowed(*NewEffect(EGameplayEffectDurationType::Infinite, UVeyraOffenceSet::GetPhysicalPenetrationRetainedAttribute(), EGameplayModOp::AddBase))));
			ASSERT_THAT(IsFalse(IsAllowed(*NewEffect(EGameplayEffectDurationType::Infinite, UVeyraDefenceSet::GetArmorReductionFlatAttribute(), EGameplayModOp::MultiplyCompound))));
		}

		TEST_METHOD(RejectsModifiersOnHealthAndMetaAttributes)
		{
			ASSERT_THAT(IsFalse(IsAllowed(*NewEffect(EGameplayEffectDurationType::Infinite, UVeyraVitalsSet::GetHealthAttribute(), EGameplayModOp::AddBase))));
			ASSERT_THAT(IsFalse(IsAllowed(*NewEffect(EGameplayEffectDurationType::Instant, UVeyraVitalsSet::GetIncomingTrueDamageAttribute(), EGameplayModOp::AddBase))));
		}

		TEST_METHOD(RejectsInstantModifiersOnStats)
		{
			// An instant modifier would change the base value for good, ahead of later flat bonuses.
			ASSERT_THAT(IsFalse(IsAllowed(*NewEffect(EGameplayEffectDurationType::Instant, UVeyraDefenceSet::GetArmorAttribute(), EGameplayModOp::AddBase))));
		}
	};

	// Veyra.Combat.CombatTuning.*: the Combat domain's committed tuning loads through its owner.
	TEST_CLASS(CombatTuning, "Veyra.Combat")
	{
		TEST_METHOD(TheCommittedFileLoads)
		{
			UVeyraCombatTuningSubsystem* Subsystem = GEngine ? GEngine->GetEngineSubsystem<UVeyraCombatTuningSubsystem>() : nullptr;
			ASSERT_THAT(IsNotNull(Subsystem));
			ASSERT_THAT(IsTrue(Subsystem->IsLoaded(), TEXT("Combat tuning did not load at startup")));

			const VeyraTuning::FErrors Errors = Subsystem->Reload();
			ASSERT_THAT(IsTrue(Errors.IsEmpty(), FString::Join(Errors, TEXT(" | "))));
			ASSERT_THAT(IsTrue(UVeyraCombatTuningSubsystem::Get().Resistance.MitigationConstant > 0.0));

			VeyraTuning::FDomainFiles Files;
			ASSERT_THAT(IsTrue(VeyraTuning::ReadDomainFiles(UVeyraCombatTuningSubsystem::Domain, Files).IsEmpty()));
			ASSERT_THAT(IsTrue(Subsystem->GetDocumentHash() == Files.DocumentHash, TEXT("The subsystem's hash is not Combat.json's")));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
