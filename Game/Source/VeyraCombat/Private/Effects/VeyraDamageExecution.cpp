// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Effects/VeyraDamageExecution.h"

#include "Statuses/VeyraStatusComponent.h"
#include "AbilitySystemComponent.h"
#include "Attributes/VeyraDefenceSet.h"
#include "Attributes/VeyraOffenceSet.h"
#include "Attributes/VeyraVitalsSet.h"
#include "Damage/VeyraDamageResolver.h"
#include "Effects/VeyraCombatEffects.h"
#include "Tuning/VeyraCombatTuningSubsystem.h"
#include "Units/VeyraUnit.h"
#include "VeyraCombatLog.h"
#include "VeyraCombatTagMapping.h"

namespace
{
	/** The attributes the execution reads. The attacker's are fixed when the damage is created. */
	struct FDamageCaptures
	{
		FGameplayEffectAttributeCaptureDefinition OutgoingDamageMultiplier;
		FGameplayEffectAttributeCaptureDefinition PhysicalPenetrationFlat;
		FGameplayEffectAttributeCaptureDefinition PhysicalPenetrationRetained;
		FGameplayEffectAttributeCaptureDefinition MagicPenetrationFlat;
		FGameplayEffectAttributeCaptureDefinition MagicPenetrationRetained;
		FGameplayEffectAttributeCaptureDefinition Armor;
		FGameplayEffectAttributeCaptureDefinition MagicResist;
		FGameplayEffectAttributeCaptureDefinition ArmorReductionFlat;
		FGameplayEffectAttributeCaptureDefinition ArmorReductionRetained;
		FGameplayEffectAttributeCaptureDefinition MagicResistReductionFlat;
		FGameplayEffectAttributeCaptureDefinition MagicResistReductionRetained;
		FGameplayEffectAttributeCaptureDefinition IncomingDamageMultiplier;

		FDamageCaptures()
		{
			const auto FromAttacker = [](const FGameplayAttribute& Attribute)
			{
				return FGameplayEffectAttributeCaptureDefinition(Attribute, EGameplayEffectAttributeCaptureSource::Source, /*bSnapshot*/ true);
			};
			const auto FromDefender = [](const FGameplayAttribute& Attribute)
			{
				return FGameplayEffectAttributeCaptureDefinition(Attribute, EGameplayEffectAttributeCaptureSource::Target, /*bSnapshot*/ false);
			};
			OutgoingDamageMultiplier = FromAttacker(UVeyraOffenceSet::GetOutgoingDamageMultiplierAttribute());
			PhysicalPenetrationFlat = FromAttacker(UVeyraOffenceSet::GetPhysicalPenetrationFlatAttribute());
			PhysicalPenetrationRetained = FromAttacker(UVeyraOffenceSet::GetPhysicalPenetrationRetainedAttribute());
			MagicPenetrationFlat = FromAttacker(UVeyraOffenceSet::GetMagicPenetrationFlatAttribute());
			MagicPenetrationRetained = FromAttacker(UVeyraOffenceSet::GetMagicPenetrationRetainedAttribute());
			Armor = FromDefender(UVeyraDefenceSet::GetArmorAttribute());
			MagicResist = FromDefender(UVeyraDefenceSet::GetMagicResistAttribute());
			ArmorReductionFlat = FromDefender(UVeyraDefenceSet::GetArmorReductionFlatAttribute());
			ArmorReductionRetained = FromDefender(UVeyraDefenceSet::GetArmorReductionRetainedAttribute());
			MagicResistReductionFlat = FromDefender(UVeyraDefenceSet::GetMagicResistReductionFlatAttribute());
			MagicResistReductionRetained = FromDefender(UVeyraDefenceSet::GetMagicResistReductionRetainedAttribute());
			IncomingDamageMultiplier = FromDefender(UVeyraDefenceSet::GetIncomingDamageMultiplierAttribute());
		}

