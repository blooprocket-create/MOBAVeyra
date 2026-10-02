// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Brain/VeyraBotBrainComponent.h"

#include "Brain/VeyraBotRules.h"
#include "Brain/VeyraBotSenses.h"
#include "Engine/World.h"
#include "Gold/VeyraGoldComponent.h"
#include "Inventory/VeyraInventoryComponent.h"
#include "Progression/VeyraProgressionComponent.h"
#include "Progression/VeyraProgressionRules.h"
#include "Progression/VeyraProgressionTuningSubsystem.h"
#include "Recall/VeyraRecallComponent.h"
#include "Shop/VeyraShopSubsystem.h"
#include "TimerManager.h"
#include "Tuning/VeyraBotsTuningSubsystem.h"
#include "Tuning/VeyraItemsTuningSubsystem.h"
#include "VeyraBotsLog.h"
#include "VeyraGameMode.h"
#include "VeyraPlayerState.h"
#include "VeyraVanguardController.h"

UVeyraBotBrainComponent::UVeyraBotBrainComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UVeyraBotBrainComponent::Configure(AVeyraPlayerState& InBot, EVeyraBotRole InRole, EVeyraBotDifficulty InDifficulty, bool bInWards, int32 Seed)
{
	bWards = bInWards;
	Bot = &InBot;
	Role = InRole;
	Difficulty = InDifficulty;
	Random.Initialize(Seed);
}

void UVeyraBotBrainComponent::BeginPlay()
{
	Super::BeginPlay();
	if (UWorld* World = GetWorld(); World && GetOwner() && GetOwner()->HasAuthority())
	{
		const double Seconds = UVeyraBotsTuningSubsystem::GetDifficulty(Difficulty).ThinkSeconds;
		World->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateWeakLambda(this, [this] { Think(); }), static_cast<float>(Seconds), /*bLoop*/ true);
	}
}

void UVeyraBotBrainComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(Timer);
	}
	Super::EndPlay(EndPlayReason);
}

FVeyraBotIntent UVeyraBotBrainComponent::Think()
{
	AVeyraPlayerState* Participant = Bot.Get();
	AVeyraGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AVeyraGameMode>() : nullptr;
	if (!Participant || !GameMode)
	{
		return {};
	}
	const FVeyraBotsTuning& Tuning = UVeyraBotsTuningSubsystem::Get();
	const FVeyraBotView View = VeyraBotSenses::Sense(*Participant, Role, bWards, Tuning);
	// Shopping and skill points work in preparation too, and while dead, as a player's do.
	Shop(View, *GameMode);
	RankUp(*GameMode);
	if (GameMode->CheckOrdersAllowed() != EVeyraOrderRejection::None)
	{
		return {};
	}
	Drink(View);
	const FVeyraBotIntent Intent = VeyraBotRules::Decide(View, UVeyraBotsTuningSubsystem::GetDifficulty(Difficulty), Tuning, Memory, Random);
	Act(Intent, *GameMode);
	LastIntent = Intent;
	return Intent;
}

void UVeyraBotBrainComponent::Shop(const FVeyraBotView& View, AVeyraGameMode& GameMode)
{
	AVeyraPlayerState* Participant = Bot.Get();
	const FVeyraBotVanguardTuning* Behaviour = Participant ? UVeyraBotsTuningSubsystem::FindVanguard(Participant->GetVanguardId()) : nullptr;
	const UVeyraInventoryComponent* Inventory = Participant ? Participant->FindComponentByClass<UVeyraInventoryComponent>() : nullptr;
	const UVeyraGoldComponent* Gold = Participant ? Participant->FindComponentByClass<UVeyraGoldComponent>() : nullptr;
	UVeyraShopSubsystem* ShopSubsystem = GetWorld()->GetSubsystem<UVeyraShopSubsystem>();
	// A bot buys where purchases arrive at once: at its fountain, or dead (ADR-013 §8.3).
	if (!Behaviour || !Inventory || !Gold || !ShopSubsystem || (View.bAlive && !View.bAtFountain) || GameMode.CheckShopAllowed() != EVeyraOrderRejection::None)
	{
		return;
	}
	const FVeyraItemsTuning& Items = UVeyraItemsTuningSubsystem::Get();
	TOptional<FVeyraContentId> Next = VeyraBotRules::NextPurchase(Items, Behaviour->Build, Inventory->GetSlots(), Inventory->GetQueue(), Inventory->GetMythical(), Gold->GetGold());
	// With nothing toward its build to buy, it tops up the consumables it carries (ADR-056 §1).
	if (!Next.IsSet())
	{
		Next = VeyraBotRules::NextConsumable(Items, UVeyraBotsTuningSubsystem::Get().Consumables, UVeyraBotsTuningSubsystem::GetDifficulty(Difficulty).ConsumablesCarried,
			Inventory->GetSlots(), Inventory->GetQueue(), Inventory->GetMythical(), Gold->GetGold(), View.MatchSeconds);
	}
	if (Next.IsSet())
	{
		const EVeyraShopRefusal Refusal = ShopSubsystem->Buy(*Participant, Next.GetValue());
		UE_LOG(LogVeyraBots, Verbose, TEXT("%s buys %s: %s."), *Participant->GetPlayerName(), *Next->ToString(), LexToString(Refusal));
	}
}

