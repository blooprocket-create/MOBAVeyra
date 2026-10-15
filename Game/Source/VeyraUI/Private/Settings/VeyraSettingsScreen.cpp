// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Settings/VeyraSettingsScreen.h"

#include "Greybox/VeyraGreyboxSettings.h"
#include "Settings/VeyraInterfacePreferences.h"
#include "Shell/VeyraShellLook.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Settings/VeyraDisplayApplier.h"
#include "Settings/VeyraDisplayRules.h"
#include "Settings/VeyraSettingsSlider.h"
#include "Shell/VeyraShellBrushes.h"
#include "Styling/SlateBrush.h"
#include "Shell/VeyraShellStyle.h"
#include "Shell/VeyraShellStyleSettings.h"
#include "Text/VeyraContentText.h"
#include "UObject/UnrealType.h"
#include "VeyraSettingsStore.h"
#include "VeyraSettingsSubsystem.h"

#define LOCTEXT_NAMESPACE "VeyraSettingsScreen"

namespace VeyraSettingsLayout
{
	using VeyraShellStyle::EVeyraShellSurface;
	using VeyraShellStyle::EVeyraShellText;

	const UVeyraShellStyleSettings& Style()
	{
		return *GetDefault<UVeyraShellStyleSettings>();
	}

	/** Child takes the row's or column's spare room. */
	void AddFilling(UPanelWidget& Parent, UWidget& Child)
	{
		if (UVerticalBox* Column = Cast<UVerticalBox>(&Parent))
		{
			Column->AddChildToVerticalBox(&Child)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
		else if (UHorizontalBox* Row = Cast<UHorizontalBox>(&Parent))
		{
			UHorizontalBoxSlot* Slot = Row->AddChildToHorizontalBox(&Child);
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot->SetVerticalAlignment(VAlign_Center);
		}
	}

	void AddStretch(UWidgetTree& Tree, UPanelWidget& Parent)
	{
		AddFilling(Parent, *Tree.ConstructWidget<USpacer>(USpacer::StaticClass()));
	}

	UTextBlock* AddText(UWidgetTree& Tree, UPanelWidget& Parent, const FText& Text, EVeyraShellText Role)
	{
		UTextBlock* Block = VeyraShellStyle::MakeText(Tree, Text, Role);
		VeyraShellStyle::AddSpaced(Parent, *Block);
		return Block;
	}

	/** Child in a box Width wide. */
	USizeBox* Sized(UWidgetTree& Tree, UWidget& Child, float Width)
	{
		USizeBox* Box = Tree.ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Box->SetWidthOverride(Width);
		Box->AddChild(&Child);
		return Box;
	}

	/** Child in a square box Size on a side. */
	USizeBox* Square(UWidgetTree& Tree, UWidget& Child, float Size)
	{
		USizeBox* Box = Tree.ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Box->SetWidthOverride(Size);
		Box->SetHeightOverride(Size);
		Box->AddChild(&Child);
		return Box;
	}

	/** The slot of Parent's last child, for its alignment. */
	UPanelSlot* LastSlot(UPanelWidget& Parent)
	{
		const int32 Count = Parent.GetChildrenCount();
		UWidget* Last = Count > 0 ? Parent.GetChildAt(Count - 1) : nullptr;
		return Last ? Last->Slot : nullptr;
	}

	/** A button that draws only its content: a switch's or a radio's face shows its own state (ADR-083 §3). */
	FButtonStyle Bare()
	{
		FButtonStyle Button;
		Button.SetNormal(VeyraShellBrushes::None());
		Button.SetHovered(VeyraShellBrushes::None());
		Button.SetPressed(VeyraShellBrushes::None());
		Button.SetDisabled(VeyraShellBrushes::None());
		Button.SetNormalPadding(FMargin(0.0f));
		Button.SetPressedPadding(FMargin(0.0f));
		return Button;
	}

	/** One option of a segmented choice: filled in the accent when it is the value, lit under the pointer otherwise. */
	FButtonStyle SegmentStyle(bool bSelected)
	{
		const UVeyraShellStyleSettings& Look = Style();
		const FSlateColorBrush Selected(Look.AccentColor);
		const FSlateColorBrush Hovered(Look.ButtonHoveredColor);
		FButtonStyle Button;
		if (bSelected)
		{
			Button.SetNormal(Selected);
			Button.SetHovered(Selected);
		}
		else
		{
			Button.SetNormal(VeyraShellBrushes::None());
			Button.SetHovered(Hovered);
		}
		Button.SetPressed(Selected);
		Button.SetDisabled(VeyraShellBrushes::None());
		const FMargin Padding(Look.ButtonPadding * 1.5f, Look.ButtonPadding);
		Button.SetNormalPadding(Padding);
		Button.SetPressedPadding(Padding);
		return Button;
	}

	/** A range's slider (ADR-083 §3): its bar in the hairline, its thumb a round knob in the accent, the switch's size. */
	FSliderStyle SliderStyle()
	{
		const UVeyraShellStyleSettings& Look = Style();
		const float Knob = Look.SettingsSwitchHeight * 0.75f;
		FSliderStyle Slider;
		const FSlateColorBrush Bar(Look.ButtonEdgeColor);
		Slider.SetNormalBarImage(Bar);
		Slider.SetHoveredBarImage(FSlateColorBrush(Look.AccentColor));
		Slider.SetDisabledBarImage(FSlateColorBrush(Look.ButtonDisabledColor));
		FSlateRoundedBoxBrush Thumb(Look.AccentColor, Knob / 2.0f);
		Thumb.ImageSize = FVector2D(Knob);
		Slider.SetNormalThumbImage(Thumb);
		Slider.SetHoveredThumbImage(Thumb);
		FSlateRoundedBoxBrush Off(Look.MutedTextColor, Knob / 2.0f);
		Off.ImageSize = FVector2D(Knob);
		Slider.SetDisabledThumbImage(Off);
		// The bar a sixth of the switch's height.
		Slider.SetBarThickness(Look.SettingsSwitchHeight / 6.0f);
		return Slider;
	}

	/** A switch's face: its track in the accent while on, its knob at the end its value is (ADR-083 §3). */
	UWidget* SwitchFace(UWidgetTree& Tree, bool bOn)
	{
		const UVeyraShellStyleSettings& Look = Style();
		const float Height = Look.SettingsSwitchHeight;
		USizeBox* Box = Tree.ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Box->SetWidthOverride(Look.SettingsSwitchWidth);
		Box->SetHeightOverride(Height);
		UOverlay* Layers = Tree.ConstructWidget<UOverlay>(UOverlay::StaticClass());
		Box->AddChild(Layers);
		UBorder* Track = Tree.ConstructWidget<UBorder>(UBorder::StaticClass());
		Track->SetBrush(FSlateRoundedBoxBrush(bOn ? Look.AccentColor : Look.ButtonColor, Height / 2.0f, Look.ButtonEdgeColor, bOn ? 0.0f : 1.0f));
		UOverlaySlot* TrackSlot = Layers->AddChildToOverlay(Track);
		TrackSlot->SetHorizontalAlignment(HAlign_Fill);
		TrackSlot->SetVerticalAlignment(VAlign_Fill);
		// The knob sits an eighth of the height in from the track's edge.
		const float Inset = Height / 8.0f;
		UOverlaySlot* KnobSlot = Layers->AddChildToOverlay(VeyraShellStyle::MakeDot(Tree, bOn ? Look.BackgroundColor : Look.MutedTextColor, Height - Inset * 2.0f));
		KnobSlot->SetHorizontalAlignment(bOn ? HAlign_Right : HAlign_Left);
		KnobSlot->SetVerticalAlignment(VAlign_Center);
		KnobSlot->SetPadding(FMargin(Inset));
		return Box;
	}

	/** A radio's face: a ring, filled at its centre when it is the value. */
	UWidget* RadioFace(UWidgetTree& Tree, bool bOn)
	{
		const UVeyraShellStyleSettings& Look = Style();
		const float Size = Look.SettingsRadioSize;
		UOverlay* Layers = Tree.ConstructWidget<UOverlay>(UOverlay::StaticClass());
		UBorder* Ring = Tree.ConstructWidget<UBorder>(UBorder::StaticClass());
		// The ring's line is an eighth of its size.
		Ring->SetBrush(FSlateRoundedBoxBrush(FLinearColor::Transparent, Size / 2.0f, bOn ? Look.AccentColor : Look.MutedTextColor, Size / 8.0f));
		Layers->AddChildToOverlay(Square(Tree, *Ring, Size));
		if (bOn)
		{
			UOverlaySlot* Dot = Layers->AddChildToOverlay(VeyraShellStyle::MakeDot(Tree, Look.AccentColor, Size / 2.0f));
			Dot->SetHorizontalAlignment(HAlign_Center);
			Dot->SetVerticalAlignment(VAlign_Center);
		}
		return Layers;
	}

	/** A line of small squares in Colors, each edged so a dark colour still shows on the panel (SET-8). */
	void AddSwatches(UWidgetTree& Tree, UPanelWidget& Parent, TConstArrayView<FLinearColor> Colors)
	{
		UHorizontalBox* Line = Tree.ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		for (const FLinearColor& Color : Colors)
		{
			UBorder* Swatch = Tree.ConstructWidget<UBorder>(UBorder::StaticClass());
			Swatch->SetBrush(VeyraShellStyle::EdgedFace(Color, Style().HairlineColor));
			USizeBox* Square = Tree.ConstructWidget<USizeBox>(USizeBox::StaticClass());
			Square->SetWidthOverride(Style().SettingsSwatchSize);
			Square->SetHeightOverride(Style().SettingsSwatchSize);
			Square->AddChild(Swatch);
			Line->AddChildToHorizontalBox(Square)->SetPadding(FMargin(0.0f, 0.0f, Style().Spacing, 0.0f));
		}
		VeyraShellStyle::AddSpaced(Parent, *Line);
	}
}

bool UVeyraSettingsScreen::Initialize()
{
	const bool bFirst = Super::Initialize();
	if (bFirst && WidgetTree && !WidgetTree->RootWidget)
	{
		const UVeyraShellStyleSettings& Style = VeyraSettingsLayout::Style();
		// The frame is built once; Present puts it on the shell's page or in a window over the screen (ADR-083 §1).
		Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		Scrim = VeyraShellStyle::MakeBorder(*WidgetTree, Style.MenuScrimColor, Style.ScreenPadding);
		Scrim->SetHorizontalAlignment(HAlign_Center);
		Scrim->SetVerticalAlignment(VAlign_Center);
		USizeBox* Window = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Window->SetWidthOverride(Style.SettingsWidth);
		Window->SetHeightOverride(Style.SettingsHeight);
		WindowPanel = VeyraShellStyle::MakeSurface(*WidgetTree, VeyraShellStyle::EVeyraShellSurface::Raised, FMargin(Style.ScreenPadding));
		Window->AddChild(WindowPanel);
		Scrim->SetContent(Window);
		UVerticalBox* Frame = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		WindowFrame = Frame;

		// The title, the search, and the screen's own actions.
		UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Title = VeyraShellStyle::MakeText(*WidgetTree, LOCTEXT("Title", "Settings"), VeyraShellStyle::EVeyraShellText::Title);
		Title->SetAutoWrapText(false);
		VeyraShellStyle::AddSpaced(*Header, *Title);
		// What the title and the actions leave, the search at the right of it; or, narrow, a line of its own.
		WideSearchHost = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		// Cut short, never laid over the actions, where even that room runs out.
		WideSearchHost->SetClipping(EWidgetClipping::ClipToBounds);
		VeyraSettingsLayout::AddFilling(*Header, *WideSearchHost);
		SearchBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
		SearchBox->SetHintText(LOCTEXT("SearchHint", "Search settings"));
		SearchBox->OnTextChanged.AddUniqueDynamic(this, &UVeyraSettingsScreen::HandleSearchChanged);
		SearchHolder = VeyraSettingsLayout::Sized(*WidgetTree, *SearchBox, Style.SettingsSearchWidth);
		Actions = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Header->AddChild(Actions);
		Frame->AddChild(Header);
		NarrowSearchHost = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Frame->AddChild(NarrowSearchHost);
		PlaceSearch();
		VeyraShellStyle::AddSpaced(*Frame, *VeyraShellStyle::MakeRule(*WidgetTree));

		// The categories, the settings and the About panel, laid out for the width given (ADR-083 §5).
		BodyHost = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		VeyraSettingsLayout::AddFilling(*Frame, *BodyHost);
		MakeBodyWidgets(0.0f);

		VeyraShellStyle::AddSpaced(*Frame, *VeyraShellStyle::MakeRule(*WidgetTree));
		Footer = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Frame->AddChild(Footer);
		WidgetTree->RootWidget = Root;
		Present();
		SetIsFocusable(true);
		StyleFrame();
	}
	return bFirst;
}

void UVeyraSettingsScreen::Present()
{
	UVerticalBox* Frame = WindowFrame.Get();
	Frame->RemoveFromParent();
	Scrim->RemoveFromParent();
	if (bAsPage)
	{
		// On the shell's page, the frame fills the space the shell gives it.
		UOverlaySlot* FrameSlot = Root->AddChildToOverlay(Frame);
		FrameSlot->SetHorizontalAlignment(HAlign_Fill);
		FrameSlot->SetVerticalAlignment(VAlign_Fill);
	}
	else
	{
		// A window over the client or the match, which stay in view around it (Settings Bible §6.2).
		WindowPanel->SetContent(Frame);
		UOverlaySlot* ScrimSlot = Root->AddChildToOverlay(Scrim);
		ScrimSlot->SetHorizontalAlignment(HAlign_Fill);
		ScrimSlot->SetVerticalAlignment(VAlign_Fill);
	}
}

void UVeyraSettingsScreen::StyleFrame()
{
	FrameLook = VeyraShellLook::Current();
	const UVeyraShellStyleSettings& Style = VeyraSettingsLayout::Style();
	VeyraShellStyle::StyleSurface(*WindowPanel, VeyraShellStyle::EVeyraShellSurface::Raised);
	// The window's title in the display face, as every screen's (ADR-082 §9).
	Title->SetFont(VeyraShellStyle::FontFor(VeyraShellStyle::EVeyraShellText::Title));
	VeyraShellStyle::StyleTextField(*SearchBox, Style.ButtonPadding);
}

int32 UVeyraSettingsScreen::GetTitleTextSize() const
{
	return Title ? FMath::RoundToInt32(Title->GetFont().Size) : 0;
}

FLinearColor UVeyraSettingsScreen::GetWindowFill() const
{
	return WindowPanel ? WindowPanel->Background.TintColor.GetSpecifiedColor() : FLinearColor::Transparent;
}

void UVeyraSettingsScreen::Show(UVeyraSettingsSubsystem& InSettings, bool bInLiveMatch, TFunction<void()> InClose, bool bInAsPage)
{
	StopListening();
	if (bAsPage != bInAsPage)
	{
		bAsPage = bInAsPage;
		Present();
	}
	Settings = &InSettings;
	bInMatch = bInLiveMatch;
	Close = MoveTemp(InClose);
	if (FVeyraSettingsStore* Store = GetStore())
	{
		// A change from anywhere, Undo or the account's sync included, shows at once.
		ChangedHandle = Store->OnChanged.AddWeakLambda(this, [this](const FVeyraContentId&) { Rebuild(); });
	}
	Display = UVeyraDisplayApplier::Get(this);
	if (UVeyraDisplayApplier* Applier = Display.Get())
	{
		ConfirmationHandle = Applier->OnConfirmationChanged.AddWeakLambda(this, [this] { Rebuild(); });
	}
	bRevealWindow = true;
	Rebuild();
}

void UVeyraSettingsScreen::ShowCategory(EVeyraSettingCategory InCategory)
{
	Category = InCategory;
	Confirming = EConfirming::None;
	Capturing.Reset();
	PendingRebind.Reset();
	if (!Search.IsEmpty())
	{
		// Setting the field's text asks for a rebuild itself.
		SearchBox->SetText(FText::GetEmpty());
	}
	Search.Reset();
	Rebuild();
}

void UVeyraSettingsScreen::SetSearch(const FString& InSearch)
{
	SearchBox->SetText(FText::FromString(InSearch));
	HandleSearchChanged(FText::FromString(InSearch));
}

void UVeyraSettingsScreen::HandleSearchChanged(const FText& Text)
{
	if (Search == Text.ToString())
	{
		return;
	}
	Search = Text.ToString();
	Confirming = EConfirming::None;
	Rebuild();
}

FVeyraSettingsStore* UVeyraSettingsScreen::GetStore() const
{
	UVeyraSettingsSubsystem* Found = Settings.Get();
	return Found && Found->IsReady() ? &Found->GetStore() : nullptr;
}

void UVeyraSettingsScreen::StopListening()
{
	if (FVeyraSettingsStore* Store = GetStore())
	{
		Store->OnChanged.Remove(ChangedHandle);
	}
	ChangedHandle.Reset();
	if (UVeyraDisplayApplier* Applier = Display.Get())
	{
		Applier->OnConfirmationChanged.Remove(ConfirmationHandle);
	}
	ConfirmationHandle.Reset();
}

void UVeyraSettingsScreen::NativeDestruct()
{
	StopListening();
	Super::NativeDestruct();
}

void UVeyraSettingsScreen::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	// Laid out for the width it is given, not the window's (ADR-083 §5).
	if (const float Width = MyGeometry.GetLocalSize().X; Width > 0.0f && LayoutFor(Width) != Layout)
	{
			ApplyWidth(Width);
		return;
	}
	// The kept scroll, set again once the rebuilt list has its rows measured.
	if (PendingScroll > 0.0f && ListScroll && ListScroll->GetCachedGeometry().GetLocalSize().Y > 0.0f)
	{
		ListScroll->SetScrollOffset(PendingScroll);
		PendingScroll = 0.0f;
	}
	// About follows the row under the pointer, or the one keyboard focus is in (ADR-083 §4).
	for (const FRowFrame& Each : RowFrames)
	{
		const UBorder* Frame = Each.Frame.Get();
		if (Frame && Each.Row != AboutRow && (Frame->IsHovered() || Frame->HasFocusedDescendants()))
		{
			AboutRow = Each.Row;
			UpdateAbout();
			break;
		}
	}
	const UVeyraDisplayApplier* Applier = Display.Get();
	if (RevertCountdown && Applier && Applier->IsAwaitingConfirmation())
	{
		RevertCountdown->SetText(FText::Format(LOCTEXT("RevertCountdown", "Keep these display settings? They revert in {0} s."),
			FText::AsNumber(FMath::CeilToInt(Applier->GetSecondsToRevert()))));
	}
}

