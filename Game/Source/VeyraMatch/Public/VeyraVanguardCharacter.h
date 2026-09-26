// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "AbilitySystemInterface.h"
#include "Delegates/IDelegateInstance.h"
#include "GameFramework/Character.h"
#include "Teams/VeyraTeam.h"

#include "VeyraVanguardCharacter.generated.h"

class UAbilitySystemComponent;
class UCameraComponent;
class USpringArmComponent;
struct FOnAttributeChangeData;

/**
 * A Vanguard's body in the world: the avatar of its participant's Ability System Component, which
 * lives on the PlayerState (ADR-006 §4). The server moves it through its AVeyraVanguardController;
 * every client, including its owner, shows the replicated result (ADR-006 §7, no prediction).
 */
UCLASS()
class VEYRAMATCH_API AVeyraVanguardCharacter : public ACharacter, public IAbilitySystemInterface, public IVeyraTeamMember
{
	GENERATED_BODY()

public:
	AVeyraVanguardCharacter(const FObjectInitializer& ObjectInitializer);

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual EVeyraTeam GetVeyraTeam() const override;
	virtual void PostInitializeComponents() override;

protected:
	virtual void OnPlayerStateChanged(APlayerState* NewPlayerState, APlayerState* OldPlayerState) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void FollowMoveSpeed(UAbilitySystemComponent* AbilitySystem);
	void OnMoveSpeedChanged(const FOnAttributeChangeData& Change);

	TWeakObjectPtr<UAbilitySystemComponent> FollowedAbilitySystem;
	FDelegateHandle MoveSpeedChangedHandle;

	/** The owning player's top-down view (UVeyraCameraSettings). Presentation only. */
	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraArm;

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<UCameraComponent> Camera;
};
