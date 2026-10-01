// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Abilities/VeyraGameplayAbility.h"

#include "Units/VeyraUnit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Attacks/VeyraBasicAttackComponent.h"
#include "Casting/VeyraCastStateComponent.h"
#include "Casting/VeyraCastSubsystem.h"
#include "Cooldowns/VeyraCooldownComponent.h"
#include "Engine/World.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Loadout/VeyraAbilityLoadoutComponent.h"
#include "Progression/VeyraProgressionComponent.h"
#include "Statuses/VeyraStatusComponent.h"
#include "Targeting/VeyraTargeting.h"
#include "TimerManager.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "VeyraAbilitiesLog.h"
#include "VeyraCombatVerbs.h"

namespace
{
	template <typename ComponentType>
	ComponentType* FindBesideAbilitySystem(const FGameplayAbilityActorInfo* ActorInfo)
	{
		const AActor* Owner = ActorInfo ? ActorInfo->OwnerActor.Get() : nullptr;
		return Owner ? Owner->FindComponentByClass<ComponentType>() : nullptr;
	}

	template <typename ComponentType>
	ComponentType* FindBesideAbilitySystem(const UAbilitySystemComponent& AbilitySystem)
	{
		const AActor* Owner = AbilitySystem.GetOwner();
		return Owner ? Owner->FindComponentByClass<ComponentType>() : nullptr;
	}

	/**
	 * Which Haste shortens Ability's cooldown (Combat Bible §21): an item's Active cools down as the
	 * item's, never by Ability Haste, and a Flux Spell's cooldown is fixed.
	 */
	EVeyraCooldownHaste HasteOf(const UAbilitySystemComponent& Caster, const FVeyraContentId& Ability)
	{
		const UVeyraAbilityLoadoutComponent* Loadout = FindBesideAbilitySystem<UVeyraAbilityLoadoutComponent>(Caster);
		const FVeyraLoadoutEntry* Entry = Loadout ? Loadout->FindAbility(Ability) : nullptr;
		if (Entry && VeyraAbilitySlots::IsSpellSlot(Entry->Slot))
		{
			return EVeyraCooldownHaste::Fixed;
		}
		return Entry && VeyraAbilitySlots::IsItemSlot(Entry->Slot) ? EVeyraCooldownHaste::Item : EVeyraCooldownHaste::Ability;
	}

	/** The ID Ability cools down under: its slot's own ability's, for a variant that shares it (ADR-018 §1). */
	FVeyraContentId CooldownIdOf(const UAbilitySystemComponent& Caster, const FVeyraContentId& Ability)
	{
		const UVeyraAbilityLoadoutComponent* Loadout = FindBesideAbilitySystem<UVeyraAbilityLoadoutComponent>(Caster);
		return Loadout ? Loadout->CooldownIdOf(Ability) : Ability;
	}

	/** The cast's target from its activation's event data, where VeyraAbilities::TryCast put it. */
	FVeyraCastTarget CastTargetFrom(const FGameplayEventData* EventData)
	{
		FVeyraCastTarget Target;
		if (!EventData)
		{
			return Target;
		}
		Target.Actor = const_cast<AActor*>(EventData->Target.Get());
		const FGameplayAbilityTargetData* Data = EventData->TargetData.Get(0);
		if (Data && Data->HasEndPoint())
		{
			Target.bHasLocation = true;
			Target.Location = Data->GetEndPoint();
		}
		return Target;
	}
}