FReply UVeyraSettingsScreen::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (Capturing.IsSet())
	{
		// Captured, so the key never reaches the game (Settings Bible §6.2).
		CaptureKey(InKeyEvent.GetKey());
		return FReply::Handled();
	}
	// Typing a search over a live match never casts: the letters stay in the field (Settings Bible §6.2).
	if (SearchBox && SearchBox->HasKeyboardFocus() && InKeyEvent.GetKey() != EKeys::Escape)
	{
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply UVeyraSettingsScreen::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (Capturing.IsSet())
	{
		CaptureKey(InMouseEvent.GetEffectingButton());
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UVeyraSettingsScreen::CaptureKey(const FKey& Key)
{
	if (!Capturing.IsSet())
	{
		return;
	}
	const FVeyraContentId Id = Capturing.GetValue();
	Capturing.Reset();
	const FVeyraSettingsStore* Store = GetStore();
	if (!Store || Key == EKeys::Escape || !Key.IsValid())
	{
		Rebuild();
		return;
	}
	if (const TOptional<FVeyraContentId> Other = VeyraSettingsModels::FindConflict(*Store, Id, Key))
	{
		PendingRebind = FPendingRebind{ Id, Key, Other.GetValue() };
		Rebuild();
		return;
	}
	Rebind(Id, Key);
	// A key that was already the binding's changes nothing, so no change event rebuilds.
	Rebuild();
}

void UVeyraSettingsScreen::Rebind(const FVeyraContentId& Id, const FKey& Key)
{
	FVeyraSettingsStore* Store = GetStore();
	const FVeyraBindingSetting* Binding = Store ? Store->GetRegistry().Bindings.Find(Id) : nullptr;
	const UObject* Defaults = Binding ? VeyraSettingsModels::BindingDefaults(*Binding) : nullptr;
	if (!Store || !Defaults)
	{
		return;
	}
	const FStructProperty* Property = VeyraSettings::FindKeyProperty(*Defaults->GetClass(), Binding->Property);
	const bool bDeveloperKey = Property && *Property->ContainerPtrToValuePtr<FKey>(Defaults) == Key;
	Store->Set(Id, bDeveloperKey ? FString() : Key.GetFName().ToString(), bInMatch);
}

void UVeyraSettingsScreen::Change(const FVeyraContentId& Id, const FString& Value)
{
	ChangeStore([this, &Id, &Value](FVeyraSettingsStore& Store) { Store.Set(Id, Value, bInMatch); });
}

void UVeyraSettingsScreen::ChangeStore(TFunctionRef<void(FVeyraSettingsStore&)> Change)
{
	FVeyraSettingsStore* Store = GetStore();
	if (!Store)
	{
		return;
	}
	// A change that could leave the player unable to see waits for Keep (SET-92): the window's size
	// outside a match, or a match's display mode in one, whether set or reset.
	UVeyraDisplayApplier* Applier = Display.Get();
	TArray<TPair<FVeyraContentId, FString>, TInlineAllocator<2>> Disruptive;
	for (const FVeyraContentId& Id : { VeyraDisplayRules::WindowSize(), VeyraDisplayRules::MatchMode() })
	{
		if (Applier && Applier->NeedsConfirmation(Id))
		{
			Disruptive.Emplace(Id, Store->Get(Id));
		}
	}
	Change(*Store);
	for (const TPair<FVeyraContentId, FString>& Before : Disruptive)
	{
		if (Store->Get(Before.Key) != Before.Value)
		{
			Applier->AwaitConfirmation(Before.Key, Before.Value);
		}
	}
}

UVeyraShellButton* UVeyraSettingsScreen::AddButton(UPanelWidget& Parent, EVeyraShellButtonKind Kind, const FText& Label, const FText& Shown, TFunction<void()> Action,
	bool bEnabled, bool bSelected)
{
	UVeyraShellButton* Button = UVeyraShellButton::MakeKindNamed(*WidgetTree, Kind, Label, Shown, MoveTemp(Action), bEnabled, bSelected);
	Button->KeepLabelOnOneLine();
	Buttons.Add(Button);
	VeyraShellStyle::AddSpaced(Parent, *Button);
	return Button;
}

void UVeyraSettingsScreen::Rebuild()
{
	const FVeyraSettingsStore* Store = GetStore();
	if (!Store || !Rows)
	{
		return;
	}
	// The rows are built in the player's look, so a change of text size or transparency shows at once (ADR-055 §2–§3).
	VeyraShellLook::Use(VeyraShellLook::For(VeyraInterfacePreferences::Resolve(*GetDefault<UVeyraGreyboxSettings>(), Store)));
	// The frame built once follows too, not only the rows rebuilt here.
	if (!(FrameLook == VeyraShellLook::Current()))
	{
		StyleFrame();
	}
	const TOptional<FVeyraContentId> Described = GetAboutSetting();
	Model = VeyraSettingsModels::Describe(*Store, Category, Search, bInMatch);
	Category = Model.Category;
	Buttons.Reset();
	// The About panel keeps describing the setting it did, if it is still shown; else the first.
	AboutRow = Model.Rows.IsEmpty() ? INDEX_NONE : 0;
	if (Described.IsSet())
	{
		const int32 Found = Model.Rows.IndexOfByPredicate([&Described](const FVeyraSettingRowModel& Row) { return Row.Id == Described.GetValue(); });
		AboutRow = Found != INDEX_NONE ? Found : AboutRow;
	}

	Actions->ClearChildren();
	AddButton(*Actions, EVeyraShellButtonKind::Secondary, UndoLabel(), UndoLabel(), [this] {
		if (FVeyraSettingsStore* Found = GetStore())
		{
			Found->Undo();
		}
	}, Model.bCanUndo);
	AddButton(*Actions, EVeyraShellButtonKind::Primary, CloseLabel(), CloseLabel(), [this] {
		if (Close)
		{
			Close();
		}
	});
	PlaceSearch();
	BuildBody();
	BuildFooter();
	RevealArrivals();
}

void UVeyraSettingsScreen::PlaceSearch()
{
	if (!SearchHolder || !WideSearchHost || !NarrowSearchHost)
	{
		return;
	}
	const bool bNarrow = Layout == ELayout::Narrow;
	UHorizontalBox* Host = bNarrow ? NarrowSearchHost.Get() : WideSearchHost.Get();
	if (SearchHolder->GetParent() != Host)
	{
		SearchHolder->RemoveFromParent();
		Host->ClearChildren();
		if (!bNarrow)
		{
			// At the right of the room it has, next to the actions.
			VeyraSettingsLayout::AddStretch(*WidgetTree, *Host);
		}
		UHorizontalBoxSlot* SearchSlot = Host->AddChildToHorizontalBox(SearchHolder);
		SearchSlot->SetVerticalAlignment(VAlign_Center);
		SearchSlot->SetPadding(FMargin(0.0f, bNarrow ? VeyraSettingsLayout::Style().Spacing : 0.0f, VeyraSettingsLayout::Style().Spacing, 0.0f));
		if (bNarrow)
		{
			// The whole line's width, as far as it has room.
			SearchSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
	}
	NarrowSearchHost->SetVisibility(bNarrow ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
}

UVeyraSettingsScreen::ELayout UVeyraSettingsScreen::LayoutFor(float Width) const
{
	const UVeyraShellStyleSettings& Style = VeyraSettingsLayout::Style();
	return Width >= Style.SettingsWideWidth ? ELayout::Wide : Width >= Style.SettingsNarrowWidth ? ELayout::Medium : ELayout::Narrow;
}

void UVeyraSettingsScreen::ApplyWidth(float Width)
{
	const ELayout Wanted = LayoutFor(Width);
	if (Wanted != Layout)
	{
		Layout = Wanted;
		Rebuild();
	}
}

TOptional<FVeyraContentId> UVeyraSettingsScreen::GetAboutSetting() const
{
	return Model.Rows.IsValidIndex(AboutRow) ? TOptional<FVeyraContentId>(Model.Rows[AboutRow].Id) : TOptional<FVeyraContentId>();
}

void UVeyraSettingsScreen::AddResetPlace(UHorizontalBox& Line, const FVeyraSettingRowModel& Row)
{
	USizeBox* Place = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Place->SetWidthOverride(VeyraSettingsLayout::Style().SettingsResetWidth);
	if (Row.bChanged && !Row.bLocked)
	{
		// Reset shows only where it would change something (§6.1).
		const FVeyraContentId Id = Row.Id;
		UVeyraShellButton* Reset = UVeyraShellButton::MakeKindNamed(*WidgetTree, EVeyraShellButtonKind::Quiet, ResetLabel(Row), LOCTEXT("ResetOne", "Reset"),
			[this, Id] { ChangeStore([this, &Id](FVeyraSettingsStore& Store) { Store.Reset(Id, bInMatch); }); });
		Reset->KeepLabelOnOneLine();
		Buttons.Add(Reset);
		if (USizeBoxSlot* ResetSlot = Cast<USizeBoxSlot>(Place->AddChild(Reset)))
		{
			ResetSlot->SetHorizontalAlignment(HAlign_Right);
			ResetSlot->SetVerticalAlignment(VAlign_Center);
		}
	}
	Line.AddChildToHorizontalBox(Place)->SetVerticalAlignment(VAlign_Center);
}

UWidget* UVeyraSettingsScreen::GetListWidget() const
{
	return ListScroll.Get();
}

float UVeyraSettingsScreen::GetListScrollOffset() const
{
	return ListScroll ? ListScroll->GetScrollOffset() : 0.0f;
}

void UVeyraSettingsScreen::MakeBodyWidgets(float Scrolled)
{
	Rows = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	ListScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	ListScroll->AddChild(Rows);
	ListScroll->SetScrollOffset(Scrolled);
	PendingScroll = Scrolled;
	Nav = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	NavScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	NavScroll->AddChild(Nav);
	AboutHost = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
}

void UVeyraSettingsScreen::BuildBody()
{
	const UVeyraShellStyleSettings& Style = VeyraSettingsLayout::Style();
	// Every widget of the body is made afresh, none carried over from the last build into new containers: a widget moved
	// out of a released container can come back as its old, detached Slate, which is never drawn and takes no click (ADR-083 §5).
	// The same list rebuilt, as after a click on one of its settings, stays where the player scrolled it; another category or
	// search starts at the top. Read before the old list is let go, which forgets it.
	const bool bSameList = BuiltCategory.IsSet() && BuiltCategory.GetValue() == Model.Category && BuiltSearch == Search;
	const float Scrolled = bSameList ? GetListScrollOffset() : 0.0f;
	BuiltCategory = Model.Category;
	BuiltSearch = Search;
	BodyHost->ClearChildren();
	RowFrames.Reset();
	GroupHeadings.Reset();
	MakeBodyWidgets(Scrolled);

	// The rows first, so the categories' group entries can find their headings.
	if (!Model.UnboundEssentials.IsEmpty() && (Model.bSearching || Model.Category == EVeyraSettingCategory::Controls))
	{
		// The game cannot be played well without them (SET-133).
		UTextBlock* Warning = VeyraSettingsLayout::AddText(*WidgetTree, *Rows,
			FText::Format(LOCTEXT("UnboundEssentials", "No key for {0}. You need these to play."), FText::Join(FText::FromString(TEXT(", ")), Model.UnboundEssentials)),
			VeyraShellStyle::EVeyraShellText::Body);
		Warning->SetColorAndOpacity(FSlateColor(Style.EnemyColor));
	}
	if (Model.bSearching && Model.Rows.IsEmpty())
	{
		VeyraSettingsLayout::AddText(*WidgetTree, *Rows, FText::Format(LOCTEXT("NoResults", "No setting matches \"{0}\"."), FText::FromString(Search)), VeyraShellStyle::EVeyraShellText::Muted);
	}
	if (!Model.bSearching && Model.Rows.IsValidIndex(0) && !Model.Rows[0].Group.IsValid())
	{
		// A category shown as one list still names itself over its rows.
		VeyraSettingsLayout::AddText(*WidgetTree, *Rows, VeyraSettingsModels::CategoryName(Model.Category), VeyraShellStyle::EVeyraShellText::Eyebrow);
	}
	// Consecutive rows of one group, or while searching of one category, under its heading (ADR-083 §2).
	for (int32 First = 0; First < Model.Rows.Num();)
	{
		const FVeyraSettingRowModel& Lead = Model.Rows[First];
		int32 Last = First + 1;
		while (Last < Model.Rows.Num() && (Model.bSearching ? Model.Rows[Last].Category == Lead.Category : Model.Rows[Last].Group == Lead.Group))
		{
			++Last;
		}
		const bool bNamed = Model.bSearching || Lead.Group.IsValid();
		BuildGroup(bNamed ? (Model.bSearching ? VeyraSettingsModels::CategoryName(Lead.Category) : VeyraContentText::SettingGroupName(Lead.Group)) : FText::GetEmpty(),
			bNamed && !Model.bSearching ? VeyraContentText::SettingGroupHint(Lead.Group) : FText::GetEmpty(), First, Last);
		First = Last;
	}

	if (Layout == ELayout::Narrow)
	{
		// Narrow, the categories run across the top, and About sits under the list.
		UWrapBox* Across = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass());
		BuildNav(*Across);
		VeyraShellStyle::AddSpaced(*BodyHost, *Across);
		VeyraSettingsLayout::AddFilling(*BodyHost, *ListScroll);
		VeyraShellStyle::AddSpaced(*BodyHost, *AboutHost);
	}
	else
	{
		UHorizontalBox* Columns = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		BuildNav(*Nav);
		UHorizontalBoxSlot* NavSlot = Columns->AddChildToHorizontalBox(VeyraSettingsLayout::Sized(*WidgetTree, *NavScroll, Style.SettingsNavWidth));
		NavSlot->SetPadding(FMargin(0.0f, 0.0f, Style.Spacing * 2.0f, 0.0f));
		if (Layout == ELayout::Wide)
		{
			// Wide, About beside the list.
			Columns->AddChildToHorizontalBox(ListScroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			UHorizontalBoxSlot* AboutSlot = Columns->AddChildToHorizontalBox(VeyraSettingsLayout::Sized(*WidgetTree, *AboutHost, Style.SettingsAboutWidth));
			AboutSlot->SetPadding(FMargin(Style.Spacing * 2.0f, 0.0f, 0.0f, 0.0f));
			AboutSlot->SetVerticalAlignment(VAlign_Top);
		}
		else
		{
			// Medium, About a strip under the list.
			UVerticalBox* Center = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
			VeyraSettingsLayout::AddFilling(*Center, *ListScroll);
			VeyraShellStyle::AddSpaced(*Center, *AboutHost);
			Columns->AddChildToHorizontalBox(Center)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
		VeyraSettingsLayout::AddFilling(*BodyHost, *Columns);
	}
	UpdateAbout();
}

void UVeyraSettingsScreen::BuildNav(UPanelWidget& Parent)
{
	for (const EVeyraSettingCategory Shown : Model.Categories)
	{
		const FText Name = VeyraSettingsModels::CategoryName(Shown);
		const bool bShown = !Model.bSearching && Shown == Model.Category;
		AddButton(Parent, EVeyraShellButtonKind::Tab, Name, Name, [this, Shown] { ShowCategory(Shown); }, true, bShown);
		if (!bShown || Layout == ELayout::Narrow)
		{
			continue;
		}
		// The shown category's groups under it, each scrolling the list to its heading (ADR-083 §2).
		for (const TPair<FText, TWeakObjectPtr<UWidget>>& Heading : GroupHeadings)
		{
			TWeakObjectPtr<UWidget> Target = Heading.Value;
			UVeyraShellButton* Link = AddButton(Parent, EVeyraShellButtonKind::Quiet, GroupLinkLabel(Heading.Key), Heading.Key, [this, Target] {
				if (UWidget* Found = Target.Get())
				{
					ListScroll->ScrollWidgetIntoView(Found, true, EDescendantScrollDestination::TopOrLeft);
				}
			});
			if (UVerticalBoxSlot* LinkSlot = Cast<UVerticalBoxSlot>(Link->Slot))
			{
				LinkSlot->SetPadding(FMargin(VeyraSettingsLayout::Style().Spacing * 1.5f, 0.0f, 0.0f, 0.0f));
			}
		}
	}
}

bool UVeyraSettingsScreen::IsGrid(int32 First, int32 Last) const
{
	// Choices sharing one set of options read best as a grid: settings down, options across (ADR-083 §3).
	if (Model.bSearching || Last - First < VeyraSettingsLayout::Style().SettingsGridLeast)
	{
		return false;
	}
	const FVeyraSettingRowModel& Lead = Model.Rows[First];
	for (int32 Index = First; Index < Last; ++Index)
	{
		const FVeyraSettingRowModel& Row = Model.Rows[Index];
		if (Row.Kind != EVeyraSettingKind::Choice || Row.Options.Num() != Lead.Options.Num())
		{
			return false;
		}
		for (int32 Option = 0; Option < Row.Options.Num(); ++Option)
		{
			if (Row.Options[Option].Value != Lead.Options[Option].Value)
			{
				return false;
			}
		}
	}
	return true;
}

void UVeyraSettingsScreen::BuildGroup(const FText& Heading, const FText& Hint, int32 First, int32 Last)
{
	const UVeyraShellStyleSettings& Style = VeyraSettingsLayout::Style();
	UVerticalBox* Group = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	if (!Heading.IsEmpty())
	{
		UHorizontalBox* Head = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UTextBlock* Name = VeyraShellStyle::MakeText(*WidgetTree, Heading, VeyraShellStyle::EVeyraShellText::Eyebrow);
		Name->SetAutoWrapText(false);
		VeyraShellStyle::AddSpaced(*Head, *Name);
		if (!Hint.IsEmpty())
		{
			UTextBlock* Said = VeyraShellStyle::MakeText(*WidgetTree, Hint, VeyraShellStyle::EVeyraShellText::Muted);
			Said->SetAutoWrapText(false);
			VeyraShellStyle::AddSpaced(*Head, *Said);
		}
		Group->AddChildToVerticalBox(Head)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, Style.Spacing / 2.0f));
		GroupHeadings.Emplace(Heading, Head);
	}
	UBorder* Box = VeyraShellStyle::MakeSurface(*WidgetTree, VeyraShellStyle::EVeyraShellSurface::Panel, FMargin(0.0f));
	UVerticalBox* Lines = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Box->SetContent(Lines);
	if (IsGrid(First, Last))
	{
		BuildGrid(*Lines, First, Last);
	}
	else
	{
		for (int32 Index = First; Index < Last; ++Index)
		{
			if (Index > First)
			{
				Lines->AddChild(VeyraShellStyle::MakeRule(*WidgetTree));
			}
			BuildRow(*Lines, Index);
		}
	}
	Group->AddChild(Box);
	Rows->AddChildToVerticalBox(Group)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, Style.Spacing * 2.0f));
}

UBorder* UVeyraSettingsScreen::MakeRowFrame(int32 Index, UWidget& Content)
{
	const UVeyraShellStyleSettings& Style = VeyraSettingsLayout::Style();
	UBorder* Frame = VeyraShellStyle::MakeBorder(*WidgetTree, FLinearColor::Transparent, 0.0f);
	Frame->SetPadding(FMargin(Style.Spacing * 1.5f, Style.Spacing / 3.0f));
	// Hit-testable, so the About panel can follow the pointer into it.
	Frame->SetVisibility(ESlateVisibility::Visible);
	USizeBox* Height = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Height->SetMinDesiredHeight(Style.SettingsRowHeight);
	if (USizeBoxSlot* ContentSlot = Cast<USizeBoxSlot>(Height->AddChild(&Content)))
	{
		ContentSlot->SetVerticalAlignment(VAlign_Center);
	}
	Frame->SetContent(Height);
	RowFrames.Add({ Frame, Index });
	return Frame;
}

void UVeyraSettingsScreen::BuildGrid(UPanelWidget& Box, int32 First, int32 Last)
{
	const UVeyraShellStyleSettings& Style = VeyraSettingsLayout::Style();
	// The options across the top, each a column of radios.
	UHorizontalBox* Head = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	VeyraSettingsLayout::AddStretch(*WidgetTree, *Head);
	for (const FVeyraSettingOptionModel& Option : Model.Rows[First].Options)
	{
		UTextBlock* Name = VeyraShellStyle::MakeText(*WidgetTree, Option.Label, VeyraShellStyle::EVeyraShellText::Column);
		Name->SetJustification(ETextJustify::Center);
		Head->AddChildToHorizontalBox(VeyraSettingsLayout::Sized(*WidgetTree, *Name, Style.SettingsGridColumnWidth))->SetVerticalAlignment(VAlign_Center);
	}
	// The rows' place for Reset, so each column of radios sits under its option's name.
	Head->AddChildToHorizontalBox(VeyraSettingsLayout::Sized(*WidgetTree, *WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass()), Style.SettingsResetWidth));
	UBorder* HeadFrame = VeyraShellStyle::MakeBorder(*WidgetTree, FLinearColor::Transparent, 0.0f);
	HeadFrame->SetPadding(FMargin(Style.Spacing * 1.5f, Style.Spacing));
	HeadFrame->SetContent(Head);
	Box.AddChild(HeadFrame);
	for (int32 Index = First; Index < Last; ++Index)
	{
		const FVeyraSettingRowModel& Row = Model.Rows[Index];
		Box.AddChild(VeyraShellStyle::MakeRule(*WidgetTree));
		UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UHorizontalBox* Who = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		// The name wraps within what the radios leave it, never under them (ADR-083 §5).
		UTextBlock* Name = VeyraShellStyle::MakeText(*WidgetTree, Row.Name, VeyraShellStyle::EVeyraShellText::Body);
		UHorizontalBoxSlot* NameSlot = Who->AddChildToHorizontalBox(Name);
		NameSlot->SetVerticalAlignment(VAlign_Center);
		NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		if (Row.bChanged)
		{
			UHorizontalBoxSlot* Dot = Who->AddChildToHorizontalBox(VeyraShellStyle::MakeDot(*WidgetTree, Style.PrimaryColor, Style.Spacing / 2.0f));
			Dot->SetVerticalAlignment(VAlign_Center);
			Dot->SetPadding(FMargin(Style.Spacing / 2.0f, 0.0f, 0.0f, 0.0f));
		}
		VeyraSettingsLayout::AddFilling(*Line, *Who);
		const FVeyraContentId Id = Row.Id;
		for (const FVeyraSettingOptionModel& Option : Row.Options)
		{
			UVeyraShellButton* Radio = UVeyraShellButton::MakeWithContent(*WidgetTree, OptionLabel(Row, Option),
				*VeyraSettingsLayout::RadioFace(*WidgetTree, Option.bSelected), [this, Id, Value = Option.Value] { Change(Id, Value); }, !Row.bLocked, Option.bSelected);
			Radio->SetStyle(VeyraSettingsLayout::Bare());
			Buttons.Add(Radio);
			USizeBox* Column = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			Column->SetWidthOverride(Style.SettingsGridColumnWidth);
			if (USizeBoxSlot* RadioSlot = Cast<USizeBoxSlot>(Column->AddChild(Radio)))
			{
				RadioSlot->SetHorizontalAlignment(HAlign_Center);
			}
			Line->AddChildToHorizontalBox(Column)->SetVerticalAlignment(VAlign_Center);
		}
		AddResetPlace(*Line, Row);
		Box.AddChild(MakeRowFrame(Index, *Line));
	}
}

