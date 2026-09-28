// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Hud/VeyraHudOverlay.h"

#include "Engine/Canvas.h"
#include "Greybox/VeyraGreyboxSubsystem.h"
#include "Hud/VeyraGreyboxHud.h"

AVeyraHudOverlay::AVeyraHudOverlay()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	SetCanBeDamaged(false);
}

void AVeyraHudOverlay::PostRenderFor(APlayerController* Viewer, UCanvas* Canvas, FVector /*CameraPosition*/, FVector /*CameraDir*/)
{
	if (const UVeyraGreyboxSubsystem* Subsystem = Greybox.Get(); Subsystem && Canvas)
	{
		VeyraGreyboxHud::Draw(*Canvas, *Subsystem, Viewer);
	}
}