UVeyraGameplayAbility::UVeyraGameplayAbility()
{
	// Server-only abilities (ADR-006 §7): the client sends a cast intent and the server runs it.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	NetSecurityPolicy = EGameplayAbilityNetSecurityPolicy::ServerOnly;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

EVeyraCastRejection UVeyraGameplayAbility::CheckCast(const UAbilitySystemComponent& Caster, const FVeyraContentId& Ability, const FVeyraCastTarget& Target) const
{
	const AActor* Avatar = Caster.GetAvatarActor();
	if (!Defines(Ability))
	{
		return EVeyraCastRejection::UnknownAbility;
	}
	if (!Avatar || !VeyraTargeting::IsAlive(Avatar))
	{
		return EVeyraCastRejection::CasterDead;
	}
	const EVeyraActionBlocks Blocks = VeyraCombat::GetActionBlocks(Caster);
	if (EnumHasAnyFlags(Blocks, EVeyraActionBlocks::Cast) || (EnumHasAnyFlags(Blocks, EVeyraActionBlocks::Dash) && MovesCaster(Ability)))
	{
		return EVeyraCastRejection::CrowdControlled;
	}
	// Ending a lasting effect early is free and ignores the cooldown (ADR-008 §9).
	if (EndsEarlyOnRecast(Caster, Ability))
	{
		return EVeyraCastRejection::None;
	}
	const int32 Rank = GetRank(Caster, Ability);
	if (Rank < 1)
	{
		return EVeyraCastRejection::NotLearned;
	}
	const UVeyraCastStateComponent* CastState = FindBesideAbilitySystem<UVeyraCastStateComponent>(Caster);
	if (CastState && CastState->IsBusy())
	{
		return EVeyraCastRejection::Busy;
	}
	const UVeyraCooldownComponent* Cooldowns = FindBesideAbilitySystem<UVeyraCooldownComponent>(Caster);
	if (!Cooldowns || Cooldowns->GetRemainingSecondsNow(CooldownIdOf(Caster, Ability)) > 0.0)
	{
		return EVeyraCastRejection::OnCooldown;
	}
	if (!VeyraCombat::CanAffordResource(Caster, GetResourceCost(Ability, Rank)))
	{
		return EVeyraCastRejection::InsufficientResource;
	}
	return CheckTarget(*Avatar, Ability, Target);
}

EVeyraCastRejection UVeyraGameplayAbility::CheckTarget(const AActor& /*Caster*/, const FVeyraContentId& /*Ability*/, const FVeyraCastTarget& /*Target*/) const
{
	return EVeyraCastRejection::None;
}

const FVeyraCastTuning* UVeyraGameplayAbility::GetCastTuning(const FVeyraContentId& /*Ability*/) const
{
	return nullptr;
}

bool UVeyraGameplayAbility::EndsEarlyOnRecast(const UAbilitySystemComponent& /*Caster*/, const FVeyraContentId& /*Ability*/) const
{
	return false;
}

void UVeyraGameplayAbility::EndEarly(UAbilitySystemComponent& /*Caster*/, const FVeyraContentId& /*Ability*/)
{
}

FVeyraChannelPlan UVeyraGameplayAbility::Deliver(const FVeyraCast& /*Cast*/)
{
	return FVeyraChannelPlan();
}

bool UVeyraGameplayAbility::IsOffensive(const FVeyraContentId& /*Ability*/) const
{
	return true;
}

void UVeyraGameplayAbility::NoteCastStarted(UAbilitySystemComponent& Caster, const FVeyraContentId& Ability) const
{
	const bool bOffensive = IsOffensive(Ability);
	if (UVeyraCombatEventSubsystem* Events = GetWorld() ? GetWorld()->GetSubsystem<UVeyraCombatEventSubsystem>() : nullptr)
	{
		Events->OnCastStarted.Broadcast(FVeyraCastEvent{ &Caster, Ability, bOffensive });
	}
	if (bOffensive)
	{
		VeyraCombat::EndCamouflage(Caster);
	}
}

void UVeyraGameplayAbility::NoteCastCommitted(UAbilitySystemComponent& Caster, const FVeyraContentId& Ability, AActor* Target) const
{
	if (UVeyraCombatEventSubsystem* Events = GetWorld() ? GetWorld()->GetSubsystem<UVeyraCombatEventSubsystem>() : nullptr)
	{
		Events->OnCastCommitted.Broadcast(FVeyraCastEvent{ &Caster, Ability, IsOffensive(Ability), Target });
	}
	UVeyraAbilityLoadoutComponent* Loadout = FindBesideAbilitySystem<UVeyraAbilityLoadoutComponent>(Caster);
	if (!Loadout)
	{
		return;
	}
	// Read before NoteCommitted, which may end the override that holds it.
	const FVeyraLoadoutEntry* Entry = Loadout->FindAbility(Ability);
	const TOptional<EVeyraAbilitySlot> Slot = Entry ? TOptional<EVeyraAbilitySlot>(Entry->Slot) : TOptional<EVeyraAbilitySlot>();
	// A follow-up is used once; this cast may open one of its own in its slot.
	Loadout->NoteCommitted(Caster, Ability);
	const FVeyraCastTuning* CastTuning = GetCastTuning(Ability);
	if (Slot.IsSet() && CastTuning && !CastTuning->RecastWindow.IsEmpty())
	{
		const FVeyraRecastTuning& Recast = CastTuning->RecastWindow[0];
		FVeyraOverrideSpec FollowUp;
		FollowUp.Ability = Recast.Ability;
		FollowUp.DurationSeconds = Recast.WindowSeconds;
		FollowUp.Use = EVeyraOverrideUse::Once;
		FollowUp.bCastOnExpiry = Recast.OnExpiry == EVeyraRecastExpiry::Cast;
		Loadout->Override(Caster, Slot.GetValue(), FollowUp);
	}
}

void UVeyraGameplayAbility::DeliverChannelTick(const FVeyraCast& /*Cast*/, int32 /*Tick*/)
{
}

EVeyraCastRejection UVeyraGameplayAbility::CheckEnemyUnit(const AActor& Caster, const AActor* Target, double CastRange, TConstArrayView<EVeyraUnitKind> Kinds)
{
	switch (VeyraTargeting::CheckEnemyTarget(Caster, Target, CastRange))
	{
	case EVeyraTargetValidity::Valid:
	{
		// It may be for some kinds of unit only, as a Smite is for monsters (ADR-015 §3).
		const TOptional<EVeyraUnitKind> Kind = VeyraUnits::KindOf(Target);
		const bool bKindAllowed = Kinds.IsEmpty() || (Kind.IsSet() && Kinds.Contains(Kind.GetValue()));
		return bKindAllowed ? EVeyraCastRejection::None : EVeyraCastRejection::InvalidTarget;
	}
	case EVeyraTargetValidity::Dead:
		return EVeyraCastRejection::TargetDead;
	case EVeyraTargetValidity::NotHostile:
		return EVeyraCastRejection::NotHostile;
	case EVeyraTargetValidity::OutOfRange:
		return EVeyraCastRejection::OutOfRange;
	case EVeyraTargetValidity::NotVisible:
		return EVeyraCastRejection::NotVisible;
	case EVeyraTargetValidity::NotACombatant:
	case EVeyraTargetValidity::Caster:
	case EVeyraTargetValidity::Structure:
	case EVeyraTargetValidity::Ward:
	case EVeyraTargetValidity::NotAllied:
		return EVeyraCastRejection::InvalidTarget;
	}
	return EVeyraCastRejection::InvalidTarget;
}

void UVeyraGameplayAbility::EndRecastWindow(UAbilitySystemComponent& Caster, const FVeyraContentId& Ability) const
{
	const FVeyraCastTuning* CastTuning = GetCastTuning(Ability);
	UVeyraAbilityLoadoutComponent* Loadout = FindBesideAbilitySystem<UVeyraAbilityLoadoutComponent>(Caster);
	if (!CastTuning || CastTuning->RecastWindow.IsEmpty() || !Loadout)
	{
		return;
	}
	const FVeyraContentId& FollowUp = CastTuning->RecastWindow[0].Ability;
	const FVeyraLoadoutEntry* Entry = Loadout->FindAbility(FollowUp);
	const FVeyraLoadoutEntry* Current = Entry ? Loadout->FindSlot(Entry->Slot) : nullptr;
	if (Current && Current->Ability == FollowUp)
	{
		Loadout->EndOverride(Caster, Entry->Slot);
	}
}

bool UVeyraGameplayAbility::HasUsablePoint(const FVeyraCastTarget& Target)
{
	return Target.bHasLocation && !Target.Location.ContainsNaN() && FMath::IsFinite(Target.Location.X) && FMath::IsFinite(Target.Location.Y)
		&& FMath::IsFinite(Target.Location.Z);
}

int32 UVeyraGameplayAbility::GetRank(const UAbilitySystemComponent& Caster, const FVeyraContentId& Ability) const
{
	const UVeyraAbilityLoadoutComponent* Loadout = FindBesideAbilitySystem<UVeyraAbilityLoadoutComponent>(Caster);
	const FVeyraLoadoutEntry* Entry = Loadout ? Loadout->FindAbility(Ability) : nullptr;
	// An item's Active and a Flux Spell have no ranks: each works at its one rank (ADR-012 §1, ADR-015 §1).
	if (Entry && (VeyraAbilitySlots::IsItemSlot(Entry->Slot) || VeyraAbilitySlots::IsSpellSlot(Entry->Slot)))
	{
		return 1;
	}
	const UVeyraProgressionComponent* Progression = FindBesideAbilitySystem<UVeyraProgressionComponent>(Caster);
	return Entry && Progression ? Progression->GetRank(Entry->Slot) : 0;
}

int32 UVeyraGameplayAbility::GetCasterLevel(const UAbilitySystemComponent& Caster)
{
	const UVeyraProgressionComponent* Progression = FindBesideAbilitySystem<UVeyraProgressionComponent>(Caster);
	return Progression ? FMath::Max(1, Progression->GetLevel()) : 1;
}

int32 UVeyraGameplayAbility::GetCommitRank(const UAbilitySystemComponent& Caster, const FVeyraContentId& Ability) const
{
	return Running.IsSet() && Running->Cast.Ability == Ability ? Running->Cast.Rank : GetRank(Caster, Ability);
}

FVeyraContentId UVeyraGameplayAbility::GetContentId(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo) const
{
	const UVeyraAbilityLoadoutComponent* Loadout = FindBesideAbilitySystem<UVeyraAbilityLoadoutComponent>(ActorInfo);
	const FVeyraLoadoutEntry* Entry = Loadout ? Loadout->FindHandle(Handle) : nullptr;
	return Entry ? Entry->Ability : FVeyraContentId();
}

void UVeyraGameplayAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	const FVeyraContentId Ability = GetContentId(Handle, ActorInfo);
	const FVeyraCastTuning* Tuning = GetCastTuning(Ability);
	UAbilitySystemComponent* Caster = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	UWorld* World = GetWorld();
	if (!Tuning || !Caster || !Avatar || !World || Running.IsSet())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility*/ true, /*bWasCancelled*/ true);
		return;
	}
	if (EndsEarlyOnRecast(*Caster, Ability))
	{
		EndEarly(*Caster, Ability);
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility*/ true, /*bWasCancelled*/ false);
		return;
	}
	const FVeyraCastTarget Target = CastTargetFrom(TriggerEventData);
	if (CheckTarget(*Avatar, Ability, Target) != EVeyraCastRejection::None)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility*/ true, /*bWasCancelled*/ true);
		return;
	}
	// Casting cancels a basic attack before its Commit, and cuts a backswing short (Combat Bible §48).
	if (UVeyraBasicAttackComponent* Attacks = FindBesideAbilitySystem<UVeyraBasicAttackComponent>(*Caster))
	{
		Attacks->CancelAttack();
	}

	FRunningCast& Run = Running.Emplace();
	Run.Handle = Handle;
	Run.ActorInfo = ActorInfo;
	Run.ActivationInfo = ActivationInfo;
	FVeyraCast& Cast = Run.Cast;
	Cast.Ability = Ability;
	Cast.Rank = GetRank(*Caster, Ability);
	Cast.CastId = World->GetSubsystem<UVeyraCastSubsystem>()->IssueCastId();
	Cast.Caster = Caster;
	Cast.TargetActor = Target.Actor;
	Cast.CasterLocation = Avatar->GetActorLocation();
	// A point beyond the cast range is brought back within it (ADR-008 §9).
	FVector Offset = Target.bHasLocation ? Target.Location - Cast.CasterLocation : FVector::ZeroVector;
	Offset.Z = 0.0;
	if (Tuning->CastRange > 0.0 && Offset.Size() > Tuning->CastRange)
	{
		Offset = Offset.GetSafeNormal() * Tuning->CastRange;
	}
	Cast.Point = Cast.CasterLocation + Offset;
	Cast.Direction = Offset.IsNearlyZero() ? Avatar->GetActorForwardVector().GetSafeNormal2D() : Offset.GetSafeNormal();

	if (UVeyraStatusComponent* Statuses = FindBesideAbilitySystem<UVeyraStatusComponent>(*Caster))
	{
		InterruptedHandle = Statuses->OnInterrupted.AddUObject(this, &UVeyraGameplayAbility::OnCasterInterrupted);
	}
	UE_LOG(LogVeyraAbilities, Verbose, TEXT("%s begins %s at rank %d (cast %d)."), *GetNameSafe(Avatar), *Ability.ToString(), Cast.Rank, Cast.CastId);
	NoteCastStarted(*Caster, Ability);
	if (Tuning->WindupSeconds > 0.0)
	{
		VeyraCombat::SetCastLocksMovement(*Caster, Tuning->WindupMovement == EVeyraCastMovement::Locked);
		EnterPhase(EVeyraCastPhase::Windup, Tuning->WindupSeconds);
		World->GetTimerManager().SetTimer(PhaseTimer, FTimerDelegate::CreateUObject(this, &UVeyraGameplayAbility::OnWindupEnded),
			static_cast<float>(Tuning->WindupSeconds), /*bLoop*/ false);
	}
	else
	{
		OnWindupEnded();
	}
}