		TArray<const FGameplayEffectAttributeCaptureDefinition*> All() const
		{
			return { &OutgoingDamageMultiplier, &PhysicalPenetrationFlat, &PhysicalPenetrationRetained, &MagicPenetrationFlat,
				&MagicPenetrationRetained, &Armor, &MagicResist, &ArmorReductionFlat, &ArmorReductionRetained,
				&MagicResistReductionFlat, &MagicResistReductionRetained, &IncomingDamageMultiplier };
		}
	};

	const FDamageCaptures& Captures()
	{
		static const FDamageCaptures Instance;
		return Instance;
	}

	/** Reads the spec's damage components; every SetByCaller magnitude must be a damage type. */
	void ReadDamage(const FGameplayEffectSpec& Spec, FVeyraRawDamageEvent& OutRaw, TArray<FString>& OutProblems)
	{
		for (const TPair<FGameplayTag, float>& Magnitude : Spec.SetByCallerTagMagnitudes)
		{
			const TOptional<EVeyraDamageType> Type = VeyraCombatTagMapping::DamageTypeFromTag(Magnitude.Key);
			if (!Type.IsSet())
			{
				OutProblems.Add(FString::Printf(TEXT("\"%s\" is not a damage type"), *Magnitude.Key.ToString()));
			}
			else if (!FMath::IsFinite(Magnitude.Value) || Magnitude.Value < 0.0f)
			{
				OutProblems.Add(FString::Printf(TEXT("%s damage is %g; it must be a finite amount of at least 0"), *Magnitude.Key.ToString(), Magnitude.Value));
			}
			else
			{
				OutRaw.Components.Add({ Type.GetValue(), Magnitude.Value });
			}
		}
		if (OutRaw.Components.IsEmpty() && OutProblems.IsEmpty())
		{
			OutProblems.Add(TEXT("the damage has no components"));
		}
	}
}

UVeyraDamageExecution::UVeyraDamageExecution()
{
	for (const FGameplayEffectAttributeCaptureDefinition* Definition : Captures().All())
	{
		RelevantAttributesToCapture.Add(*Definition);
	}
}

