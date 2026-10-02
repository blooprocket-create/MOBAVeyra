// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Loading/VeyraLoadingScreen.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CircularThrobber.h"
#include "Components/HorizontalBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Greybox/VeyraGreyboxSettings.h"
#include "HAL/PlatformTime.h"
#include "InputCoreTypes.h"
#include "Settings/VeyraInterfacePreferences.h"
#include "Shell/VeyraShellButton.h"
#include "Shell/VeyraShellStyle.h"
#include "Shell/VeyraShellStyleSettings.h"
#include "Text/VeyraContentText.h"
#include "VeyraSettingsStore.h"

#define LOCTEXT_NAMESPACE "VeyraLoadingScreen"

bool UVeyraLoadingScreen::Initialize()
{
	const bool bFirst = Super::Initialize();
	if (!bFirst || !WidgetTree || WidgetTree->RootWidget)
	{
		return bFirst;
	}
	// The battleground waits behind an opaque backdrop: nothing under it is for playing yet.
	const UVeyraShellStyleSettings& Style = *GetDefault<UVeyraShellStyleSettings>();
	UBorder* Backdrop = VeyraShellStyle::MakeBorder(*WidgetTree, Style.BackgroundColor, Style.ScreenPadding);
	Backdrop->SetHorizontalAlignment(HAlign_Center);
	Backdrop->SetVerticalAlignment(VAlign_Center);
	USizeBox* Width = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Width->SetWidthOverride(Style.LoadingPanelWidth);
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());

	// The stage in plain words, over an indicator that only says work goes on (SET-114).
	StageLabel = VeyraShellStyle::MakeText(*WidgetTree, StageText(Stage), VeyraShellStyle::EVeyraShellText::Display);
	StageLabel->SetJustification(ETextJustify::Center);
	VeyraShellStyle::AddSpaced(*Column, *StageLabel);
	UCircularThrobber* Activity = WidgetTree->ConstructWidget<UCircularThrobber>(UCircularThrobber::StaticClass());
	VeyraShellStyle::AddSpaced(*Column, *Activity);

	// The tip or fact, its kind above it, and Previous and Next under it.
	UBorder* Panel = VeyraShellStyle::MakeSurface(*WidgetTree, VeyraShellStyle::EVeyraShellSurface::Panel, FMargin(Style.ScreenPadding));
	UVerticalBox* Entry = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	EntryKind = VeyraShellStyle::MakeText(*WidgetTree, FText::GetEmpty(), VeyraShellStyle::EVeyraShellText::Eyebrow);
	VeyraShellStyle::AddSpaced(*Entry, *EntryKind);
	EntryText = VeyraShellStyle::MakeText(*WidgetTree, FText::GetEmpty(), VeyraShellStyle::EVeyraShellText::Body);
	EntryText->SetAutoWrapText(true);
	VeyraShellStyle::AddSpaced(*Entry, *EntryText);
	UHorizontalBox* Browsing = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	const auto AddBrowse = [this, Browsing](const FText& Label, int32 Step) {
		UVeyraShellButton* Button = UVeyraShellButton::Make(*WidgetTree, Label, [this, Step] { Browse(Step, FPlatformTime::Seconds()); });
		Buttons.Add(Button);
		VeyraShellStyle::AddSpaced(*Browsing, *Button);
	};
	AddBrowse(LOCTEXT("Previous", "Previous"), -1);
	AddBrowse(LOCTEXT("Next", "Next"), 1);
	Entry->AddChildToVerticalBox(Browsing);
	Panel->SetContent(Entry);
	EntryPanel = Panel;
	EntryPanel->SetVisibility(ESlateVisibility::Collapsed);
	Column->AddChildToVerticalBox(Panel);

	Width->AddChild(Column);
	Backdrop->SetContent(Width);
	WidgetTree->RootWidget = Backdrop;
	// It takes the keyboard while loading, when no other screen has it (UVeyraMatchMenuSubsystem::UpdateInputMode).
	SetIsFocusable(true);
	return bFirst;
}

void UVeyraLoadingScreen::ShowFor(FVeyraSettingsStore* Store, int32 Seed, const FVeyraLoadingTiming& InTiming, double Now)
{
	UnbindSettings();
	SettingsStore = Store;
	ShownSeed = Seed;
	Timing = InTiming;
	if (SettingsStore)
	{
		SettingsHandle = SettingsStore->OnChanged.AddWeakLambda(this, [this](const FVeyraContentId& /*Id*/) { ApplyCategories(FPlatformTime::Seconds(), /*bAlways*/ false); });
	}
	ApplyCategories(Now, /*bAlways*/ true);
}