void UVeyraGameplayAbility::OnWindupEnded()
{
	FRunningCast& Run = Running.GetValue();
	UAbilitySystemComponent* Caster = Run.Cast.Caster.Get();
	const AActor* Avatar = Run.ActorInfo ? Run.ActorInfo->AvatarActor.Get() : nullptr;
	if (Caster)
	{
		VeyraCombat::SetCastLocksMovement(*Caster, false);
	}
	// A caster that died or lost its body during the windup casts nothing.
	if (!Caster || !Avatar || !VeyraTargeting::IsAlive(Avatar) || !CommitAbility(Run.Handle, Run.ActorInfo, Run.ActivationInfo))
	{
		FinishCast(/*bCancelled*/ true);
		return;
	}

	NoteCastCommitted(*Caster, Run.Cast.Ability, Run.Cast.TargetActor.Get());

	Run.Channel = Deliver(Run.Cast);
	if (Run.Channel.Ticks > 0 && Run.Channel.Seconds > 0.0)
	{
		VeyraCombat::SetCastLocksMovement(*Caster, Run.Channel.bLocksMovement);
		EnterPhase(EVeyraCastPhase::Channel, Run.Channel.Seconds);
		GetWorld()->GetTimerManager().SetTimer(PhaseTimer, FTimerDelegate::CreateUObject(this, &UVeyraGameplayAbility::OnChannelTick),
			static_cast<float>(Run.Channel.Seconds / Run.Channel.Ticks), /*bLoop*/ true);
		return;
	}
	BeginRecovery();
}