void UVeyraSettingsScreen::BuildRow(UPanelWidget& Box, int32 Index)
{
	const UVeyraShellStyleSettings& Style = VeyraSettingsLayout::Style();
	const FVeyraSettingRowModel& Row = Model.Rows[Index];
	UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Stack->AddChild(Line);
	// Its name, and the brass mark of a value changed from its default.
	UHorizontalBox* Who = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UTextBlock* Name = VeyraShellStyle::MakeText(*WidgetTree, Row.Name, VeyraShellStyle::EVeyraShellText::Body);
	Name->SetAutoWrapText(false);
	Who->AddChildToHorizontalBox(Name)->SetVerticalAlignment(VAlign_Center);
	if (Row.bChanged)
	{
		UHorizontalBoxSlot* Dot = Who->AddChildToHorizontalBox(VeyraShellStyle::MakeDot(*WidgetTree, Style.PrimaryColor, Style.Spacing / 2.0f));
		Dot->SetVerticalAlignment(VAlign_Center);
		Dot->SetPadding(FMargin(Style.Spacing / 2.0f, 0.0f, 0.0f, 0.0f));
	}
	VeyraSettingsLayout::AddFilling(*Line, *Who);
	BuildControl(*Line, *Stack, Row);
	// Reset shows only where it would change something (§6.1).
	const FVeyraContentId Id = Row.Id;
	AddResetPlace(*Line, Row);
	Box.AddChild(MakeRowFrame(Index, *Stack));
}

