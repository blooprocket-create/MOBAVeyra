// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Cooldowns/VeyraCooldownComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Algo/Count.h"
#include "Attributes/VeyraOffenceSet.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "Stats/VeyraHaste.h"

namespace VeyraCooldowns
{
double RemainingSeconds(TConstArrayView<FVeyraCooldownEntry> Entries, const FVeyraContentId& Ability, double Now)
{
	const FVeyraCooldownEntry* Entry = Entries.FindByPredicate([&Ability](const FVeyraCooldownEntry& Candidate) { return Candidate.Ability == Ability; });
	return Entry ? FMath::Max(0.0, Entry->ReadyAt - Now) : 0.0;
}

void Start(TArray<FVeyraCooldownEntry>& Entries, const FVeyraContentId& Ability, double DurationSeconds, double Now, bool bAbilityHaste)
{
	FVeyraCooldownEntry* Entry = Entries.FindByPredicate([&Ability](const FVeyraCooldownEntry& Candidate) { return Candidate.Ability == Ability; });
	if (!Entry)
	{
		Entry = &Entries.AddDefaulted_GetRef();
		Entry->Ability = Ability;
	}
	Entry->ReadyAt = Now + DurationSeconds;
	Entry->DurationSeconds = DurationSeconds;
	Entry->bAbilityHaste = bAbilityHaste;
}

bool Clear(TArray<FVeyraCooldownEntry>& Entries, const FVeyraContentId& Ability)
{
	return Entries.RemoveAll([&Ability](const FVeyraCooldownEntry& Entry) { return Entry.Ability == Ability; }) > 0;
}

bool Reduce(TArray<FVeyraCooldownEntry>& Entries, const FVeyraContentId& Ability, double Now, double Fraction, double Seconds)
{
	FVeyraCooldownEntry* Entry = Entries.FindByPredicate([&Ability](const FVeyraCooldownEntry& Candidate) { return Candidate.Ability == Ability; });
	if (!Entry || Entry->ReadyAt <= Now)
	{
		return false;
	}
	const double Remaining = (Entry->ReadyAt - Now) * (1.0 - FMath::Clamp(Fraction, 0.0, 1.0)) - FMath::Max(0.0, Seconds);
	Entry->ReadyAt = Now + FMath::Max(0.0, Remaining);
	return true;
}

void Rescale(TArray<FVeyraCooldownEntry>& Entries, double Factor, double Now)
{
	for (FVeyraCooldownEntry& Entry : Entries)
	{
		if (Entry.bAbilityHaste && Entry.ReadyAt > Now)
		{
			Entry.ReadyAt = Now + (Entry.ReadyAt - Now) * Factor;
			Entry.DurationSeconds *= Factor;
		}
	}
}
}

UVeyraCooldownComponent::UVeyraCooldownComponent()
{
	SetIsReplicatedByDefault(true);
}

void UVeyraCooldownComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	Params.Condition = COND_ReplayOrOwner;
	DOREPLIFETIME_WITH_PARAMS_FAST(UVeyraCooldownComponent, Entries, Params);
}

void UVeyraCooldownComponent::OnRegister()
{
	Super::OnRegister();
	if (UAbilitySystemComponent* AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()))
	{
		HasteHandle = AbilitySystem->GetGameplayAttributeValueChangeDelegate(UVeyraOffenceSet::GetAbilityHasteAttribute())
			.AddUObject(this, &UVeyraCooldownComponent::OnAbilityHasteChanged);
	}
}

void UVeyraCooldownComponent::OnUnregister()
{
	if (UAbilitySystemComponent* AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()))
	{
		AbilitySystem->GetGameplayAttributeValueChangeDelegate(UVeyraOffenceSet::GetAbilityHasteAttribute()).Remove(HasteHandle);
	}
	HasteHandle.Reset();
	Super::OnUnregister();
}