void UVeyraGameplayAbility::OnChannelTick()
{
	FRunningCast& Run = Running.GetValue();
	++Run.ChannelTicksDelivered;
	DeliverChannelTick(Run.Cast, Run.ChannelTicksDelivered);
	if (Running.IsSet() && Run.ChannelTicksDelivered >= Run.Channel.Ticks)
	{
		GetWorld()->GetTimerManager().ClearTimer(PhaseTimer);
		if (UAbilitySystemComponent* Caster = Run.Cast.Caster.Get())
		{
			VeyraCombat::SetCastLocksMovement(*Caster, false);
		}
		BeginRecovery();
	}
}

void UVeyraGameplayAbility::BeginRecovery()
{
	const FVeyraCastTuning* Tuning = GetCastTuning(Running->Cast.Ability);
	const double RecoverySeconds = Tuning ? Tuning->RecoverySeconds : 0.0;
	if (RecoverySeconds <= 0.0)
	{
		FinishCast(/*bCancelled*/ false);
		return;
	}
	EnterPhase(EVeyraCastPhase::Recovery, RecoverySeconds);
	GetWorld()->GetTimerManager().SetTimer(PhaseTimer, FTimerDelegate::CreateUObject(this, &UVeyraGameplayAbility::FinishCast, false),
		static_cast<float>(RecoverySeconds), /*bLoop*/ false);
}

