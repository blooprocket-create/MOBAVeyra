// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "GenericPlatform/GenericWindow.h"
#include "Math/IntPoint.h"
#include "Math/Vector2D.h"
#include "Misc/Optional.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "VeyraMatchDisplaySubsystem.generated.h"

class UVeyraClientFlowSubsystem;

/**
 * Gives a match the screen and the client its window back (UVeyraDisplaySettings): when the
 * coordinator starts loading a match after champion select, it remembers the window and switches to
 * the match's display mode; when the match is over, it puts the window back as it was for the
 * results. It follows the coordinator's state and decides nothing about the flow. It exists only in a
 * game signed in by a launcher, and does nothing without a window (a commandlet or a test).
 */
UCLASS()
class VEYRAUI_API UVeyraMatchDisplaySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Whether the match has the screen now: the client's window is remembered, to be put back. */
	bool HasTheScreen() const { return Saved.IsSet(); }

	/** The player changed the match's display mode during the match: it takes the screen again, in the new mode. */
	void RetakeTheScreen();

private:
	/** The client's window as it was before the match took the screen. */
	struct FSavedWindow
	{
		EWindowMode::Type Mode = EWindowMode::Windowed;
		FIntPoint Size = FIntPoint::ZeroValue;
		FVector2D Position = FVector2D::ZeroVector;
		/** Whether the match changed it: a Windowed match leaves the window alone. */
		bool bChanged = false;
	};

	void Update();
	void TakeTheScreen();
	void GiveTheScreenBack();
	void ReleaseClient();

	TWeakObjectPtr<UVeyraClientFlowSubsystem> Flow;
	TOptional<FSavedWindow> Saved;
	FDelegateHandle ChangedHandle;
	FDelegateHandle EndingHandle;
};