void UVeyraDamageExecution::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams,
	FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();
	TArray<FString> Problems;

	FVeyraRawDamageEvent Raw;
	ReadDamage(Spec, Raw, Problems);

	// A missing set would read as 0 and silently change the result, so both sides must have theirs.
	const UAbilitySystemComponent* Attacker = ExecutionParams.GetSourceAbilitySystemComponent();
	const UAbilitySystemComponent* Defender = ExecutionParams.GetTargetAbilitySystemComponent();
	if (!Attacker || !Attacker->GetSet<UVeyraOffenceSet>())
	{
		Problems.Add(TEXT("the attacker has no UVeyraOffenceSet"));
	}
	if (!Defender || !Defender->GetSet<UVeyraDefenceSet>() || !Defender->GetSet<UVeyraVitalsSet>())
	{
		Problems.Add(TEXT("the defender lacks UVeyraDefenceSet or UVeyraVitalsSet"));
	}

	FAggregatorEvaluateParameters Evaluation;
	Evaluation.SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	Evaluation.TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();
	const auto Read = [&ExecutionParams, &Evaluation, &Problems](const FGameplayEffectAttributeCaptureDefinition& Definition) -> double
	{
		float Value = 0.0f;
		if (!ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(Definition, Evaluation, Value))
		{
			Problems.Add(FString::Printf(TEXT("%s could not be read"), *Definition.AttributeToCapture.GetName()));
		}
		return Value;
	};

	// The event's own penetration joins the attacker's: flat values add, retained fractions multiply (§3).
	// An event without its own adds nothing.
	const FVeyraPenetration Identity;
	const auto EventValue = [&Spec](const FName Name, double Default) {
		return static_cast<double>(Spec.GetSetByCallerMagnitude(Name, /*WarnIfNotFound*/ false, static_cast<float>(Default)));
	};

	const FDamageCaptures& Capture = Captures();
	FVeyraAttackerOffence Offence;
	Offence.OutgoingDamageMultiplier = Read(Capture.OutgoingDamageMultiplier);
	Offence.PhysicalPenetration = { Read(Capture.PhysicalPenetrationFlat) + EventValue(UVeyraDamageEffect::PhysicalPenetrationFlatName, Identity.Flat),
		Read(Capture.PhysicalPenetrationRetained) * EventValue(UVeyraDamageEffect::PhysicalPenetrationRetainedName, Identity.Retained) };
	Offence.MagicPenetration = { Read(Capture.MagicPenetrationFlat) + EventValue(UVeyraDamageEffect::MagicPenetrationFlatName, Identity.Flat),
		Read(Capture.MagicPenetrationRetained) * EventValue(UVeyraDamageEffect::MagicPenetrationRetainedName, Identity.Retained) };

	FVeyraDefenderDefence Defence;
	Defence.Armor = Read(Capture.Armor);
	Defence.MagicResist = Read(Capture.MagicResist);
	Defence.ArmorReduction = { Read(Capture.ArmorReductionFlat), Read(Capture.ArmorReductionRetained) };
	Defence.MagicResistReduction = { Read(Capture.MagicResistReductionFlat), Read(Capture.MagicResistReductionRetained) };
	Defence.IncomingDamageMultiplier = Read(Capture.IncomingDamageMultiplier);

	if (!Problems.IsEmpty())
	{
		UE_LOG(LogVeyraCombat, Error, TEXT("Damage from %s to %s was not applied: %s."), *GetNameSafe(Attacker ? Attacker->GetOwner() : nullptr),
			*GetNameSafe(Defender ? Defender->GetOwner() : nullptr), *FString::Join(Problems, TEXT("; ")));
		return;
	}

	// A guard facing its attacker takes less (DirectionalDamageReduction, ADR-018 §2). A projectile is
	// judged from where its source stands as it lands.
	const AActor* DefenderBody = Defender->GetAvatarActor();
	const AActor* AttackerBody = Attacker->GetAvatarActor();
	const UVeyraStatusComponent* Guards = Defender->GetOwner() ? Defender->GetOwner()->FindComponentByClass<UVeyraStatusComponent>() : nullptr;
	if (DefenderBody && AttackerBody && Guards)
	{
		Defence.IncomingDamageMultiplier *= Guards->GetDirectionalRetained(DefenderBody->GetActorForwardVector(), AttackerBody->GetActorLocation() - DefenderBody->GetActorLocation());
	}

	// A ward counts hits, not damage: every blow that reaches it takes one point of its Health, whatever
	// its amount and modifiers (ADR-016 §6). Only a Vanguard's basic attack gets this far. A placed
	// marker counts them the same way, from anything of its enemies' (ADR-030 §5).
	if (VeyraUnits::IsWard(Defender->GetOwner()) || VeyraUnits::IsMarker(Defender->GetOwner()))
	{
		constexpr float OneHit = 1.0f;
		OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(UVeyraVitalsSet::GetIncomingDamageAttribute(EVeyraDamageType::TrueDamage),
			EGameplayModOp::AddBase, OneHit));
		return;
	}

	// §33: a structure's Armor and Magic Resist are its own; penetration and resistance reduction do
	// not touch them.
	if (VeyraUnits::IsStructure(Defender->GetOwner()))
	{
		Offence.PhysicalPenetration = Identity;
		Offence.MagicPenetration = Identity;
		Defence.ArmorReduction = FVeyraResistanceReduction();
		Defence.MagicResistReduction = FVeyraResistanceReduction();
	}

	const FVeyraMitigatedDamage Mitigated = VeyraDamage::ApplyTargetSide(VeyraDamage::ApplySourceSide(Raw, Offence), Defence,
		UVeyraCombatTuningSubsystem::Get().Resistance);

	// One output per component, always in the same order, so absorption is deterministic.
	for (const EVeyraDamageType Type : { EVeyraDamageType::Physical, EVeyraDamageType::Magic, EVeyraDamageType::TrueDamage })
	{
		for (const FVeyraDamageComponent& Component : Mitigated.Components)
		{
			if (Component.Type == Type)
			{
				OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(UVeyraVitalsSet::GetIncomingDamageAttribute(Type),
					EGameplayModOp::AddBase, static_cast<float>(Component.Amount)));
			}
		}
	}
}