void UVeyraGameplayAbility::OnCasterInterrupted()
{
	if (!Running.IsSet())
	{
		return;
	}
	switch (Running->Phase)
	{
	case EVeyraCastPhase::Windup:
	{
		// Before Commit nothing is paid, and part of the cooldown starts (Combat Bible §26).
		const FVeyraCast& Cast = Running->Cast;
		UAbilitySystemComponent* Caster = Cast.Caster.Get();
		UVeyraCooldownComponent* Cooldowns = Caster ? FindBesideAbilitySystem<UVeyraCooldownComponent>(*Caster) : nullptr;
		if (Cooldowns)
		{
			Cooldowns->StartCooldown(CooldownIdOf(*Caster, Cast.Ability), GetCooldownSeconds(Cast.Ability, Cast.Rank) * UVeyraAbilitiesTuningSubsystem::Get().Casting.InterruptedCooldownFraction,
				HasteOf(*Caster, Cast.Ability));
		}
		FinishCast(/*bCancelled*/ true);
		break;
	}
	case EVeyraCastPhase::Channel:
		// The rest of the channel is lost; what Commit paid stays paid (§26, §54).
		FinishCast(/*bCancelled*/ true);
		break;
	case EVeyraCastPhase::Recovery:
		FinishCast(/*bCancelled*/ false);
		break;
	case EVeyraCastPhase::None:
		break;
	}
}

