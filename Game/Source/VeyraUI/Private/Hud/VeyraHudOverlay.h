// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "GameFramework/Actor.h"

#include "VeyraHudOverlay.generated.h"

class UVeyraGreyboxSubsystem;

/**
 * Draws the grey-box HUD (VeyraGreyboxHud) when the local player's HUD renders its overlay actors.
 * The HUD passes them its own canvas, which the menus' widgets cover; the engine's post-render event
 * passes the debug canvas instead, which is drawn above every widget. Client only, never replicated.
 */
UCLASS(NotPlaceable, Transient)
class AVeyraHudOverlay : public AActor
{
	GENERATED_BODY()

public:
	AVeyraHudOverlay();

	void SetGreybox(UVeyraGreyboxSubsystem& InGreybox) { Greybox = &InGreybox; }

	virtual void PostRenderFor(APlayerController* Viewer, UCanvas* Canvas, FVector CameraPosition, FVector CameraDir) override;

private:
	TWeakObjectPtr<UVeyraGreyboxSubsystem> Greybox;
};
