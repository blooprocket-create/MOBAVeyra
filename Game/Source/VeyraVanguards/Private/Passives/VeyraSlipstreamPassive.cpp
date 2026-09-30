// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Passives/VeyraSlipstreamPassive.h"

#include "AbilitySystemComponent.h"
#include "Delivery/VeyraAreaDelivery.h"
#include "Delivery/VeyraEffectDelivery.h"
#include "Engine/World.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Targeting/VeyraTargeting.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "Tuning/VeyraVanguardsTuningSubsystem.h"

void UVeyraSlipstreamPassive::Start(UAbilitySystemComponent& Owner, const FVeyraContentId& InPassiveId)
{
	Super::Start(Owner, InPassiveId);
	if (UVeyraCombatEventSubsystem* Events = GetWorld() ? GetWorld()->GetSubsystem<UVeyraCombatEventSubsystem>() : nullptr)
	{
		CastHandle = Events->OnCastCommitted.AddUObject(this, &UVeyraSlipstreamPassive::OnCastCommitted);
	}
}

void UVeyraSlipstreamPassive::Stop()
{
	if (UVeyraCombatEventSubsystem* Events = GetWorld() ? GetWorld()->GetSubsystem<UVeyraCombatEventSubsystem>() : nullptr)
	{
		Events->OnCastCommitted.Remove(CastHandle);
	}
	CastHandle.Reset();
	Super::Stop();
}

void UVeyraSlipstreamPassive::OnCastCommitted(const FVeyraCastEvent& Event)
{
	UAbilitySystemComponent* Owner = OwnerAbilitySystem.Get();
	const FVeyraSlipstreamTuning* Tuning = UVeyraVanguardsTuningSubsystem::FindSlipstream(PassiveId);
	UWorld* World = GetWorld();
	const AActor* Body = Owner ? Owner->GetAvatarActor() : nullptr;
	const AActor* Ally = Event.Target.Get();
	if (!Owner || !Tuning || !World || !Body || !Ally || Event.Caster.Get() != Owner)
	{
		return;
	}
	// Only her ally-targeted buffs, cast at a living allied Vanguard other than herself (§24).
	const FVeyraSelfBuffAbilityTuning* Buff = UVeyraAbilitiesTuningSubsystem::FindSelfBuff(Event.Ability);
	const EVeyraTargetValidity Validity = VeyraTargeting::CheckAllyTarget(*Body, Ally, Buff ? Buff->Cast.CastRange : 0.0);
	if (!Buff || Buff->Recipient != EVeyraBuffRecipient::CasterOrAlly
		|| (Validity != EVeyraTargetValidity::Valid && Validity != EVeyraTargetValidity::OutOfRange))
	{
		return;
	}
	const FVector From = Body->GetActorLocation();
	const FVector Toward = Ally->GetActorLocation() - From;
	const double Length = FMath::Min(Toward.Size2D(), Tuning->MaxLength);
	if (!(Length > 0.0))
	{
		return;
	}
	// A rectangle from her toward the ally, lasting a little while and speeding those inside.
	FVeyraPreparedLinger Current;
	Current.Shape.Kind = EVeyraShapeKind::Rectangle;
	Current.Shape.Length = Length;
	Current.Shape.Width = Tuning->Width;
	Current.DurationSeconds = Tuning->DurationSeconds;
	Current.PulseSeconds = Tuning->PulseSeconds;
	Current.Ability = PassiveId;
	Current.Statuses.Caster = VeyraEffectDelivery::StatusSpecs(Tuning->Statuses);
	Current.Statuses.Allies = Current.Statuses.Caster;
	FVeyraEffectFrame Placement;
	Placement.Origin = From;
	Placement.Direction = Toward.GetSafeNormal2D();
	VeyraAreaDelivery::ArmLinger(*World, *Owner, Placement, Current);
}