void UVeyraLoadingScreen::UnbindSettings()
{
	if (SettingsStore)
	{
		SettingsStore->OnChanged.Remove(SettingsHandle);
	}
	SettingsHandle.Reset();
	SettingsStore = nullptr;
}

void UVeyraLoadingScreen::ApplyCategories(double Now, bool bAlways)
{
	const EVeyraLoadingContent Content = VeyraInterfacePreferences::Resolve(*GetDefault<UVeyraGreyboxSettings>(), SettingsStore).LoadingContent;
	if (!bAlways && ShownContent == Content)
	{
		return;
	}
	ShownContent = Content;
	Show(VeyraLoadingModel::EntriesFor(Content, VeyraContentText::LoadingTips(), VeyraContentText::LoadingLore()), ShownSeed, Timing, Now);
}

bool UVeyraLoadingScreen::BrowseWithKey(const FKey& Key, double Now)
{
	if (Key != EKeys::Left && Key != EKeys::Right)
	{
		return false;
	}
	Browse(Key == EKeys::Left ? -1 : 1, Now);
	return true;
}

TSharedPtr<SWidget> UVeyraLoadingScreen::GetFocusTarget()
{
	UVeyraShellButton* Next = Buttons.IsValidIndex(1) ? Buttons[1].Get() : nullptr;
	return Next ? Next->TakeWidget().ToSharedPtr() : TakeWidget().ToSharedPtr();
}

FReply UVeyraLoadingScreen::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// Left and Right arrive here from a focused button, which leaves them unhandled.
	return BrowseWithKey(InKeyEvent.GetKey(), FPlatformTime::Seconds()) ? FReply::Handled() : Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UVeyraLoadingScreen::NativeDestruct()
{
	UnbindSettings();
	Super::NativeDestruct();
}

FText UVeyraLoadingScreen::StageText(EVeyraLoadingStage InStage)
{
	switch (InStage)
	{
	case EVeyraLoadingStage::LoadingMatch:
		return LOCTEXT("LoadingMatch", "Loading Match");
	case EVeyraLoadingStage::WaitingForPlayers:
		return LOCTEXT("WaitingForPlayers", "Waiting for Players");
	}
	return FText::GetEmpty();
}

void UVeyraLoadingScreen::Show(TArray<FVeyraLoadingEntry> InEntries, int32 Seed, const FVeyraLoadingTiming& InTiming, double Now)
{
	Entries = MoveTemp(InEntries);
	Timing = InTiming;
	Rotation = VeyraLoadingModel::Start(Entries.Num(), Seed, Now);
	if (EntryPanel)
	{
		// The player's Off shows the stage alone (SET-118).
		EntryPanel->SetVisibility(Entries.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
	ShowEntry();
}

void UVeyraLoadingScreen::SetStage(EVeyraLoadingStage InStage)
{
	Stage = InStage;
	if (StageLabel)
	{
		StageLabel->SetText(StageText(Stage));
	}
}

void UVeyraLoadingScreen::Update(double Now)
{
	if (VeyraLoadingModel::Advance(Rotation, Entries, Timing, Now))
	{
		ShowEntry();
	}
}

void UVeyraLoadingScreen::Browse(int32 Step, double Now)
{
	VeyraLoadingModel::Browse(Rotation, Step, Now);
	ShowEntry();
}

FText UVeyraLoadingScreen::GetShownText() const
{
	const TOptional<int32> Shown = Rotation.Shown();
	return Shown.IsSet() && Entries.IsValidIndex(Shown.GetValue()) ? Entries[Shown.GetValue()].Text : FText::GetEmpty();
}

TArray<UVeyraShellButton*> UVeyraLoadingScreen::GetButtons() const
{
	TArray<UVeyraShellButton*> Out;
	for (const TObjectPtr<UVeyraShellButton>& Button : Buttons)
	{
		Out.Add(Button.Get());
	}
	return Out;
}

void UVeyraLoadingScreen::ShowEntry()
{
	const TOptional<int32> Shown = Rotation.Shown();
	const FVeyraLoadingEntry* Entry = Shown.IsSet() && Entries.IsValidIndex(Shown.GetValue()) ? &Entries[Shown.GetValue()] : nullptr;
	if (EntryKind)
	{
		EntryKind->SetText(!Entry ? FText::GetEmpty() : Entry->bLore ? LOCTEXT("Lore", "Lore") : LOCTEXT("Tip", "Tip"));
	}
	if (EntryText)
	{
		EntryText->SetText(Entry ? Entry->Text : FText::GetEmpty());
	}
}

#undef LOCTEXT_NAMESPACE