void UVeyraSettingsScreen::BuildControl(UPanelWidget& Line, UPanelWidget& Under, const FVeyraSettingRowModel& Row)
{
	const UVeyraShellStyleSettings& Style = VeyraSettingsLayout::Style();
	const FVeyraContentId Id = Row.Id;
	const bool bEnabled = !Row.bLocked;
	if (Row.Kind == EVeyraSettingKind::Binding)
	{
		// A key cap, which waits for its new key when pressed.
		if (Capturing.IsSet() && Capturing.GetValue() == Id)
		{
			UTextBlock* Hint = VeyraShellStyle::MakeText(*WidgetTree, LOCTEXT("PressAKey", "Press a key or a mouse button. Escape cancels."), VeyraShellStyle::EVeyraShellText::Body);
			Hint->SetColorAndOpacity(FSlateColor(Style.AccentColor));
			Hint->SetAutoWrapText(false);
			VeyraShellStyle::AddSpaced(Line, *Hint);
			return;
		}
		AddButton(Line, EVeyraShellButtonKind::Secondary, ChangeLabel(Row), Row.ValueText, [this, Id] {
			Capturing = Id;
			PendingRebind.Reset();
			Confirming = EConfirming::None;
			Rebuild();
			// The next key comes here, not to the game.
			SetKeyboardFocus();
		}, bEnabled);
		return;
	}
	if (Row.Kind == EVeyraSettingKind::Range)
	{
		// A slider with its value, between its steps; it sets the value when the player lets go (ADR-083 §3).
		AddButton(Line, EVeyraShellButtonKind::Quiet, StepLabel(Row, false), LOCTEXT("Lower", "‹"), [this, Id, Value = Row.Lower.Get(FString())] { Change(Id, Value); },
			bEnabled && Row.Lower.IsSet());
		UVeyraSettingsSlider* Slider = WidgetTree->ConstructWidget<UVeyraSettingsSlider>(UVeyraSettingsSlider::StaticClass());
		Slider->SetValue(Row.RangeFraction);
		const double Span = Row.RangeMaximum - Row.RangeMinimum;
		// One of the setting's own steps a press; a range with no span is one step from end to end.
		Slider->SetStepSize(Span > 0.0 ? static_cast<float>(Row.RangeStep / Span) : 1.0f);
		Slider->SetWidgetStyle(VeyraSettingsLayout::SliderStyle());
		Slider->SetIsEnabled(bEnabled);
		UTextBlock* Value = VeyraShellStyle::MakeText(*WidgetTree, Row.ValueText, VeyraShellStyle::EVeyraShellText::Heading);
		Value->SetAutoWrapText(false);
		Value->SetJustification(ETextJustify::Right);
		const double Minimum = Row.RangeMinimum;
		const double Step = Row.RangeStep;
		const auto ValueAt = [Minimum, Span, Step](float Fraction) {
			const double Raw = Minimum + Span * FMath::Clamp(Fraction, 0.0f, 1.0f);
			return Step > 0.0 ? Minimum + FMath::RoundToDouble((Raw - Minimum) / Step) * Step : Raw;
		};
		TWeakObjectPtr<UTextBlock> Shown = Value;
		Slider->Bind(
			[Shown, ValueAt](float Fraction) {
				if (UTextBlock* Text = Shown.Get())
				{
					FNumberFormattingOptions Format;
					Format.SetUseGrouping(false).SetMaximumFractionalDigits(2);
					Text->SetText(FText::AsNumber(ValueAt(Fraction), &Format));
				}
			},
			[this, Id, ValueAt](float Fraction) { Change(Id, VeyraSettings::NumberText(ValueAt(Fraction))); });
		Line.AddChild(VeyraSettingsLayout::Sized(*WidgetTree, *Slider, Style.SettingsSliderWidth));
		if (UHorizontalBoxSlot* SliderSlot = Cast<UHorizontalBoxSlot>(VeyraSettingsLayout::LastSlot(Line)))
		{
			SliderSlot->SetVerticalAlignment(VAlign_Center);
			SliderSlot->SetPadding(FMargin(Style.Spacing / 2.0f, 0.0f));
		}
		Line.AddChild(VeyraSettingsLayout::Sized(*WidgetTree, *Value, Style.Spacing * 4.0f));
		if (UHorizontalBoxSlot* ValueSlot = Cast<UHorizontalBoxSlot>(VeyraSettingsLayout::LastSlot(Line)))
		{
			ValueSlot->SetVerticalAlignment(VAlign_Center);
		}
		AddButton(Line, EVeyraShellButtonKind::Quiet, StepLabel(Row, true), LOCTEXT("Higher", "›"), [this, Id, Value = Row.Higher.Get(FString())] { Change(Id, Value); },
			bEnabled && Row.Higher.IsSet());
		return;
	}
	if (Row.Kind == EVeyraSettingKind::Toggle)
	{
		// A switch: pressed, it takes the value it does not have; it is named for that value.
		const bool bOn = Row.Options.IsValidIndex(0) && Row.Options[0].bSelected;
		const FVeyraSettingOptionModel& Target = Row.Options[bOn ? 1 : 0];
		UVeyraShellButton* Switch = UVeyraShellButton::MakeWithContent(*WidgetTree, OptionLabel(Row, Target), *VeyraSettingsLayout::SwitchFace(*WidgetTree, bOn),
			[this, Id, Value = Target.Value] { Change(Id, Value); }, bEnabled, bOn);
		Switch->SetStyle(VeyraSettingsLayout::Bare());
		Buttons.Add(Switch);
		VeyraShellStyle::AddSpaced(Line, *Switch);
		return;
	}
	// A choice: side by side when its options fit, else a dropdown (ADR-083 §3).
	int32 Letters = 0;
	for (const FVeyraSettingOptionModel& Option : Row.Options)
	{
		Letters += Option.Label.ToString().Len();
	}
	if (Row.Options.Num() <= Style.SettingsSegmentMost && Letters <= Style.SettingsSegmentLetters)
	{
		UBorder* Track = VeyraShellStyle::MakeBorder(*WidgetTree, FLinearColor::Transparent, 1.0f);
		Track->SetBrush(VeyraShellStyle::EdgedFace(Style.BackgroundColor, Style.ButtonEdgeColor));
		UHorizontalBox* Segments = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Track->SetContent(Segments);
		for (const FVeyraSettingOptionModel& Option : Row.Options)
		{
			UVeyraShellButton* Segment = UVeyraShellButton::MakeKindNamed(*WidgetTree, EVeyraShellButtonKind::Tab, OptionLabel(Row, Option), Option.Label,
				[this, Id, Value = Option.Value] { Change(Id, Value); }, bEnabled, Option.bSelected);
			Segment->KeepLabelOnOneLine();
			Segment->SetStyle(VeyraSettingsLayout::SegmentStyle(Option.bSelected));
			if (UTextBlock* Text = Cast<UTextBlock>(Segment->GetContent()))
			{
				Text->SetColorAndOpacity(FSlateColor(Option.bSelected ? Style.BackgroundColor : Style.MutedTextColor));
			}
			Buttons.Add(Segment);
			Segments->AddChildToHorizontalBox(Segment);
		}
		if (Layout == ELayout::Narrow)
		{
			// Narrow, the segments take a line of their own under the name rather than crowd it (ADR-083 §5).
			if (UVerticalBox* Stack = Cast<UVerticalBox>(&Under))
			{
				Stack->AddChildToVerticalBox(Track)->SetPadding(FMargin(0.0f, Style.Spacing / 2.0f, 0.0f, 0.0f));
				return;
			}
		}
		VeyraShellStyle::AddSpaced(Line, *Track);
		return;
	}
	// The dropdown shows the value, with a colour setting's swatches; its options open under the row.
	UHorizontalBox* Head = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	for (const FLinearColor& Color : Row.Swatches)
	{
		UBorder* Swatch = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Swatch->SetBrush(VeyraShellStyle::EdgedFace(Color, Style.HairlineColor));
		Head->AddChildToHorizontalBox(VeyraSettingsLayout::Square(*WidgetTree, *Swatch, Style.SettingsSwatchSize))->SetPadding(FMargin(0.0f, 0.0f, Style.Spacing / 2.0f, 0.0f));
	}
	UTextBlock* Value = VeyraShellStyle::MakeText(*WidgetTree, Row.ValueText, VeyraShellStyle::EVeyraShellText::Body);
	Value->SetAutoWrapText(false);
	Head->AddChildToHorizontalBox(Value)->SetVerticalAlignment(VAlign_Center);
	UTextBlock* Chevron = VeyraShellStyle::MakeText(*WidgetTree, LOCTEXT("DropdownChevron", "›"), VeyraShellStyle::EVeyraShellText::Muted);
	Chevron->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	Chevron->SetRenderTransformAngle(90.0f);
	UHorizontalBoxSlot* ChevronSlot = Head->AddChildToHorizontalBox(Chevron);
	ChevronSlot->SetVerticalAlignment(VAlign_Center);
	ChevronSlot->SetPadding(FMargin(Style.Spacing, 0.0f, 0.0f, 0.0f));
	const bool bOpen = OpenDropdown.IsSet() && OpenDropdown.GetValue() == Id;
	UVeyraShellButton* Dropdown = UVeyraShellButton::MakeWithContent(*WidgetTree, DropdownLabel(Row), *Head, [this, Id, bOpen] {
		OpenDropdown = bOpen ? TOptional<FVeyraContentId>() : TOptional<FVeyraContentId>(Id);
		Rebuild();
	}, bEnabled, bOpen);
	Buttons.Add(Dropdown);
	VeyraShellStyle::AddSpaced(Line, *Dropdown);
	// Its options: built always, so each keeps its name, and shown while the dropdown is open.
	UWrapBox* Options = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass());
	for (const FVeyraSettingOptionModel& Option : Row.Options)
	{
		UVeyraShellButton* Pick = AddButton(*Options, EVeyraShellButtonKind::Tab, OptionLabel(Row, Option), Option.Label, [this, Id, Value = Option.Value] {
			OpenDropdown.Reset();
			Change(Id, Value);
			Rebuild();
		}, bEnabled, Option.bSelected);
		Pick->KeepLabelOnOneLine();
	}
	Options->SetVisibility(bOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	Under.AddChild(Options);
}

