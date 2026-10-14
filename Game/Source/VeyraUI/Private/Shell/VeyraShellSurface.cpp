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
			, _Dash(nullptr)
			, _FlipForRightToLeftFlowDirection(false)
		{}
			SLATE_ARGUMENT(const FSlateBrush*, Outline)
			SLATE_ARGUMENT(const FVeyraShellDash*, Dash)
			SLATE_ARGUMENT(bool, FlipForRightToLeftFlowDirection)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			SBorder::Construct(SBorder::FArguments().FlipForRightToLeftFlowDirection(InArgs._FlipForRightToLeftFlowDirection));
			Outline = InArgs._Outline;
			Dash = InArgs._Dash;
		}

		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
			int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
		{
			const int32 Top = PaintDash(AllottedGeometry, OutDrawElements,
				SBorder::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled), InWidgetStyle);
			if (!Outline || Outline->DrawAs == ESlateBrushDrawType::NoDrawType || !Outline->GetResourceObject())
			{
				return Top;
			}
			const FLinearColor Tint = InWidgetStyle.GetColorAndOpacityTint() * Outline->GetTint(InWidgetStyle);
			FSlateDrawElement::MakeBox(OutDrawElements, Top + 1, AllottedGeometry.ToPaintGeometry(), Outline, ESlateDrawEffect::None, Tint);
			return Top + 1;
		}

	private:
		/** The dashed edge, if any, along each side in turn; returns the layer it drew on. */
		int32 PaintDash(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle) const
		{
			if (!Dash || Dash->Length <= 0.0f)
			{
				return LayerId;
			}
			// Half a unit in, so the one-unit line falls inside the surface.
			const FVector2f Size = FVector2f(AllottedGeometry.GetLocalSize()) - FVector2f(0.5f);
			const FVector2f Corners[] = { FVector2f(0.5f), FVector2f(Size.X, 0.5f), Size, FVector2f(0.5f, Size.Y) };
			const FLinearColor Color = InWidgetStyle.GetColorAndOpacityTint() * Dash->Color;
			const float Step = Dash->Length + FMath::Max(Dash->Gap, 0.0f);
			for (int32 Side = 0; Side < UE_ARRAY_COUNT(Corners); ++Side)
			{
				const FVector2f From = Corners[Side];
				const FVector2f To = Corners[(Side + 1) % UE_ARRAY_COUNT(Corners)];
				const float Length = FVector2f::Distance(From, To);
				const FVector2f Along = Length > 0.0f ? (To - From) / Length : FVector2f::ZeroVector;
				for (float At = 0.0f; At < Length; At += Step)
				{
					TArray<FVector2f> Points = { From + Along * At, From + Along * FMath::Min(At + Dash->Length, Length) };
					FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(), MoveTemp(Points), ESlateDrawEffect::None, Color, false);
				}
			}
			return LayerId + 1;
		}

		const FSlateBrush* Outline = nullptr;
		const FVeyraShellDash* Dash = nullptr;
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

void UVeyraShellSurface::SetDash(const FVeyraShellDash& InDash)
{
	Dash = InDash;
	if (MyBorder.IsValid())
	{
		MyBorder->Invalidate(EInvalidateWidgetReason::Paint);
	}
}

TSharedRef<SWidget> UVeyraShellSurface::RebuildWidget()
{
	// UBorder's own widget, with the outline drawn over it; its slot and properties are UBorder's.
	MyBorder = SNew(SVeyraShellSurface).Outline(&Outline).Dash(&Dash).FlipForRightToLeftFlowDirection(bFlipForRightToLeftFlowDirection);
	if (GetChildrenCount() > 0)
	{
		Cast<UBorderSlot>(GetContentSlot())->BuildSlot(MyBorder.ToSharedRef());
	}
	return MyBorder.ToSharedRef();
}
