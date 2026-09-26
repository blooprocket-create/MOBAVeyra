// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Cooldowns/VeyraCooldownComponent.h"

#include "Engine/World.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"

namespace VeyraCooldowns
{
double RemainingSeconds(TConstArrayView<FVeyraCooldownEntry> Entries, const FVeyraContentId& Ability, double Now)
{
	const FVeyraCooldownEntry* Entry = Entries.FindByPredicate([&Ability](const FVeyraCooldownEntry& Candidate) { return Candidate.Ability == Ability; });
	return Entry ? FMath::Max(0.0, Entry->ReadyAt - Now) : 0.0;
}

void Start(TArray<FVeyraCooldownEntry>& Entries, const FVeyraContentId& Ability, double DurationSeconds, double Now)
{
	FVeyraCooldownEntry* Entry = Entries.FindByPredicate([&Ability](const FVeyraCooldownEntry& Candidate) { return Candidate.Ability == Ability; });
	if (!Entry)
	{
		Entry = &Entries.AddDefaulted_GetRef();
		Entry->Ability = Ability;
	}
	Entry->ReadyAt = Now + DurationSeconds;
	Entry->DurationSeconds = DurationSeconds;
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

void UVeyraCooldownComponent::StartCooldown(const FVeyraContentId& Ability, double DurationSeconds)
{
	check(GetOwner() && GetOwner()->HasAuthority());
	VeyraCooldowns::Start(Entries, Ability, DurationSeconds, GetWorld()->GetTimeSeconds());
	MARK_PROPERTY_DIRTY_FROM_NAME(UVeyraCooldownComponent, Entries, this);
}

double UVeyraCooldownComponent::GetRemainingSeconds(const FVeyraContentId& Ability, double Now) const
{
	return VeyraCooldowns::RemainingSeconds(Entries, Ability, Now);
}

double UVeyraCooldownComponent::GetRemainingSecondsNow(const FVeyraContentId& Ability) const
{
	return GetRemainingSeconds(Ability, GetWorld()->GetTimeSeconds());
}

double UVeyraCooldownComponent::GetDurationSeconds(const FVeyraContentId& Ability) const
{
	const FVeyraCooldownEntry* Entry = Entries.FindByPredicate([&Ability](const FVeyraCooldownEntry& Candidate) { return Candidate.Ability == Ability; });
	return Entry ? Entry->DurationSeconds : 0.0;
}