void UVeyraGameplayAbility::EnterPhase(EVeyraCastPhase Phase, double Seconds)
{
	FRunningCast& Run = Running.GetValue();
	Run.Phase = Phase;
	UAbilitySystemComponent* Caster = Run.Cast.Caster.Get();
	if (UVeyraCastStateComponent* CastState = Caster ? FindBesideAbilitySystem<UVeyraCastStateComponent>(*Caster) : nullptr)
	{
		FVeyraCastState State;
		State.Ability = Run.Cast.Ability;
		State.CastId = Run.Cast.CastId;
		State.Phase = Phase;
		State.PhaseEndsAt = GetWorld()->GetTimeSeconds() + Seconds;
		State.Location = Run.Cast.Point;
		State.Direction = Run.Cast.Direction;
		CastState->SetState(State);
	}
}

void UVeyraGameplayAbility::FinishCast(bool bCancelled)
{
	if (!Running.IsSet())
	{
		return;
	}
	const FRunningCast Run = Running.GetValue();
	Running.Reset();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PhaseTimer);
	}
	if (UAbilitySystemComponent* Caster = Run.Cast.Caster.Get())
	{
		VeyraCombat::SetCastLocksMovement(*Caster, false);
		if (UVeyraStatusComponent* Statuses = FindBesideAbilitySystem<UVeyraStatusComponent>(*Caster))
		{
			Statuses->OnInterrupted.Remove(InterruptedHandle);
		}
		if (UVeyraCastStateComponent* CastState = FindBesideAbilitySystem<UVeyraCastStateComponent>(*Caster))
		{
			CastState->Clear();
		}
	}
	InterruptedHandle.Reset();
	EndAbility(Run.Handle, Run.ActorInfo, Run.ActivationInfo, /*bReplicateEndAbility*/ true, bCancelled);
}

