// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Commandlets/Commandlet.h"

#include "VeyraEffectsCommandlet.generated.h"

/**
 * Builds the presentation's Niagara effects (ADR-063 §4) from Game/ArtSource/Presentation/Effects.json: each system
 * starts as a copy of the engine template its spec names, gains the spec's user colour, and has every colour input of
 * its particles linked to that colour, so one system serves every side. Its sprites and ribbons draw with the spec's
 * generated materials, which glow alike under any exposure; it refuses a system that would keep an engine default.
 * Game/Scripts/BuildEffects.ps1 runs it.
 *
 * With -Describe it changes nothing and writes each template's topology to Saved/Effects instead. It uses the
 * engine's Niagara editing utilities, approved for this generator only (ADR-063 §4). It needs the editor.
 */
UCLASS()
class UVeyraEffectsCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UVeyraEffectsCommandlet();

	virtual int32 Main(const FString& Params) override;
};
