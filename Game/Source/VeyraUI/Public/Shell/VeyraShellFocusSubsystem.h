// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "VeyraShellFocusSubsystem.generated.h"

class UVeyraShellButton;

/** Which shell button wears the enhanced focus outline: the focused one under Enhanced, none otherwise (ADR-055 §3). */
struct VEYRAUI_API FVeyraFocusLight
{
	/** Lights Focused if bEnhanced; the one lit before goes back to its own style. */
	void Follow(UVeyraShellButton* Focused, bool bEnhanced);

	UVeyraShellButton* GetLit() const { return Lit.Get(); }

private:
	TWeakObjectPtr<UVeyraShellButton> Lit;
};

/**
 * The enhanced keyboard focus indicator (Settings Bible Proposal 74; ADR-055 §3): while the player chose Enhanced, the
 * shell button keyboard focus is on draws a thicker, high-contrast outline, in every menu. Under Standard, Slate's own
 * focus outline shows alone. It changes no navigation or targeting.
 */
UCLASS()
class VEYRAUI_API UVeyraShellFocusSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	bool Tick(float DeltaSeconds);

	FVeyraFocusLight Light;
	FTSTicker::FDelegateHandle TickHandle;
};