bool UVeyraGameplayAbility::CheckCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayTagContainer* /*OptionalRelevantTags*/) const
{
	const FVeyraContentId Ability = GetContentId(Handle, ActorInfo);
	const UAbilitySystemComponent* Caster = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (Caster && EndsEarlyOnRecast(*Caster, Ability))
	{
		return true;
	}
	const UVeyraCooldownComponent* Cooldowns = FindBesideAbilitySystem<UVeyraCooldownComponent>(ActorInfo);
	return Cooldowns && Caster && Cooldowns->GetRemainingSecondsNow(CooldownIdOf(*Caster, Ability)) <= 0.0;
}

void UVeyraGameplayAbility::ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo /*ActivationInfo*/) const
{
	UVeyraCooldownComponent* Cooldowns = FindBesideAbilitySystem<UVeyraCooldownComponent>(ActorInfo);
	const UAbilitySystemComponent* Caster = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const FVeyraContentId Ability = GetContentId(Handle, ActorInfo);
	if (Cooldowns && Caster && Ability.IsValid())
	{
		Cooldowns->StartCooldown(CooldownIdOf(*Caster, Ability), GetCooldownSeconds(Ability, GetCommitRank(*Caster, Ability)), HasteOf(*Caster, Ability));
	}
}

void UVeyraGameplayAbility::GetCooldownTimeRemainingAndDuration(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	float& TimeRemaining, float& CooldownDuration) const
{
	const UVeyraCooldownComponent* Cooldowns = FindBesideAbilitySystem<UVeyraCooldownComponent>(ActorInfo);
	const UAbilitySystemComponent* Caster = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const FVeyraContentId Ability = Caster ? CooldownIdOf(*Caster, GetContentId(Handle, ActorInfo)) : GetContentId(Handle, ActorInfo);
	TimeRemaining = Cooldowns ? static_cast<float>(Cooldowns->GetRemainingSecondsNow(Ability)) : 0.0f;
	CooldownDuration = Cooldowns ? static_cast<float>(Cooldowns->GetDurationSeconds(Ability)) : 0.0f;
}

bool UVeyraGameplayAbility::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayTagContainer* /*OptionalRelevantTags*/) const
{
	const UAbilitySystemComponent* AbilitySystem = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const FVeyraContentId Ability = GetContentId(Handle, ActorInfo);
	if (!AbilitySystem)
	{
		return false;
	}
	return EndsEarlyOnRecast(*AbilitySystem, Ability) || VeyraCombat::CanAffordResource(*AbilitySystem, GetResourceCost(Ability, GetCommitRank(*AbilitySystem, Ability)));
}

void UVeyraGameplayAbility::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo /*ActivationInfo*/) const
{
	UAbilitySystemComponent* AbilitySystem = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const FVeyraContentId Ability = GetContentId(Handle, ActorInfo);
	if (AbilitySystem && !VeyraCombat::SpendResource(*AbilitySystem, GetResourceCost(Ability, GetCommitRank(*AbilitySystem, Ability))))
	{
		// CommitAbility checked the cost a moment ago, so this means the rules changed underneath it.
		UE_LOG(LogVeyraAbilities, Error, TEXT("%s committed but could not pay its cost."), *GetNameSafe(this));
	}
}
