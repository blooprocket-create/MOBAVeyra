// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Chat/VeyraChatComposer.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Framework/Application/SlateApplication.h"
#include "Greybox/VeyraGreyboxSettings.h"
#include "Hud/VeyraChatLogModel.h"
#include "Hud/VeyraHudLayout.h"
#include "Settings/VeyraInterfacePreferences.h"
#include "Shell/VeyraShellStyle.h"
#include "Shell/VeyraShellStyleSettings.h"
#include "Slots/VeyraAbilitySlot.h"
#include "Widgets/SViewport.h"

#define LOCTEXT_NAMESPACE "VeyraChatComposer"

bool UVeyraChatComposer::Initialize()
{
	const bool bFirst = Super::Initialize();
	if (bFirst && WidgetTree && !WidgetTree->RootWidget)
	{
		const UVeyraShellStyleSettings& Style = *GetDefault<UVeyraShellStyleSettings>();
		UBorder* Surface = VeyraShellStyle::MakeBorder(*WidgetTree, Style.SurfaceColor, Style.Spacing / 2.0f);
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Surface->SetContent(Row);
		Label = VeyraShellStyle::MakeText(*WidgetTree, FText::GetEmpty(), VeyraShellStyle::EVeyraShellText::Eyebrow);
		Label->SetAutoWrapText(false);
		UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(Label);
		LabelSlot->SetVerticalAlignment(VAlign_Center);
		LabelSlot->SetPadding(FMargin(0.0f, 0.0f, Style.Spacing / 2.0f, 0.0f));

		// Styled as the Settings search is, so the client's fields look alike.
		Field = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
		VeyraShellStyle::StyleTextField(*Field, Style.Spacing / 2.0f);
		Field->SetHintText(LOCTEXT("Hint", "Enter sends, Tab switches Team and All, Escape closes"));
		Field->OnTextChanged.AddUniqueDynamic(this, &UVeyraChatComposer::HandleTextChanged);
		UHorizontalBoxSlot* FieldSlot = Row->AddChildToHorizontalBox(Field);
		FieldSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		FieldSlot->SetVerticalAlignment(VAlign_Center);
		WidgetTree->RootWidget = Surface;
	}
	return bFirst;
}

void UVeyraChatComposer::Show(EVeyraChatChannel InChannel, int32 InMaxCharacters, TFunction<void(EVeyraChatChannel, const FString&)> InSubmit, TFunction<void()> InClose)
{
	Channel = InChannel;
	MaxCharacters = InMaxCharacters;
	Submit = MoveTemp(InSubmit);
	Close = MoveTemp(InClose);
	if (Label)
	{
		Label->SetText(ChannelLabel(Channel));
	}
	SetTyped(FString());
}

FText UVeyraChatComposer::ChannelLabel(EVeyraChatChannel Channel)
{
	return Channel == EVeyraChatChannel::All ? LOCTEXT("All", "[All]") : LOCTEXT("Team", "[Team]");
}

void UVeyraChatComposer::SwitchChannel()
{
	Channel = Channel == EVeyraChatChannel::All ? EVeyraChatChannel::Team : EVeyraChatChannel::All;
	if (Label)
	{
		Label->SetText(ChannelLabel(Channel));
	}
}

FString UVeyraChatComposer::GetTyped() const
{
	return Field ? Field->GetText().ToString() : FString();
}

void UVeyraChatComposer::SetTyped(const FString& Text)
{
	if (Field)
	{
		Field->SetText(FText::FromString(Text));
	}
}

void UVeyraChatComposer::Send()
{
	const FString Typed = GetTyped();
	// Copied first: closing may let the screen that opened the composer go.
	const TFunction<void(EVeyraChatChannel, const FString&)> Sent = Submit;
	const TFunction<void()> Closed = Close;
	if (Sent && !Typed.TrimStartAndEnd().IsEmpty())
	{
		Sent(Channel, Typed);
	}
	if (Closed)
	{
		Closed();
	}
}

void UVeyraChatComposer::Cancel()
{
	const TFunction<void()> Closed = Close;
	if (Closed)
	{
		Closed();
	}
}

TSharedRef<SWidget> UVeyraChatComposer::GetFocusTarget()
{
	return Field ? Field->TakeWidget() : TakeWidget();
}

void UVeyraChatComposer::HandleTextChanged(const FText& Text)
{
	// No more than the server takes (ADR-029 §2).
	if (MaxCharacters > 0 && Text.ToString().Len() > MaxCharacters)
	{
		SetTyped(Text.ToString().Left(MaxCharacters));
	}
}

void UVeyraChatComposer::Place()
{
	const ULocalPlayer* Player = GetOwningLocalPlayer();
	FVector2D Viewport = FVector2D::ZeroVector;
	if (!Player || !Player->ViewportClient)
	{
		return;
	}
	Player->ViewportClient->GetViewportSize(Viewport);
	if (Viewport.IsNearlyZero())
	{
		return;
	}
	// Where the HUD's layout puts the chat: inside the safe area, above a deck that reaches under it (ADR-059 §1-§2).
	const UVeyraGreyboxSettings& Settings = *GetDefault<UVeyraGreyboxSettings>();
	const FVeyraInterfacePreferences Preferences = VeyraInterfacePreferences::Resolve(Settings, VeyraInterfacePreferences::StoreOf(this));
	const FVeyraChatFrame Frame = VeyraHudLayout::Arrange(Viewport, Settings, Preferences, UE_ARRAY_COUNT(VeyraAbilitySlots::All)).Chat;
	const FBox2D Placed(Frame.InputTopLeft, Frame.InputTopLeft + Frame.InputSize);
	if (Placed == PlacedAt)
	{
		return;
	}
	PlacedAt = Placed;
	// The frame is in pixels; the viewport's sizes are in its own units, so the size loses the DPI scale as the position does.
	const float DpiScale = UWidgetLayoutLibrary::GetViewportScale(this);
	SetDesiredSizeInViewport(DpiScale > 0.0f ? Frame.InputSize / DpiScale : Frame.InputSize);
	SetPositionInViewport(Frame.InputTopLeft, /*bRemoveDPIScale*/ true);
}

void UVeyraChatComposer::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	Place();
	// A click on the battleground hands the keyboard to the game; the composer takes it back, so typing
	// never casts (ADR-029 §5). Another screen that takes the keyboard keeps it.
	const ULocalPlayer* Player = GetOwningLocalPlayer();
	if (!Field || Field->HasKeyboardFocus() || !FSlateApplication::IsInitialized() || !Player || !Player->ViewportClient)
	{
		return;
	}
	const TSharedPtr<SWidget> Focused = FSlateApplication::Get().GetKeyboardFocusedWidget();
	const TSharedPtr<SViewport> Game = Player->ViewportClient->GetGameViewportWidget();
	if (!Focused.IsValid() || Focused.Get() == static_cast<SWidget*>(Game.Get()))
	{
		Field->SetKeyboardFocus();
	}
}

FReply UVeyraChatComposer::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// Seen before the field sees them, so none of these types into it or reaches the game.
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Enter)
	{
		Send();
		return FReply::Handled();
	}
	if (Key == EKeys::Escape)
	{
		Cancel();
		return FReply::Handled();
	}
	if (Key == EKeys::Tab)
	{
		SwitchChannel();
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

FReply UVeyraChatComposer::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// Whatever the field leaves stops here, so the letters typed never cast (ADR-029 §5).
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
