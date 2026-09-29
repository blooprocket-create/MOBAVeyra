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
#include "VeyraPlayerState.h"

#define LOCTEXT_NAMESPACE "VeyraMatchMenu"

namespace VeyraMatchMenuModel
{
bool CanEndCustomMatch(EVeyraMatchRules Rules, const APlayerState* Host, const APlayerState* Self)
{
	return Rules == EVeyraMatchRules::Practice && Host && Host == Self;
}

bool OffersVotes(EVeyraMatchRules Rules)
{
	return Rules == EVeyraMatchRules::Standard;
}

bool OffersDeveloperEnd(EVeyraMatchRules Rules)
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return Rules == EVeyraMatchRules::Standard;
#endif
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
	Confirming = EConfirming::Nothing;
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
	const bool bCanEndAsDeveloper = Owner && GameState && VeyraMatchMenuModel::OffersDeveloperEnd(GameState->GetMatchRules());
	const FText EndCustomLabel = LOCTEXT("EndCustomMatch", "End Custom Match");
	const FText DeveloperEndLabel = LOCTEXT("DeveloperEnd", "End Match (Developer)");

	const bool bConfirmingCustom = Confirming == EConfirming::EndCustomMatch && bCanEnd;
	const bool bConfirmingDeveloper = Confirming == EConfirming::DeveloperEnd && bCanEndAsDeveloper;
	const bool bVotes = GameState && VeyraMatchMenuModel::OffersVotes(GameState->GetMatchRules());
	if (bVotes && (Confirming == EConfirming::Surrender || Confirming == EConfirming::Remake))
	{
		const bool bSurrender = Confirming == EConfirming::Surrender;
		VeyraShellStyle::AddSpaced(*Content, *VeyraShellStyle::MakeText(*WidgetTree,
			bSurrender ? LOCTEXT("ConfirmSurrender", "Start a surrender vote?") : LOCTEXT("ConfirmRemake", "Start a remake vote?"),
			VeyraShellStyle::EVeyraShellText::Heading));
		VeyraShellStyle::AddSpaced(*Content, *VeyraShellStyle::MakeText(*WidgetTree,
			bSurrender ? LOCTEXT("ConfirmSurrenderDetail", "Your team votes; if enough agree, your team loses now.")
					   : LOCTEXT("ConfirmRemakeDetail", "Your team votes; if enough agree, the match ends with no contest."),
			VeyraShellStyle::EVeyraShellText::Body));
		AddButton(bSurrender ? LOCTEXT("StartSurrender", "Vote to Surrender") : LOCTEXT("StartRemake", "Vote to Remake"), [this, bSurrender] {
			if (AVeyraPlayerController* Player = Controller.Get())
			{
				Player->RequestVote(bSurrender ? EVeyraVoteKind::Surrender : EVeyraVoteKind::Remake);
			}
			if (Close)
			{
				Close();
			}
		});
		AddButton(LOCTEXT("CancelVote", "Cancel"), [this] {
			Confirming = EConfirming::Nothing;
			Rebuild();
		});
		return;
	}
	if (bConfirmingCustom || bConfirmingDeveloper)
	{
		const FText Title = bConfirmingCustom ? LOCTEXT("ConfirmTitle", "End this practice match?") : LOCTEXT("ConfirmDeveloperTitle", "End this match for everyone?");
		const FText Detail = bConfirmingCustom ? LOCTEXT("ConfirmDetail", "It ends now, with no winner.")
											   : LOCTEXT("ConfirmDeveloperDetail", "A development build's shortcut: it ends now, with no winner.");
		VeyraShellStyle::AddSpaced(*Content, *VeyraShellStyle::MakeText(*WidgetTree, Title, VeyraShellStyle::EVeyraShellText::Heading));
		VeyraShellStyle::AddSpaced(*Content, *VeyraShellStyle::MakeText(*WidgetTree, Detail, VeyraShellStyle::EVeyraShellText::Body));
		AddButton(bConfirmingCustom ? EndCustomLabel : DeveloperEndLabel, [this, bConfirmingCustom] {
			if (AVeyraPlayerController* Player = Controller.Get())
			{
				if (bConfirmingCustom)
				{
					Player->RequestEndCustomMatch();
				}
				else
				{
					Player->RequestDeveloperEndMatch();
				}
			}
			if (Close)
			{
				Close();
			}
		});
		AddButton(LOCTEXT("Cancel", "Cancel"), [this] {
			Confirming = EConfirming::Nothing;
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
	if (bVotes)
	{
		const auto Ask = [this](EVeyraVoteKind Kind) {
			if (AVeyraPlayerController* Player = Controller.Get())
			{
				Player->RequestVote(Kind);
			}
			if (Close)
			{
				Close();
			}
		};
		const auto Answer = [this](bool bYes) {
			if (AVeyraPlayerController* Player = Controller.Get())
			{
				Player->CastVote(bYes);
			}
			if (Close)
			{
				Close();
			}
		};
		const AVeyraPlayerController* Voter = Controller.Get();
		const FVeyraVoteState& Vote = Voter ? Voter->GetOpenVote() : GameState->GetVote();
		const AVeyraPlayerState* Own = Owner->GetPlayerState<AVeyraPlayerState>();
		// A team's vote reaches only that team (AVeyraPlayerController::GetOpenVote).
		const bool bMayAnswer = Vote.bOpen && Own && !Vote.Voted.Contains(Own->GetPlayerId());
		if (bMayAnswer)
		{
			AddButton(LOCTEXT("VoteYes", "Vote Yes"), [Answer] { Answer(true); });
			AddButton(LOCTEXT("VoteNo", "Vote No"), [Answer] { Answer(false); });
		}
		if (GameState->IsMatchPaused())
		{
			AddButton(LOCTEXT("ResumeEarly", "Resume Early"), [Ask] { Ask(EVeyraVoteKind::Resume); });
		}
		else
		{
			AddButton(LOCTEXT("RequestPause", "Request Pause"), [Ask] { Ask(EVeyraVoteKind::Pause); });
			AddButton(LOCTEXT("Surrender", "Surrender"), [this] {
				Confirming = EConfirming::Surrender;
				Rebuild();
			});
			AddButton(LOCTEXT("Remake", "Remake"), [this] {
				Confirming = EConfirming::Remake;
				Rebuild();
			});
		}
	}
	if (bCanEnd)
	{
		AddButton(EndCustomLabel, [this] {
			Confirming = EConfirming::EndCustomMatch;
			Rebuild();
		});
	}
	if (bCanEndAsDeveloper)
	{
		AddButton(DeveloperEndLabel, [this] {
			Confirming = EConfirming::DeveloperEnd;
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
