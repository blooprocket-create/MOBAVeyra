// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "CoreMinimal.h"

class AActor;
class USceneComponent;

/**
 * Where a unit's body is drawn on this machine (ADR-065 §12). A machine that only shows a unit moves its capsule to each
 * update from the server as it arrives, and eases its character's mesh after it, so the body glides where the capsule
 * steps. What is drawn of a body hangs from that mesh, and what is drawn over it reads the mesh's place. On the server, and
 * for units that are not characters, the body is drawn where the unit stands.
 */
namespace VeyraDrawnBody
{
	/** The component a unit's drawn body hangs from: a character's mesh, or else its root. */
	VEYRACOMBAT_API USceneComponent* AnchorOf(const AActor& Unit);

	/** Where the unit's body is drawn: its place as the mesh eases after the capsule, without the mesh's own offset. */
	VEYRACOMBAT_API FVector LocationOf(const AActor& Unit);
}
