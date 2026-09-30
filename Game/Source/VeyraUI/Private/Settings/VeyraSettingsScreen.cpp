// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Settings/VeyraSettingsScreen.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Shell/VeyraShellStyle.h"
#include "Shell/VeyraShellStyleSettings.h"
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
}

bool UVeyraSettingsScreen::Initialize()
{
	const bool bFirst = Super::Initialize();
	if (bFirst && WidgetTree && !WidgetTree->RootWidget)
	{
		// A window over the client or the match, which stay in view around it (Settings Bible §6.2).
		UBorder* Scrim = VeyraShellStyle::MakeBorder(*WidgetTree, VeyraSettingsLayout::Style().MenuScrimColor, VeyraSettingsLayout::Style().ScreenPadding);
		Scrim->SetHorizontalAlignment(HAlign_Center);
		Scrim->SetVerticalAlignment(VAlign_Center);
		USizeBox* Window = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Window->SetWidthOverride(VeyraSettingsLayout::Style().SettingsWidth);
		Window->SetHeightOverride(VeyraSettingsLayout::Style().SettingsHeight);
		UBorder* Panel = VeyraShellStyle::MakeSurface(*WidgetTree, VeyraShellStyle::EVeyraShellSurface::Raised, FMargin(VeyraSettingsLayout::Style().ScreenPadding));
		UVerticalBox* Frame = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Panel->SetContent(Frame);
		Window->AddChild(Panel);
		Scrim->SetContent(Window);

		// The title, the search, and the screen's own actions.
		UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UTextBlock* Title = VeyraShellStyle::MakeText(*WidgetTree, LOCTEXT("Title", "Settings"), VeyraShellStyle::EVeyraShellText::Heading);
		Title->SetAutoWrapText(false);
		VeyraShellStyle::AddSpaced(*Header, *Title);
		VeyraSettingsLayout::AddStretch(*WidgetTree, *Header);
		SearchBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
		FEditableTextBoxStyle FieldStyle = SearchBox->GetWidgetStyle();
		const FSlateRoundedBoxBrush Field(VeyraSettingsLayout::Style().SurfaceRaisedColor, VeyraSettingsLayout::Style().ButtonCornerRadius, VeyraSettingsLayout::Style().HairlineColor, 1.0f);
		const FSlateRoundedBoxBrush Focused(VeyraSettingsLayout::Style().SurfaceRaisedColor, VeyraSettingsLayout::Style().ButtonCornerRadius, VeyraSettingsLayout::Style().AccentColor, 1.0f);
		FieldStyle.SetBackgroundImageNormal(Field);
		FieldStyle.SetBackgroundImageHovered(Field);
		FieldStyle.SetBackgroundImageFocused(Focused);
		FieldStyle.SetBackgroundImageReadOnly(Field);
		FieldStyle.SetForegroundColor(FSlateColor(VeyraSettingsLayout::Style().TextColor));
		FieldStyle.SetFocusedForegroundColor(FSlateColor(VeyraSettingsLayout::Style().TextColor));
		FieldStyle.SetPadding(FMargin(VeyraSettingsLayout::Style().ButtonPadding));
		FieldStyle.SetFont(VeyraShellStyle::FontFor(VeyraShellStyle::EVeyraShellText::Body));
		SearchBox->SetWidgetStyle(FieldStyle);
		SearchBox->SetHintText(LOCTEXT("SearchHint", "Search settings"));
		SearchBox->OnTextChanged.AddUniqueDynamic(this, &UVeyraSettingsScreen::HandleSearchChanged);
		VeyraShellStyle::AddSpaced(*Header, *VeyraSettingsLayout::Sized(*WidgetTree, *SearchBox, VeyraSettingsLayout::Style().SettingsSearchWidth));
		Actions = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Header->AddChild(Actions);
		Frame->AddChild(Header);
		VeyraShellStyle::AddSpaced(*Frame, *VeyraShellStyle::MakeRule(*WidgetTree));

		// The categories down the left, the settings beside them.
		UHorizontalBox* Body = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Nav = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		UHorizontalBoxSlot* NavSlot = Body->AddChildToHorizontalBox(VeyraSettingsLayout::Sized(*WidgetTree, *Nav, VeyraSettingsLayout::Style().SettingsNavWidth));
		NavSlot->SetPadding(FMargin(0.0f, 0.0f, VeyraSettingsLayout::Style().Spacing, 0.0f));
		UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
		Rows = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Scroll->AddChild(Rows);
		Body->AddChildToHorizontalBox(Scroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		VeyraSettingsLayout::AddFilling(*Frame, *Body);

		VeyraShellStyle::AddSpaced(*Frame, *VeyraShellStyle::MakeRule(*WidgetTree));
		Footer = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Frame->AddChild(Footer);
		WidgetTree->RootWidget = Scrim;
		SetIsFocusable(true);
	}
	return bFirst;
}

void UVeyraSettingsScreen::Show(UVeyraSettingsSubsystem& InSettings, bool bInLiveMatch, TFunction<void()> InClose)
{
	StopListening();
	Settings = &InSettings;
	bInMatch = bInLiveMatch;
	Close = MoveTemp(InClose);
	if (FVeyraSettingsStore* Store = GetStore())
	{
		// A change from anywhere, Undo or the account's sync included, shows at once.
		ChangedHandle = Store->OnChanged.AddWeakLambda(this, [this](const FVeyraContentId&) { Rebuild(); });
	}
	Rebuild();
}

void UVeyraSettingsScreen::ShowCategory(EVeyraSettingCategory InCategory)
{
	Category = InCategory;
	Confirming = EConfirming::None;
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
}

void UVeyraSettingsScreen::NativeDestruct()
{
	StopListening();
	Super::NativeDestruct();
}

void UVeyraSettingsScreen::Change(const FVeyraContentId& Id, const FString& Value)
{
	if (FVeyraSettingsStore* Store = GetStore())
	{
		Store->Set(Id, Value, bInMatch);
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
	Model = VeyraSettingsModels::Describe(*Store, Category, Search, bInMatch);
	Category = Model.Category;
	Buttons.Reset();

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

	Nav->ClearChildren();
	for (const EVeyraSettingCategory Shown : Model.Categories)
	{
		const FText Name = VeyraSettingsModels::CategoryName(Shown);
		AddButton(*Nav, EVeyraShellButtonKind::Tab, Name, Name, [this, Shown] { ShowCategory(Shown); }, true, !Model.bSearching && Shown == Model.Category);
	}

	Rows->ClearChildren();
	if (Model.bSearching && Model.Rows.IsEmpty())
	{
		VeyraSettingsLayout::AddText(*WidgetTree, *Rows, FText::Format(LOCTEXT("NoResults", "No setting matches \"{0}\"."), FText::FromString(Search)), VeyraShellStyle::EVeyraShellText::Muted);
	}
	for (const FVeyraSettingRowModel& Row : Model.Rows)
	{
		BuildRow(Row);
	}
	BuildFooter();
}

void UVeyraSettingsScreen::BuildRow(const FVeyraSettingRowModel& Row)
{
	UBorder* Surface = VeyraShellStyle::MakeSurface(*WidgetTree, VeyraShellStyle::EVeyraShellSurface::Panel, FMargin(VeyraSettingsLayout::Style().Spacing));
	UVerticalBox* Lines = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Surface->SetContent(Lines);
	if (Model.bSearching)
	{
		// A search spans categories, so each result says where it lives (§6.3).
		VeyraSettingsLayout::AddText(*WidgetTree, *Lines, VeyraSettingsModels::CategoryName(Row.Category), VeyraShellStyle::EVeyraShellText::Eyebrow);
	}
	UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UVerticalBox* About = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	VeyraSettingsLayout::AddText(*WidgetTree, *About, Row.Name, VeyraShellStyle::EVeyraShellText::Body);
	if (!Row.Description.IsEmpty())
	{
		VeyraSettingsLayout::AddText(*WidgetTree, *About, Row.Description, VeyraShellStyle::EVeyraShellText::Muted);
	}
	if (Row.bAfterRestart)
	{
		VeyraSettingsLayout::AddText(*WidgetTree, *About, LOCTEXT("AfterRestart", "Takes effect after a restart."), VeyraShellStyle::EVeyraShellText::Muted);
	}
	if (Row.bLocked)
	{
		VeyraSettingsLayout::AddText(*WidgetTree, *About, LOCTEXT("OutsideMatches", "Changes outside matches only."), VeyraShellStyle::EVeyraShellText::Muted);
	}
	VeyraSettingsLayout::AddFilling(*Line, *About);

	UWrapBox* Controls = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass());
	Controls->SetExplicitWrapSize(true);
	Controls->SetWrapSize(VeyraSettingsLayout::Style().SettingsControlWidth);
	const FVeyraContentId Id = Row.Id;
	if (Row.Kind == EVeyraSettingKind::Range)
	{
		AddButton(*Controls, EVeyraShellButtonKind::Secondary, StepLabel(Row, false), LOCTEXT("Lower", "-"), [this, Id, Value = Row.Lower.Get(FString())] {
			Change(Id, Value);
		}, !Row.bLocked && Row.Lower.IsSet());
		UTextBlock* Value = VeyraShellStyle::MakeText(*WidgetTree, Row.ValueText, VeyraShellStyle::EVeyraShellText::Heading);
		Value->SetAutoWrapText(false);
		VeyraShellStyle::AddSpaced(*Controls, *Value);
		AddButton(*Controls, EVeyraShellButtonKind::Secondary, StepLabel(Row, true), LOCTEXT("Higher", "+"), [this, Id, Value = Row.Higher.Get(FString())] {
			Change(Id, Value);
		}, !Row.bLocked && Row.Higher.IsSet());
	}
	else
	{
		for (const FVeyraSettingOptionModel& Option : Row.Options)
		{
			AddButton(*Controls, EVeyraShellButtonKind::Tab, OptionLabel(Row, Option), Option.Label, [this, Id, Value = Option.Value] { Change(Id, Value); },
				!Row.bLocked, Option.bSelected);
		}
	}
	UHorizontalBoxSlot* ControlsSlot = Line->AddChildToHorizontalBox(VeyraSettingsLayout::Sized(*WidgetTree, *Controls, VeyraSettingsLayout::Style().SettingsControlWidth));
	ControlsSlot->SetVerticalAlignment(VAlign_Center);
	ControlsSlot->SetPadding(FMargin(VeyraSettingsLayout::Style().Spacing, 0.0f));
	// Reset shows only where it would change something (§6.1).
	if (Row.bChanged && !Row.bLocked)
	{
		AddButton(*Line, EVeyraShellButtonKind::Quiet, ResetLabel(Row), LOCTEXT("ResetOne", "Reset"), [this, Id] {
			if (FVeyraSettingsStore* Store = GetStore())
			{
				Store->Reset(Id, bInMatch);
			}
		});
	}
	Lines->AddChild(Line);
	VeyraShellStyle::AddSpaced(*Rows, *Surface);
}

void UVeyraSettingsScreen::BuildFooter()
{
	Footer->ClearChildren();
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
			if (FVeyraSettingsStore* Store = GetStore())
			{
				if (bAll)
				{
					Store->ResetAll(bInMatch);
				}
				else
				{
					Store->ResetCategory(Model.Category, bInMatch);
				}
			}
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
	VeyraSettingsLayout::AddStretch(*WidgetTree, *Footer);
	// Nothing to save: every change is kept as it is made (§13).
	VeyraSettingsLayout::AddText(*WidgetTree, *Footer, LOCTEXT("Autosave", "Changes are saved as you make them."), VeyraShellStyle::EVeyraShellText::Muted)->SetAutoWrapText(false);
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

#undef LOCTEXT_NAMESPACE
