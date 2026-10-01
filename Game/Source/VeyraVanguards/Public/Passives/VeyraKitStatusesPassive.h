// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Passives/VeyraPassive.h"

#include "VeyraKitStatusesPassive.generated.h"

/**
 * A passive made wholly of its kit's statuses and the reactions to them (ADR-026 §1–§2), as Korruk's
 * Embedded. The abilities apply the statuses, Abilities turns one into another at its most stacks, and
 * the reactions detonate them, so the passive runs nothing of its own. Its data is an entry in
 * Vanguards.json's kitStatuses map.
 */
UCLASS()
class VEYRAVANGUARDS_API UVeyraKitStatusesPassive : public UVeyraPassive
{
	GENERATED_BODY()
};