void UVeyraCooldownComponent::StartCooldown(const FVeyraContentId& Ability, double BaseSeconds, EVeyraCooldownHaste Haste)
{
	check(GetOwner() && GetOwner()->HasAuthority());
	const bool bAbilityHaste = Haste == EVeyraCooldownHaste::Ability;
	const UAbilitySystemComponent* AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
	const double AbilityHaste = bAbilityHaste && AbilitySystem && AbilitySystem->HasAttributeSetForAttribute(UVeyraOffenceSet::GetAbilityHasteAttribute())
		? AbilitySystem->GetNumericAttribute(UVeyraOffenceSet::GetAbilityHasteAttribute())
		: 0.0;
	VeyraCooldowns::Start(Entries, Ability, BaseSeconds * VeyraHaste::CooldownMultiplier(AbilityHaste), GetServerNow(), bAbilityHaste);
	MARK_PROPERTY_DIRTY_FROM_NAME(UVeyraCooldownComponent, Entries, this);
}

void UVeyraCooldownComponent::ClearCooldown(const FVeyraContentId& Ability)
{
	check(GetOwner() && GetOwner()->HasAuthority());
	if (VeyraCooldowns::Clear(Entries, Ability))
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(UVeyraCooldownComponent, Entries, this);
	}
}

void UVeyraCooldownComponent::ReduceCooldown(const FVeyraContentId& Ability, double Fraction, double Seconds)
{
	check(GetOwner() && GetOwner()->HasAuthority());
	if (VeyraCooldowns::Reduce(Entries, Ability, GetServerNow(), Fraction, Seconds))
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(UVeyraCooldownComponent, Entries, this);
	}
}

int32 UVeyraCooldownComponent::ClearAllCooldowns()
{
	check(GetOwner() && GetOwner()->HasAuthority());
	const double Now = GetServerNow();
	const int32 Running = Algo::CountIf(Entries, [Now](const FVeyraCooldownEntry& Entry) { return Entry.ReadyAt > Now; });
	if (!Entries.IsEmpty())
	{
		Entries.Reset();
		MARK_PROPERTY_DIRTY_FROM_NAME(UVeyraCooldownComponent, Entries, this);
	}
	return Running;
}

void UVeyraCooldownComponent::OnAbilityHasteChanged(const FOnAttributeChangeData& Change)
{
	const AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || Entries.IsEmpty())
	{
		return;
	}
	// What remains keeps its proportion: it becomes the new multiplier's share of the old one's (§21).
	const double Factor = VeyraHaste::CooldownMultiplier(Change.NewValue) / VeyraHaste::CooldownMultiplier(Change.OldValue);
	VeyraCooldowns::Rescale(Entries, Factor, GetServerNow());
	MARK_PROPERTY_DIRTY_FROM_NAME(UVeyraCooldownComponent, Entries, this);
}

double UVeyraCooldownComponent::GetRemainingSeconds(const FVeyraContentId& Ability, double Now) const
{
	return VeyraCooldowns::RemainingSeconds(Entries, Ability, Now);
}

double UVeyraCooldownComponent::GetRemainingSecondsNow(const FVeyraContentId& Ability) const
{
	return GetRemainingSeconds(Ability, GetServerNow());
}

double UVeyraCooldownComponent::GetServerNow() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World->GetGameState();
	return GameState ? GameState->GetServerWorldTimeSeconds() : World->GetTimeSeconds();
}

double UVeyraCooldownComponent::GetDurationSeconds(const FVeyraContentId& Ability) const
{
	const FVeyraCooldownEntry* Entry = Entries.FindByPredicate([&Ability](const FVeyraCooldownEntry& Candidate) { return Candidate.Ability == Ability; });
	return Entry ? Entry->DurationSeconds : 0.0;
}

double UVeyraCooldownComponent::GetReadyAt(const FVeyraContentId& Ability) const
{
	const FVeyraCooldownEntry* Entry = Entries.FindByPredicate([&Ability](const FVeyraCooldownEntry& Candidate) { return Candidate.Ability == Ability; });
	return Entry ? Entry->ReadyAt : 0.0;
}
