// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Delegates/IDelegateInstance.h"
#include "UObject/ObjectKey.h"
#include "UObject/WeakObjectPtr.h"

class AVeyraEcho;
class AVeyraPlayerState;
class AVeyraVanguardController;
class UAbilitySystemComponent;
class UVeyraEchoSubsystem;
class UWorld;
enum class EVeyraEchoEnd : uint8;

/**
 * Match's side of a projected Echo (ADR-050 §6; Combat Bible §10's controllable proxy). When control passes to a
 * participant's Echo:
 * - a controller of its own possesses the Echo;
 * - the participant's own Vanguard controller lets its orders go, so none resumes when the Stasis ends;
 * - the participant's PlayerController learns its commanded unit, which its camera follows.
 * When the Echo ends, all of it is undone. The game mode owns one and asks it which controller a participant's orders
 * reach. Server only.
 */
class FVeyraEchoLink
{
public:
	~FVeyraEchoLink();

	/** Listens to World's Echoes. Does nothing where there are none. */
	void Start(UWorld& World);

	/** Stops listening, and hands every participant its Vanguard back. */
	void Stop();

	/** The controller Participant's move and attack orders reach: its Echo's while it commands one, else its Vanguard's. */
	AVeyraVanguardController* CommandedControllerOf(const AVeyraPlayerState* Participant) const;

	/** Whether Participant commands its Echo now. */
	bool IsCommanding(const AVeyraPlayerState* Participant) const;

private:
	void OnEchoCommanded(UAbilitySystemComponent& Holder, AVeyraEcho& Echo);
	void OnEchoEnded(AVeyraEcho& Echo, EVeyraEchoEnd Why);

	/** Participant's Echo controller lets its Echo go, and its player commands its Vanguard again. */
	void Release(AVeyraPlayerState& Participant);

	TWeakObjectPtr<UVeyraEchoSubsystem> Echoes;
	TMap<TObjectKey<AVeyraPlayerState>, TWeakObjectPtr<AVeyraVanguardController>> Controllers;
	FDelegateHandle CommandedHandle;
	FDelegateHandle EndedHandle;
};
