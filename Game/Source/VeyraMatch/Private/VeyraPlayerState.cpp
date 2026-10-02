// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "VeyraPlayerState.h"

#include "AbilitySystemComponent.h"
#include "Absorption/VeyraDamageAbsorptionComponent.h"
#include "Attacks/VeyraBasicAttackComponent.h"
#include "Attribution/VeyraAttributionComponent.h"
#include "CombatState/VeyraCombatStateComponent.h"
#include "Attributes/VeyraAbilitySystemComponent.h"
#include "Attributes/VeyraDefenceSet.h"
#include "Attributes/VeyraMobilitySet.h"
#include "Attributes/VeyraOffenceSet.h"
#include "Attributes/VeyraResourceSet.h"
#include "Attributes/VeyraVitalsSet.h"
#include "Buyback/VeyraBuybackComponent.h"
#include "Casting/VeyraCastStateComponent.h"
#include "Cooldowns/VeyraCooldownComponent.h"
#include "Loadout/VeyraAbilityLoadoutComponent.h"
#include "Life/VeyraLifeComponent.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "Gold/VeyraGoldComponent.h"
#include "Inventory/VeyraInventoryComponent.h"
#include "Progression/VeyraProgressionComponent.h"
#include "Regeneration/VeyraRegenerationComponent.h"
#include "Passives/VeyraPassive.h"
#include "Recall/VeyraRecallComponent.h"
#include "Statistics/VeyraScoreComponent.h"
#include "Statuses/VeyraStatusComponent.h"
#include "Targeting/VeyraParticipantData.h"
#include "Tools/VeyraVisionToolComponent.h"
#include "VeyraCombatVerbs.h"
#include "VeyraMatchLog.h"
#include "VeyraVanguardCharacter.h"

AVeyraPlayerState::AVeyraPlayerState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Its attribute sets replicate behind the fog (ADR-016 §3).
	AbilitySystem = CreateDefaultSubobject<UVeyraAbilitySystemComponent>(TEXT("AbilitySystem"));
	AbilitySystem->SetIsReplicated(true);
	AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	DamageAbsorption = CreateDefaultSubobject<UVeyraDamageAbsorptionComponent>(TEXT("DamageAbsorption"));
	Statuses = CreateDefaultSubobject<UVeyraStatusComponent>(TEXT("Statuses"));
	CombatState = CreateDefaultSubobject<UVeyraCombatStateComponent>(TEXT("CombatState"));
	Attribution = CreateDefaultSubobject<UVeyraAttributionComponent>(TEXT("Attribution"));
	Life = CreateDefaultSubobject<UVeyraLifeComponent>(TEXT("Life"));
	Loadout = CreateDefaultSubobject<UVeyraAbilityLoadoutComponent>(TEXT("Loadout"));
	Cooldowns = CreateDefaultSubobject<UVeyraCooldownComponent>(TEXT("Cooldowns"));
	CastState = CreateDefaultSubobject<UVeyraCastStateComponent>(TEXT("CastState"));
	BasicAttack = CreateDefaultSubobject<UVeyraBasicAttackComponent>(TEXT("BasicAttack"));
	Regeneration = CreateDefaultSubobject<UVeyraRegenerationComponent>(TEXT("Regeneration"));
	Progression = CreateDefaultSubobject<UVeyraProgressionComponent>(TEXT("Progression"));
	Gold = CreateDefaultSubobject<UVeyraGoldComponent>(TEXT("Gold"));
	Buyback = CreateDefaultSubobject<UVeyraBuybackComponent>(TEXT("Buyback"));
	Inventory = CreateDefaultSubobject<UVeyraInventoryComponent>(TEXT("Inventory"));
	Recall = CreateDefaultSubobject<UVeyraRecallComponent>(TEXT("Recall"));
	VisionTool = CreateDefaultSubobject<UVeyraVisionToolComponent>(TEXT("VisionTool"));
	PublicScore = CreateDefaultSubobject<UVeyraScoreComponent>(TEXT("PublicScore"));

	// Attribute Sets created as default subobjects of the owner register with its Ability System
	// Component when the component initializes.
	VitalsSet = CreateDefaultSubobject<UVeyraVitalsSet>(TEXT("VitalsSet"));
	OffenceSet = CreateDefaultSubobject<UVeyraOffenceSet>(TEXT("OffenceSet"));
	DefenceSet = CreateDefaultSubobject<UVeyraDefenceSet>(TEXT("DefenceSet"));
	MobilitySet = CreateDefaultSubobject<UVeyraMobilitySet>(TEXT("MobilitySet"));
	ResourceSet = CreateDefaultSubobject<UVeyraResourceSet>(TEXT("ResourceSet"));
}