void UVeyraSettingsScreen::UpdateAbout()
{
	const UVeyraShellStyleSettings& Style = VeyraSettingsLayout::Style();
	// The described row stands out from the others.
	for (const FRowFrame& Each : RowFrames)
	{
		if (UBorder* Frame = Each.Frame.Get())
		{
			Frame->SetBrushColor(Each.Row == AboutRow ? Style.SelectedColor : FLinearColor::Transparent);
		}
	}
	AboutHost->ClearChildren();
	if (!Model.Rows.IsValidIndex(AboutRow))
	{
		return;
	}
	const FVeyraSettingRowModel& Row = Model.Rows[AboutRow];
	UBorder* Panel = VeyraShellStyle::MakeSurface(*WidgetTree, VeyraShellStyle::EVeyraShellSurface::Raised, FMargin(Style.Spacing * 1.5f));
	const bool bBeside = Layout == ELayout::Wide;
	UPanelWidget* Lines = bBeside ? static_cast<UPanelWidget*>(WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass()))
								  : static_cast<UPanelWidget*>(WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass()));
	Panel->SetContent(Lines);
	if (bBeside)
	{
		UTextBlock* Eyebrow = VeyraSettingsLayout::AddText(*WidgetTree, *Lines, LOCTEXT("AboutEyebrow", "About"), VeyraShellStyle::EVeyraShellText::Eyebrow);
		Eyebrow->SetColorAndOpacity(FSlateColor(Style.PrimaryColor));
	}
	UTextBlock* Name = VeyraSettingsLayout::AddText(*WidgetTree, *Lines, Row.Name, VeyraShellStyle::EVeyraShellText::Heading);
	Name->SetAutoWrapText(bBeside);
	// What it does, and what to know before changing it.
	TArray<FText> Notes;
	if (!Row.Description.IsEmpty())
	{
		Notes.Add(Row.Description);
	}
	if (Row.bAfterRestart)
	{
		Notes.Add(LOCTEXT("AfterRestart", "Takes effect after a restart."));
	}
	if (Row.bLocked)
	{
		Notes.Add(LOCTEXT("OutsideMatches", "Changes outside matches only."));
	}
	const FText Said = FText::Join(FText::FromString(TEXT(" ")), Notes);
	if (!Said.IsEmpty())
	{
		UTextBlock* Text = VeyraShellStyle::MakeText(*WidgetTree, Said, VeyraShellStyle::EVeyraShellText::Muted);
		if (bBeside)
		{
			VeyraShellStyle::AddSpaced(*Lines, *Text);
		}
		else
		{
			VeyraSettingsLayout::AddFilling(*Lines, *Text);
		}
	}
	if (!Row.DefaultText.IsEmpty())
	{
		UTextBlock* Default = VeyraShellStyle::MakeText(*WidgetTree, FText::Format(LOCTEXT("AboutDefault", "Default: {0}"), Row.DefaultText), VeyraShellStyle::EVeyraShellText::Small);
		Default->SetAutoWrapText(false);
		Default->SetColorAndOpacity(FSlateColor(Style.MutedTextColor));
		VeyraShellStyle::AddSpaced(*Lines, *Default);
	}
	AboutHost->AddChild(Panel);
}

