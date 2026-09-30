// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Entities/VeyraPlacedMarker.h"

#include "AbilitySystemComponent.h"
#include "Absorption/VeyraDamageAbsorptionComponent.h"
#include "Attributes/VeyraDefenceSet.h"
#include "Attributes/VeyraVitalsSet.h"
#include "Attribution/VeyraAttributionComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Life/VeyraLifeComponent.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "Statuses/VeyraStatusComponent.h"
#include "VeyraCombatLog.h"
#include "VeyraCombatVerbs.h"

AVeyraPlacedMarker::AVeyraPlacedMarker(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	// Always relevant, so replays record it; Vision's fog gate decides which clients receive it (ADR-016 §3).
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);
	AutoPossessAI = EAutoPossessAI::Disabled;
	AIControllerClass = nullptr;

	// It stops no one, as a ward does: its body is on the channel every other body ignores, and it
	// still blocks the cursor's trace, so a player who sees it can click it.
	Body = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Body"));
	Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Body->SetCollisionObjectType(ECC_GameTraceChannel1);
	Body->SetCollisionResponseToAllChannels(ECR_Ignore);
	Body->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	Body->SetCanEverAffectNavigation(false);
	Body->SetGenerateOverlapEvents(false);
	RootComponent = Body;

	AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystem"));
	AbilitySystem->SetIsReplicated(true);
	AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
	DamageAbsorption = CreateDefaultSubobject<UVeyraDamageAbsorptionComponent>(TEXT("DamageAbsorption"));
	Statuses = CreateDefaultSubobject<UVeyraStatusComponent>(TEXT("Statuses"));
	Life = CreateDefaultSubobject<UVeyraLifeComponent>(TEXT("Life"));
	Attribution = CreateDefaultSubobject<UVeyraAttributionComponent>(TEXT("Attribution"));
	VitalsSet = CreateDefaultSubobject<UVeyraVitalsSet>(TEXT("VitalsSet"));
	DefenceSet = CreateDefaultSubobject<UVeyraDefenceSet>(TEXT("DefenceSet"));
}

AVeyraPlacedMarker* AVeyraPlacedMarker::Place(UWorld& World, UAbilitySystemComponent& Owner, const FVeyraMarkerSpec& InSpec, const FTransform& Where)
{
	const AActor* Participant = Owner.GetOwner();
	if (!Participant || !Participant->HasAuthority() || !(InSpec.LifetimeSeconds > 0.0) || InSpec.HitsToDestroy < 0)
	{
		UE_LOG(LogVeyraCombat, Error, TEXT("Refused a marker %s: only the server places one, for a participant, with a lifetime above 0."), *InSpec.Id.ToString());
		return nullptr;
	}
	AVeyraPlacedMarker* Marker = World.SpawnActorDeferred<AVeyraPlacedMarker>(AVeyraPlacedMarker::StaticClass(), Where, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Marker)
	{
		return nullptr;
	}
	// Its side before it joins the world, so the fog gate knows it from the start (ADR-016 §3).
	Marker->Team = VeyraTeams::TeamOf(Participant);
	Marker->OwnerAbilities = &Owner;
	Marker->Spec = InSpec;
	Marker->bTargetable = InSpec.HitsToDestroy > 0;
	APlayerState* OwnerState = const_cast<APlayerState*>(Cast<APlayerState>(Participant));
	Marker->PresentedAs = InSpec.bPresentsAsOwner ? OwnerState : nullptr;
	// Its owner's size, when it stands in for its owner; a ward's-size post otherwise.
	const APawn* OwnerBody = Cast<APawn>(Owner.GetAvatarActor());
	float Radius = 0.0f;
	float HalfHeight = 0.0f;
	if (OwnerBody)
	{
		OwnerBody->GetSimpleCollisionCylinder(Radius, HalfHeight);
	}
	Marker->BodySize = FVector2f(Radius, HalfHeight);
	Marker->FinishSpawning(Where);
	Marker->Start();
	return Marker;
}

void AVeyraPlacedMarker::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	AbilitySystem->InitAbilityActorInfo(this, this);
	VeyraCombat::ConfigureCombatant(*AbilitySystem, *DamageAbsorption, *Statuses);
}

void AVeyraPlacedMarker::BeginPlay()
{
	Super::BeginPlay();
	ApplyBody();
}

void AVeyraPlacedMarker::ApplyBody()
{
	if (BodySize.X > 0.0f && BodySize.Y > 0.0f)
	{
		Body->SetCapsuleSize(BodySize.X, BodySize.Y);
	}
}

void AVeyraPlacedMarker::Start()
{
	ApplyBody();
	// A point of Health a hit, as a ward counts them (ADR-016 §6); one no one can target has none.
	if (bTargetable)
	{
		VeyraCombat::InitializeVitals(*AbilitySystem, static_cast<double>(Spec.HitsToDestroy));
	}
	// World time, so a pause holds it.
	GetWorldTimerManager().SetTimer(LifetimeTimer, FTimerDelegate::CreateWeakLambda(this, [this] { EndMarker(EVeyraMarkerEndReason::Expired); }),
		static_cast<float>(Spec.LifetimeSeconds), /*bLoop*/ false);
	if (UVeyraCombatEventSubsystem* Events = GetWorld()->GetSubsystem<UVeyraCombatEventSubsystem>())
	{
		DeathHandle = Events->OnDeath.AddUObject(this, &AVeyraPlacedMarker::OnDeath);
	}
}

void AVeyraPlacedMarker::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UVeyraCombatEventSubsystem* Events = GetWorld() ? GetWorld()->GetSubsystem<UVeyraCombatEventSubsystem>() : nullptr)
	{
		Events->OnDeath.Remove(DeathHandle);
	}
	GetWorldTimerManager().ClearTimer(LifetimeTimer);
	Super::EndPlay(EndPlayReason);
}

void AVeyraPlacedMarker::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraPlacedMarker, Team, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraPlacedMarker, bTargetable, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraPlacedMarker, PresentedAs, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraPlacedMarker, BodySize, Params);
}

void AVeyraPlacedMarker::OnDeath(const FVeyraDeathEvent& Death)
{
	const UAbilitySystemComponent* Victim = Death.Victim.Get();
	if (Victim == AbilitySystem)
	{
		UAbilitySystemComponent* Destroyer = Death.CreditedKiller.IsValid() ? Death.CreditedKiller.Get() : Death.Killer.Get();
		EndMarker(EVeyraMarkerEndReason::Destroyed, Destroyer);
	}
	else if (Victim && Victim == OwnerAbilities.Get())
	{
		EndMarker(EVeyraMarkerEndReason::OwnerDied);
	}
}

void AVeyraPlacedMarker::EndMarker(EVeyraMarkerEndReason Reason, UAbilitySystemComponent* Destroyer)
{
	if (!HasAuthority() || bEnded)
	{
		return;
	}
	bEnded = true;
	GetWorldTimerManager().ClearTimer(LifetimeTimer);
	if (UVeyraCombatEventSubsystem* Events = GetWorld() ? GetWorld()->GetSubsystem<UVeyraCombatEventSubsystem>() : nullptr)
	{
		Events->OnMarkerEnded.Broadcast(FVeyraMarkerEnd{ this, OwnerAbilities, Spec.Id, Reason, GetActorLocation(), Destroyer });
	}
	Destroy();
}