void UVeyraBotBrainComponent::RankUp(AVeyraGameMode& GameMode)
{
	AVeyraPlayerState* Participant = Bot.Get();
	const FVeyraBotVanguardTuning* Behaviour = Participant ? UVeyraBotsTuningSubsystem::FindVanguard(Participant->GetVanguardId()) : nullptr;
	UVeyraProgressionComponent* Progression = Participant ? Participant->FindComponentByClass<UVeyraProgressionComponent>() : nullptr;
	if (!Behaviour || !Progression || !Progression->IsInitialized() || GameMode.CheckRankUpAllowed() != EVeyraOrderRejection::None)
	{
		return;
	}
	while (Progression->GetUnspentSkillPoints() > 0)
	{
		// Through the unit's own rank shape, which may rank Q, W and E further or learn R from the start (ADR-031 §2).
		const TOptional<EVeyraAbilitySlot> Slot = VeyraBotRules::NextRank(Behaviour->SkillPriority, [Progression](EVeyraAbilitySlot Candidate) {
			return Progression->CheckRankUp(Candidate) == EVeyraRankRefusal::None;
		});
		if (!Slot.IsSet() || Progression->AllocateRank(Slot.GetValue()) != EVeyraRankRefusal::None)
		{
			return;
		}
	}
}

void UVeyraBotBrainComponent::Drink(const FVeyraBotView& View)
{
	AVeyraPlayerState* Participant = Bot.Get();
	UVeyraShopSubsystem* ShopSubsystem = GetWorld()->GetSubsystem<UVeyraShopSubsystem>();
	const TOptional<int32> Slot = VeyraBotRules::NextDrink(View, UVeyraBotsTuningSubsystem::Get().Consumables);
	if (Participant && ShopSubsystem && Slot.IsSet())
	{
		// The rule a player's drink goes through, which refuses one while another still restores.
		const EVeyraShopRefusal Refusal = ShopSubsystem->UseConsumable(*Participant, Slot.GetValue());
		UE_LOG(LogVeyraBots, Verbose, TEXT("%s drinks: %s."), *Participant->GetPlayerName(), LexToString(Refusal));
	}
}

void UVeyraBotBrainComponent::Act(const FVeyraBotIntent& Intent, AVeyraGameMode& GameMode)
{
	AVeyraPlayerState* Participant = Bot.Get();
	const AVeyraVanguardController* Controller = Participant ? Participant->GetVanguardController() : nullptr;
	if (!Controller)
	{
		return;
	}
	const double Tolerance = UVeyraBotsTuningSubsystem::Get().Positioning.HoldTolerance;
	switch (Intent.Action)
	{
	case EVeyraBotAction::Wait:
		break;
	case EVeyraBotAction::Move:
	case EVeyraBotAction::Retreat:
	{
		// Walking there already, it lets the order stand, so its path is not thrown away.
		const TOptional<FVector>& Walking = Controller->GetMoveOrder();
		if (!Walking.IsSet() || FVector::Dist2D(Walking.GetValue(), Intent.Destination) > Tolerance || Controller->GetAttackTarget())
		{
			GameMode.HandleMoveOrder(Participant, Intent.Destination);
		}
		break;
	}
	case EVeyraBotAction::Recall:
		GameMode.HandleRecallOrder(Participant);
		break;
	case EVeyraBotAction::Attack:
		if (AActor* Target = Intent.Target.Get(); Target && Controller->GetAttackTarget() != Target)
		{
			GameMode.HandleAttackOrder(Participant, Target);
		}
		break;
	case EVeyraBotAction::Cast:
	{
		const EVeyraCastRejection Rejection = GameMode.HandleCastOrder(Participant, Intent.Slot, Intent.CastTarget);
		UE_LOG(LogVeyraBots, Verbose, TEXT("%s casts %s: %s."), *Participant->GetPlayerName(), *UEnum::GetValueAsString(Intent.Slot), LexToString(Rejection));
		// A refused cast, or one that empowers the next attack, is followed by the attack.
		AActor* Target = Intent.Target.Get();
		if (Target && Controller->GetAttackTarget() != Target)
		{
			GameMode.HandleAttackOrder(Participant, Target);
		}
		break;
	}
	case EVeyraBotAction::Ward:
	{
		const EVeyraOrderRejection Rejection = GameMode.HandleVisionToolOrder(Participant, Intent.Destination);
		UE_LOG(LogVeyraBots, Verbose, TEXT("%s wards: %s."), *Participant->GetPlayerName(), LexToString(Rejection));
		break;
	}
	}
	if (Intent.Action != LastIntent.Action || Intent.Target != LastIntent.Target)
	{
		UE_LOG(LogVeyraBots, Verbose, TEXT("%s: %s."), *Participant->GetPlayerName(), Intent.Reason);
	}
}
