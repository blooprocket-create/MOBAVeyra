// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Match/VeyraMatchMenu.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/World.h"
#include "Shell/VeyraShellButton.h"
#include "Shell/VeyraShellStyle.h"
#include "Shell/VeyraShellStyleSettings.h"
#include "VeyraGameState.h"
#include "VeyraPlayerController.h"

#define LOCTEXT_NAMESPACE "VeyraMatchMenu"

namespace VeyraMatchMenuModel
{
bool CanEndCustomMatch(EVeyraMatchRules Rules, const APlayerState* Host, const APlayerState* Self)
{
	return Rules == EVeyraMatchRules::Practice && Host && Host == Self;
}
}

bool UVeyraMatchMenu::Initialize()
{
	const bool bFirst = Super::Initialize();
	if (bFirst && WidgetTree && !WidgetTree->RootWidget)
	{
		// A translucent backdrop over the match, and the menu in its middle.
		const UVeyraShellStyleSettings& Style = *GetDefault<UVeyraShellStyleSettings>();
		UBorder* Scrim = VeyraShellStyle::MakeBorder(*WidgetTree, Style.MenuScrimColor, Style.ScreenPadding);
		Scrim->SetHorizontalAlignment(HAlign_Center);
		Scrim->SetVerticalAlignment(VAlign_Center);
		USizeBox* Width = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Width->SetWidthOverride(Style.MenuWidth);
		UBorder* Panel = VeyraShellStyle::MakeBorder(*WidgetTree, Style.PanelColor, Style.ScreenPadding);
		Content = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Panel->SetContent(Content);
		Width->AddChild(Panel);
		Scrim->SetContent(Width);
		WidgetTree->RootWidget = Scrim;
		// The open menu's input mode gives it keyboard focus.
		SetIsFocusable(true);
	}
	return bFirst;
}

void UVeyraMatchMenu::Show(AVeyraPlayerController& InController, TFunction<void()> InClose)
{
	Controller = &InController;
	Close = MoveTemp(InClose);
	bConfirming = false;
	Rebuild();
}

void UVeyraMatchMenu::Rebuild()
{
	if (!Content)
	{
		return;
	}
	Content->ClearChildren();
	Buttons.Reset();
	AVeyraPlayerController* Owner = Controller.Get();
	const AVeyraGameState* GameState = Owner && Owner->GetWorld() ? Owner->GetWorld()->GetGameState<AVeyraGameState>() : nullptr;
	const bool bCanEnd = Owner && GameState && VeyraMatchMenuModel::CanEndCustomMatch(GameState->GetMatchRules(), GameState->GetHost(), Owner->PlayerState);

	if (bConfirming && bCanEnd)
	{
		VeyraShellStyle::AddSpaced(*Content, *VeyraShellStyle::MakeText(*WidgetTree, LOCTEXT("ConfirmTitle", "End this practice match?"), VeyraShellStyle::EVeyraShellText::Heading));
		VeyraShellStyle::AddSpaced(*Content,
			*VeyraShellStyle::MakeText(*WidgetTree, LOCTEXT("ConfirmDetail", "It ends now, with no winner."), VeyraShellStyle::EVeyraShellText::Body));
		AddButton(LOCTEXT("EndCustomMatch", "End Custom Match"), [this] {
			if (AVeyraPlayerController* Player = Controller.Get())
			{
				Player->RequestEndCustomMatch();
			}
			if (Close)
			{
				Close();
			}
		});
		AddButton(LOCTEXT("Cancel", "Cancel"), [this] {
			bConfirming = false;
			Rebuild();
		});
		return;
	}

	VeyraShellStyle::AddSpaced(*Content, *VeyraShellStyle::MakeText(*WidgetTree, LOCTEXT("Title", "Menu"), VeyraShellStyle::EVeyraShellText::Title));
	AddButton(LOCTEXT("Resume", "Resume"), [this] {
		if (Close)
		{
			Close();
		}
	});
	if (bCanEnd)
	{
		AddButton(LOCTEXT("EndCustomMatch", "End Custom Match"), [this] {
			bConfirming = true;
			Rebuild();
		});
	}
}

UVeyraShellButton* UVeyraMatchMenu::AddButton(const FText& Label, TFunction<void()> Action)
{
	UVeyraShellButton* Button = UVeyraShellButton::Make(*WidgetTree, Label, MoveTemp(Action));
	Buttons.Add(Button);
	VeyraShellStyle::AddSpaced(*Content, *Button);
	return Button;
}

TArray<UVeyraShellButton*> UVeyraMatchMenu::GetButtons() const
{
	TArray<UVeyraShellButton*> Out;
	for (const TObjectPtr<UVeyraShellButton>& Button : Buttons)
	{
		Out.Add(Button.Get());
	}
	return Out;
}

UVeyraShellButton* UVeyraMatchMenu::FindButton(const FText& Label) const
{
	for (const TObjectPtr<UVeyraShellButton>& Button : Buttons)
	{
		if (Button && Button->GetLabel().ToString() == Label.ToString())
		{
			return Button.Get();
		}
	}
	return nullptr;
}

#undef LOCTEXT_NAMESPACE