void UVeyraSettingsScreen::RevealArrivals()
{
	// Opening, the window's parts rise in; a new category or search brings its rows in, one after another. A rebuild for a
	// change carries a reveal under way over to the parts it made anew (ADR-082 §9).
	const bool bNewRows = !RevealedCategory.IsSet() || RevealedCategory.GetValue() != Model.Category || RevealedSearch != Search;
	if (bRevealWindow)
	{
		bRevealWindow = false;
		bRevealingRows = false;
		Reveal.Play(WindowFrame->GetAllChildren());
	}
	else if (bNewRows)
	{
		bRevealingRows = true;
		Reveal.Play(Rows->GetAllChildren());
	}
	else
	{
		Reveal.Continue(bRevealingRows ? Rows->GetAllChildren() : WindowFrame->GetAllChildren());
	}
	RevealedCategory = Model.Category;
	RevealedSearch = Search;
}

void UVeyraSettingsScreen::BuildFooter()
{
	Footer->ClearChildren();
	RevertCountdown = nullptr;
	if (UVeyraDisplayApplier* Applier = Display.Get(); Applier && Applier->IsAwaitingConfirmation())
	{
		// Before anything else: the display the player sees may not work for them (SET-92).
		RevertCountdown = VeyraShellStyle::MakeText(*WidgetTree, FText::GetEmpty(), VeyraShellStyle::EVeyraShellText::Body);
		VeyraSettingsLayout::AddFilling(*Footer, *RevertCountdown);
		AddButton(*Footer, EVeyraShellButtonKind::Primary, KeepChangesLabel(), KeepChangesLabel(), [this] {
			if (UVeyraDisplayApplier* Found = Display.Get())
			{
				Found->KeepChange();
			}
		});
		AddButton(*Footer, EVeyraShellButtonKind::Secondary, RevertLabel(), RevertLabel(), [this] {
			if (UVeyraDisplayApplier* Found = Display.Get())
			{
				Found->RevertChange();
			}
		});
		return;
	}
	if (PendingRebind.IsSet())
	{
		const FPendingRebind& Pending = PendingRebind.GetValue();
		VeyraSettingsLayout::AddFilling(*Footer, *VeyraShellStyle::MakeText(*WidgetTree,
			FText::Format(LOCTEXT("KeyTaken", "{0} is already {1}. Put it on {2} instead, leaving {1} without a key?"), Pending.Key.GetDisplayName(),
				VeyraContentText::SettingName(Pending.Other), VeyraContentText::SettingName(Pending.Id)),
			VeyraShellStyle::EVeyraShellText::Body));
		AddButton(*Footer, EVeyraShellButtonKind::Primary, ReplaceLabel(), ReplaceLabel(), [this] {
			const FPendingRebind Replacing = PendingRebind.GetValue();
			PendingRebind.Reset();
			if (FVeyraSettingsStore* Store = GetStore())
			{
				Store->Set(Replacing.Other, VeyraSettings::Unbound(), bInMatch);
			}
			Rebind(Replacing.Id, Replacing.Key);
			Rebuild();
		});
		AddButton(*Footer, EVeyraShellButtonKind::Quiet, CancelLabel(), CancelLabel(), [this] {
			PendingRebind.Reset();
			Rebuild();
		});
		return;
	}
	if (Confirming != EConfirming::None)
	{
		// Resetting a category or everything asks first (§6.1); Undo does not take them back.
		const bool bAll = Confirming == EConfirming::All;
		const FText Question = bAll ? LOCTEXT("ConfirmAll", "Return every setting to its default? Undo cannot take this back.")
									: FText::Format(LOCTEXT("ConfirmCategory", "Return every {0} setting to its default? Undo cannot take this back."),
										  VeyraSettingsModels::CategoryName(Model.Category));
		VeyraSettingsLayout::AddFilling(*Footer, *VeyraShellStyle::MakeText(*WidgetTree, Question, VeyraShellStyle::EVeyraShellText::Body));
		AddButton(*Footer, EVeyraShellButtonKind::Primary, ConfirmResetLabel(), ConfirmResetLabel(), [this, bAll] {
			Confirming = EConfirming::None;
			ChangeStore([this, bAll](FVeyraSettingsStore& Store) {
				if (bAll)
				{
					Store.ResetAll(bInMatch);
				}
				else
				{
					Store.ResetCategory(Model.Category, bInMatch);
				}
			});
			// A reset that changed nothing sends no change to rebuild on.
			Rebuild();
		});
		AddButton(*Footer, EVeyraShellButtonKind::Quiet, CancelLabel(), CancelLabel(), [this] {
			Confirming = EConfirming::None;
			Rebuild();
		});
		return;
	}
	AddButton(*Footer, EVeyraShellButtonKind::Secondary, ResetCategoryLabel(),
		FText::Format(LOCTEXT("ResetCategoryShown", "Reset {0}"), VeyraSettingsModels::CategoryName(Model.Category)), [this] {
			Confirming = EConfirming::Category;
			Rebuild();
		}, !Model.bSearching && Model.bCategoryChanged);
	AddButton(*Footer, EVeyraShellButtonKind::Quiet, ResetAllLabel(), ResetAllLabel(), [this] {
		Confirming = EConfirming::All;
		Rebuild();
	}, Model.bAnyChanged);
	// Nothing to save: every change is kept as it is made (§13). It takes the room the resets leave, at their right,
	// and wraps where that is short.
	UTextBlock* Autosave = VeyraShellStyle::MakeText(*WidgetTree, LOCTEXT("Autosave", "Changes are saved as you make them."), VeyraShellStyle::EVeyraShellText::Muted);
	Autosave->SetJustification(ETextJustify::Right);
	VeyraSettingsLayout::AddFilling(*Footer, *Autosave);
	if (UHorizontalBoxSlot* AutosaveSlot = Cast<UHorizontalBoxSlot>(Autosave->Slot))
	{
		AutosaveSlot->SetVerticalAlignment(VAlign_Center);
	}
}

