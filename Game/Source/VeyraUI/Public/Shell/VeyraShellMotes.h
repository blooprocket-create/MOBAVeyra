// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Components/Widget.h"
#include "Widgets/SLeafWidget.h"

#include "VeyraShellMotes.generated.h"

/**
 * Flux motes drifting up behind the menus, as on the website (ADR-082 §7): soft points of light in the Flux teal and brass,
 * rising and swaying, fading in from the foot and out toward the head. Presentation only; none under Reduce UI Animation.
 */
class VEYRAUI_API SVeyraShellMotes : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SVeyraShellMotes) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override { return FVector2D::ZeroVector; }

	/** How many motes it holds now. For tests. */
	int32 Num() const { return Motes.Num(); }

private:
	struct FMote
	{
		FVector2D At = FVector2D::ZeroVector;
		float Radius = 1.0f;
		float Rise = 0.0f;
		float Phase = 0.0f;
		float SwaySpeed = 0.0f;
		float Flicker = 0.0f;
		bool bBrass = false;
		bool bHot = false;
	};

	EActiveTimerReturnType Step(double CurrentTime, float DeltaTime);
	FMote Made(const FVector2D& Area, bool bAnywhere);
	/** Fills or trims the motes to the count Area holds, then moves them by Seconds. */
	void Advance(const FVector2D& Area, float Seconds);

	TArray<FMote> Motes;
	FRandomStream Random;
	/** The area it last painted, which its step moves the motes over. */
	mutable FVector2D LastArea = FVector2D::ZeroVector;
	/** The area the motes were last scattered over. */
	FVector2D LastAreaMoved = FVector2D::ZeroVector;
};

/** The motes as a widget for a screen's layers. */
UCLASS()
class VEYRAUI_API UVeyraShellMotes : public UWidget
{
	GENERATED_BODY()

public:
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	TSharedPtr<SVeyraShellMotes> Motes;
};
