// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Shell/VeyraShellButton.h"

#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Shell/VeyraShellStyle.h"
#include "Shell/VeyraShellStyleSettings.h"

UVeyraShellButton* UVeyraShellButton::Make(UWidgetTree& Tree, const FText& Label, TFunction<void()> Action, bool bEnabled, bool bSelected)
{
	return MakeKind(Tree, EVeyraShellButtonKind::Secondary, Label, MoveTemp(Action), bEnabled, bSelected);
}

UVeyraShellButton* UVeyraShellButton::MakeKind(UWidgetTree& Tree, EVeyraShellButtonKind Kind, const FText& Label, TFunction<void()> Action, bool bEnabled,
	bool bSelected)
{
	const UVeyraShellStyleSettings& Style = *GetDefault<UVeyraShellStyleSettings>();
	UVeyraShellButton* Button = Create(Tree, Label, MoveTemp(Action), bEnabled);
	Button->SetStyle(VeyraShellStyle::ButtonStyleFor(Kind, bSelected));
	UTextBlock* Text = VeyraShellStyle::MakeText(Tree, Label, VeyraShellStyle::LabelRoleFor(Kind));
	// A tab or a quiet action speaks up only when it is the one shown.
	const bool bQuiet = Kind == EVeyraShellButtonKind::Tab || Kind == EVeyraShellButtonKind::Quiet;
	if (bQuiet && !bSelected)
	{
		Text->SetColorAndOpacity(FSlateColor(Style.MutedTextColor));
	}
	if (!bEnabled && Kind != EVeyraShellButtonKind::Primary)
	{
		Text->SetColorAndOpacity(FSlateColor(Style.MutedTextColor.CopyWithNewOpacity(0.6f)));
	}
	// A primary action is never narrower than the least width that marks it the way forward.
	if (Kind == EVeyraShellButtonKind::Primary)
	{
		Text->SetJustification(ETextJustify::Center);
		Text->SetMinDesiredWidth(Style.PrimaryButtonWidth - Style.ButtonPadding * 4.0f);
	}
	Button->AddChild(Text);
	return Button;
}

UVeyraShellButton* UVeyraShellButton::MakeKindNamed(UWidgetTree& Tree, EVeyraShellButtonKind Kind, const FText& Label, const FText& Shown, TFunction<void()> Action,
	bool bEnabled, bool bSelected)
{
	UVeyraShellButton* Button = MakeKind(Tree, Kind, Shown, MoveTemp(Action), bEnabled, bSelected);
	Button->Label = Label;
	Button->SetToolTipText(Label);
	return Button;
}

UVeyraShellButton* UVeyraShellButton::MakeWithContent(UWidgetTree& Tree, const FText& Label, UWidget& Content, TFunction<void()> Action, bool bEnabled, bool bSelected)
{
	const UVeyraShellStyleSettings& Style = *GetDefault<UVeyraShellStyleSettings>();
	UVeyraShellButton* Button = Create(Tree, Label, MoveTemp(Action), bEnabled);
	// A thin edge around the content, in the frame's colour when selected.
	Button->SetStyle(VeyraShellStyle::ButtonStyle(bSelected ? Style.FrameColor : Style.ButtonColor, Style.TilePadding));
	Button->AddChild(&Content);
	return Button;
}

namespace
{
	/** Every live shell button, so the one under keyboard focus can be found from its Slate widget. */
	TArray<TWeakObjectPtr<UVeyraShellButton>>& LiveButtons()
	{
		static TArray<TWeakObjectPtr<UVeyraShellButton>> Live;
		return Live;
	}
}

UVeyraShellButton* UVeyraShellButton::FindBySlate(const TSharedPtr<SWidget>& Widget)
{
	if (!Widget.IsValid())
	{
		return nullptr;
	}
	TArray<TWeakObjectPtr<UVeyraShellButton>>& Live = LiveButtons();
	Live.RemoveAll([](const TWeakObjectPtr<UVeyraShellButton>& Each) { return !Each.IsValid(); });
	for (const TWeakObjectPtr<UVeyraShellButton>& Each : Live)
	{
		if (Each->GetCachedWidget() == Widget)
		{
			return Each.Get();
		}
	}
	return nullptr;
}

void UVeyraShellButton::ShowEnhancedFocus(bool bShow)
{
	if (bShow == bEnhancedFocus)
	{
		return;
	}
	bEnhancedFocus = bShow;
	if (!bShow)
	{
		SetStyle(OwnStyle);
		return;
	}
	OwnStyle = GetStyle();
	const UVeyraShellStyleSettings& Style = *GetDefault<UVeyraShellStyleSettings>();
	FButtonStyle Focused = OwnStyle;
	for (FSlateBrush* Brush : { &Focused.Normal, &Focused.Hovered, &Focused.Pressed })
	{
		Brush->DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush->OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Brush->OutlineSettings.CornerRadii = FVector4(Style.ButtonCornerRadius);
		Brush->OutlineSettings.Color = Style.FocusOutlineColor;
		Brush->OutlineSettings.Width = Style.FocusOutlineWidth;
	}
	SetStyle(Focused);
}

float UVeyraShellButton::GetOutlineWidth() const
{
	return GetStyle().Normal.OutlineSettings.Width;
}

FLinearColor UVeyraShellButton::GetOutlineColor() const
{
	return GetStyle().Normal.OutlineSettings.Color.GetSpecifiedColor();
}

FLinearColor UVeyraShellButton::GetFillColor() const
{
	return GetStyle().Normal.TintColor.GetSpecifiedColor();
}

void UVeyraShellButton::BeginDestroy()
{
	LiveButtons().RemoveAll([this](const TWeakObjectPtr<UVeyraShellButton>& Each) { return !Each.IsValid() || Each.Get() == this; });
	Super::BeginDestroy();
}

UVeyraShellButton* UVeyraShellButton::Create(UWidgetTree& Tree, const FText& Label, TFunction<void()> Action, bool bEnabled)
{
	UVeyraShellButton* Button = Tree.ConstructWidget<UVeyraShellButton>(UVeyraShellButton::StaticClass());
	LiveButtons().Add(Button);
	Button->Label = Label;
	Button->Action = MoveTemp(Action);
	Button->SetIsEnabled(bEnabled);
	Button->OnClicked.AddUniqueDynamic(Button, &UVeyraShellButton::HandleClicked);
	return Button;
}

void UVeyraShellButton::Press()
{
	if (GetIsEnabled())
	{
		HandleClicked();
	}
}

void UVeyraShellButton::HandleClicked()
{
	if (Action)
	{
		// The action may rebuild the screen that holds this button, so it runs from a copy.
		const TFunction<void()> Run = Action;
		Run();
	}
}

UVeyraShellButton& UVeyraShellButton::KeepLabelOnOneLine()
{
	if (UTextBlock* Text = Cast<UTextBlock>(GetChildAt(0)))
	{
		Text->SetAutoWrapText(false);
	}
	return *this;
}