TArray<UVeyraShellButton*> UVeyraSettingsScreen::GetButtons() const
{
	TArray<UVeyraShellButton*> Found;
	for (const TObjectPtr<UVeyraShellButton>& Button : Buttons)
	{
		Found.Add(Button.Get());
	}
	return Found;
}

UVeyraShellButton* UVeyraSettingsScreen::FindButton(const FText& Label) const
{
	for (const TObjectPtr<UVeyraShellButton>& Button : Buttons)
	{
		if (Button->GetLabel().EqualTo(Label))
		{
			return Button.Get();
		}
	}
	return nullptr;
}

FString UVeyraSettingsScreen::DescribeText() const
{
	TArray<FString> Lines;
	if (WidgetTree)
	{
		WidgetTree->ForEachWidget([&Lines](UWidget* Widget) {
			if (const UTextBlock* Block = Cast<UTextBlock>(Widget))
			{
				Lines.Add(Block->GetText().ToString());
			}
		});
	}
	return FString::Join(Lines, TEXT("\n"));
}

FText UVeyraSettingsScreen::OptionLabel(const FVeyraSettingRowModel& Row, const FVeyraSettingOptionModel& Option)
{
	return FText::Format(LOCTEXT("OptionLabel", "{0}: {1}"), Row.Name, Option.Label);
}

