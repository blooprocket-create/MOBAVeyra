// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Components/Border.h"
#include "Styling/SlateBrush.h"

#include "VeyraShellSurface.generated.h"

/**
 * A Smoked Crucible surface (ADR-078 §3): a border whose background is the surface's fill, tinted its colour, with its
 * outline drawn over it, so one widget is both. The fill's tint stays the surface's colour for the look to make opaque.
 */
UCLASS()
class VEYRAUI_API UVeyraShellSurface : public UBorder
{
	GENERATED_BODY()

public:
	/** The outline drawn over the fill and the content, around the surface's edge. */
	void SetOutline(const FSlateBrush& InOutline);
	const FSlateBrush& GetOutline() const { return Outline; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	UPROPERTY(Transient)
	FSlateBrush Outline;
};
