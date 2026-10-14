// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Shell/VeyraShellSurface.h"

#include "Components/BorderSlot.h"
#include "Rendering/DrawElements.h"
#include "Styling/WidgetStyle.h"
#include "Widgets/Layout/SBorder.h"

namespace
{
	/** A border that draws an outline over its background and content. */
	class SVeyraShellSurface : public SBorder
	{
	public:
		SLATE_BEGIN_ARGS(SVeyraShellSurface)
			: _Outline(nullptr)
			, _FlipForRightToLeftFlowDirection(false)
		{}
			SLATE_ARGUMENT(const FSlateBrush*, Outline)
			SLATE_ARGUMENT(bool, FlipForRightToLeftFlowDirection)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			SBorder::Construct(SBorder::FArguments().FlipForRightToLeftFlowDirection(InArgs._FlipForRightToLeftFlowDirection));
			Outline = InArgs._Outline;
		}

		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
			int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
		{
			const int32 Top = SBorder::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
			if (!Outline || Outline->DrawAs == ESlateBrushDrawType::NoDrawType || !Outline->GetResourceObject())
			{
				return Top;
			}
			const FLinearColor Tint = InWidgetStyle.GetColorAndOpacityTint() * Outline->GetTint(InWidgetStyle);
			FSlateDrawElement::MakeBox(OutDrawElements, Top + 1, AllottedGeometry.ToPaintGeometry(), Outline, ESlateDrawEffect::None, Tint);
			return Top + 1;
		}

	private:
		const FSlateBrush* Outline = nullptr;
	};
}

void UVeyraShellSurface::SetOutline(const FSlateBrush& InOutline)
{
	Outline = InOutline;
	if (MyBorder.IsValid())
	{
		MyBorder->Invalidate(EInvalidateWidgetReason::Paint);
	}
}

TSharedRef<SWidget> UVeyraShellSurface::RebuildWidget()
{
	// UBorder's own widget, with the outline drawn over it; its slot and properties are UBorder's.
	MyBorder = SNew(SVeyraShellSurface).Outline(&Outline).FlipForRightToLeftFlowDirection(bFlipForRightToLeftFlowDirection);
	if (GetChildrenCount() > 0)
	{
		Cast<UBorderSlot>(GetContentSlot())->BuildSlot(MyBorder.ToSharedRef());
	}
	return MyBorder.ToSharedRef();
}