FText UVeyraSettingsScreen::StepLabel(const FVeyraSettingRowModel& Row, bool bHigher)
{
	return FText::Format(bHigher ? LOCTEXT("HigherLabel", "{0}: Higher") : LOCTEXT("LowerLabel", "{0}: Lower"), Row.Name);
}

FText UVeyraSettingsScreen::ResetLabel(const FVeyraSettingRowModel& Row)
{
	return FText::Format(LOCTEXT("ResetLabel", "Reset {0}"), Row.Name);
}

FText UVeyraSettingsScreen::CloseLabel()
{
	return LOCTEXT("Close", "Close");
}

FText UVeyraSettingsScreen::UndoLabel()
{
	return LOCTEXT("Undo", "Undo");
}

FText UVeyraSettingsScreen::ResetCategoryLabel()
{
	return LOCTEXT("ResetCategory", "Reset Category");
}

FText UVeyraSettingsScreen::ResetAllLabel()
{
	return LOCTEXT("ResetAll", "Reset All");
}

FText UVeyraSettingsScreen::ConfirmResetLabel()
{
	return LOCTEXT("ConfirmReset", "Reset");
}

FText UVeyraSettingsScreen::CancelLabel()
{
	return LOCTEXT("Cancel", "Cancel");
}

FText UVeyraSettingsScreen::ChangeLabel(const FVeyraSettingRowModel& Row)
{
	return FText::Format(LOCTEXT("ChangeLabel", "{0}: Change"), Row.Name);
}

FText UVeyraSettingsScreen::DropdownLabel(const FVeyraSettingRowModel& Row)
{
	return FText::Format(LOCTEXT("DropdownLabel", "{0}: Options"), Row.Name);
}

FText UVeyraSettingsScreen::GroupLinkLabel(const FText& GroupName)
{
	return FText::Format(LOCTEXT("GroupLinkLabel", "Go to {0}"), GroupName);
}

FText UVeyraSettingsScreen::ReplaceLabel()
{
	return LOCTEXT("Replace", "Replace");
}

FText UVeyraSettingsScreen::KeepChangesLabel()
{
	return LOCTEXT("KeepChanges", "Keep Changes");
}

FText UVeyraSettingsScreen::RevertLabel()
{
	return LOCTEXT("Revert", "Revert");
}

#undef LOCTEXT_NAMESPACE