UAbilitySystemComponent* AVeyraPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}

void AVeyraPlayerState::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// Until a Vanguard exists the PlayerState is its own avatar; the Vanguard takes over when it is
	// given this PlayerState (AVeyraVanguardCharacter::OnPlayerStateChanged).
	AbilitySystem->InitAbilityActorInfo(this, GetPawn() ? static_cast<AActor*>(GetPawn()) : this);
	VeyraCombat::ConfigureCombatant(*AbilitySystem, *DamageAbsorption, *Statuses);
}

void AVeyraPlayerState::OnReplicationStartedForIris(const FOnReplicationStartedParams& Params)
{
	Super::OnReplicationStartedForIris(Params);
	VeyraParticipantData::ApplyGates(*this);
}

void AVeyraPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraPlayerState, Team, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraPlayerState, VanguardId, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraPlayerState, RespawnAt, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraPlayerState, MasteryLevel, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraPlayerState, EmoteTier, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraPlayerState, MasteryEmoteUntil, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraPlayerState, bAway, Params);
}

void AVeyraPlayerState::SetAway(bool bNewAway)
{
	SetIsInactive(bNewAway);
	if (bAway != bNewAway)
	{
		bAway = bNewAway;
		MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraPlayerState, bAway, this);
	}
	// A player state replicates rarely; the others should learn at once.
	ForceNetUpdate();
}

void AVeyraPlayerState::OnRep_Away()
{
	// As the engine does for its own flag: in or out of this client's list of players.
	SetIsInactive(bAway);
	OnRep_bIsInactive();
}

void AVeyraPlayerState::SetVeyraTeam(EVeyraTeam NewTeam)
{
	Team = NewTeam;
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraPlayerState, Team, this);
}

void AVeyraPlayerState::SetVanguardId(const FVeyraContentId& InVanguardId)
{
	VanguardId = InVanguardId;
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraPlayerState, VanguardId, this);
}

void AVeyraPlayerState::SetRespawnAt(double InRespawnAt)
{
	RespawnAt = InRespawnAt;
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraPlayerState, RespawnAt, this);
}

void AVeyraPlayerState::SetMastery(int32 InLevel, int32 InEmoteTier)
{
	MasteryLevel = FMath::Max(InLevel, 0);
	EmoteTier = FMath::Max(InEmoteTier, 0);
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraPlayerState, MasteryLevel, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraPlayerState, EmoteTier, this);
}

void AVeyraPlayerState::ShowMasteryEmote(double Until)
{
	MasteryEmoteUntil = Until;
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraPlayerState, MasteryEmoteUntil, this);
}

void AVeyraPlayerState::OnRep_VanguardId()
{
	// The body may have arrived first, before it knew which Vanguard it is.
	if (AVeyraVanguardCharacter* Body = GetPawn<AVeyraVanguardCharacter>())
	{
		Body->ApplyVanguardBody();
	}
}

void AVeyraPlayerState::OnDeactivated()
{
	SetAway(true);
	UE_LOG(LogVeyraMatch, Log, TEXT("%s disconnected; the Vanguard stays in the match."), *GetPlayerName());
}
