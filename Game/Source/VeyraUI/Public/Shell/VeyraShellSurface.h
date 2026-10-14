// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Components/Border.h"
#include "Styling/SlateBrush.h"

#include "VeyraShellSurface.generated.h"

/** A dashed edge drawn around a surface: dashes Length long, Gap apart, in Color; none while Length is 0. */
struct FVeyraShellDash
{
	FLinearColor Color = FLinearColor::Transparent;
	float Length = 0.0f;
	float Gap = 0.0f;
};

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

	/** A dashed edge drawn over the fill and the content, such as an open place waiting to be filled (ADR-083 §6). */
	void SetDash(const FVeyraShellDash& InDash);
	const FVeyraShellDash& GetDash() const { return Dash; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	UPROPERTY(Transient)
	FSlateBrush Outline;

	FVeyraShellDash Dash;
};
