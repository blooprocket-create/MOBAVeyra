// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Echoes/VeyraEchoLink.h"

#include "AbilitySystemComponent.h"
#include "Echoes/VeyraEcho.h"
#include "Echoes/VeyraEchoSubsystem.h"
#include "Engine/World.h"
#include "VeyraMatchLog.h"
#include "VeyraPlayerController.h"
#include "VeyraPlayerState.h"
#include "VeyraVanguardController.h"

FVeyraEchoLink::~FVeyraEchoLink()
{
	Stop();
}

void FVeyraEchoLink::Start(UWorld& World)
{
	UVeyraEchoSubsystem* Subsystem = World.GetSubsystem<UVeyraEchoSubsystem>();
	if (!Subsystem)
	{
		return;
	}
	Echoes = Subsystem;
	CommandedHandle = Subsystem->OnEchoCommanded.AddRaw(this, &FVeyraEchoLink::OnEchoCommanded);
	EndedHandle = Subsystem->OnEchoEnded.AddRaw(this, &FVeyraEchoLink::OnEchoEnded);
}

void FVeyraEchoLink::Stop()
{
	if (UVeyraEchoSubsystem* Subsystem = Echoes.Get())
	{
		Subsystem->OnEchoCommanded.Remove(CommandedHandle);
		Subsystem->OnEchoEnded.Remove(EndedHandle);
	}
	Echoes.Reset();
	TArray<TObjectKey<AVeyraPlayerState>> Commanding;
	Controllers.GetKeys(Commanding);
	for (const TObjectKey<AVeyraPlayerState>& Key : Commanding)
	{
		if (AVeyraPlayerState* Participant = Key.ResolveObjectPtr())
		{
			Release(*Participant);
		}
	}
	Controllers.Reset();
}

AVeyraVanguardController* FVeyraEchoLink::CommandedControllerOf(const AVeyraPlayerState* Participant) const
{
	if (!Participant)
	{
		return nullptr;
	}
	const TWeakObjectPtr<AVeyraVanguardController>* Echo = Controllers.Find(Participant);
	return Echo && Echo->IsValid() ? Echo->Get() : Participant->GetVanguardController();
}

bool FVeyraEchoLink::IsCommanding(const AVeyraPlayerState* Participant) const
{
	const TWeakObjectPtr<AVeyraVanguardController>* Echo = Participant ? Controllers.Find(Participant) : nullptr;
	return Echo && Echo->IsValid();
}

void FVeyraEchoLink::OnEchoCommanded(UAbilitySystemComponent& Holder, AVeyraEcho& Echo)
{
	AVeyraPlayerState* Participant = Cast<AVeyraPlayerState>(Holder.GetOwner());
	UWorld* World = Echo.GetWorld();
	if (!Participant || !World)
	{
		return;
	}
	// The Vanguard waits in Stasis: whatever it was told to do ends now, not when it wakes.
	if (AVeyraVanguardController* Own = Participant->GetVanguardController())
	{
		Own->StopOrders();
	}
	FActorSpawnParameters Parameters;
	Parameters.ObjectFlags |= RF_Transient;
	AVeyraVanguardController* Controller = World->SpawnActor<AVeyraVanguardController>(Parameters);
	if (!Controller)
	{
		return;
	}
	Controller->Possess(&Echo);
	Controllers.Add(Participant, Controller);
	if (AVeyraPlayerController* Player = Cast<AVeyraPlayerController>(Participant->GetOwner()))
	{
		Player->SetCommandedUnit(&Echo);
	}
	UE_LOG(LogVeyraMatch, Log, TEXT("%s commands its Echo %s."), *Participant->GetPlayerName(), *Echo.GetName());
}

void FVeyraEchoLink::OnEchoEnded(AVeyraEcho& Echo, EVeyraEchoEnd /*Why*/)
{
	if (AVeyraPlayerState* Participant = Cast<AVeyraPlayerState>(Echo.GetHolderState()))
	{
		Release(*Participant);
	}
}

void FVeyraEchoLink::Release(AVeyraPlayerState& Participant)
{
	TWeakObjectPtr<AVeyraVanguardController> Echo;
	if (!Controllers.RemoveAndCopyValue(&Participant, Echo))
	{
		return;
	}
	if (AVeyraVanguardController* Controller = Echo.Get())
	{
		Controller->UnPossess();
		Controller->Destroy();
	}
	if (AVeyraPlayerController* Player = Cast<AVeyraPlayerController>(Participant.GetOwner()))
	{
		Player->SetCommandedUnit(nullptr);
	}
	UE_LOG(LogVeyraMatch, Log, TEXT("%s commands its Vanguard again."), *Participant.GetPlayerName());
}
