// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Match/VeyraMatchMenuSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Chat/VeyraChatCommands.h"
#include "Chat/VeyraChatComposer.h"
#include "Client/VeyraClientFlowSubsystem.h"
#include "Client/VeyraClientIntents.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerState.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Loading/VeyraLoadingScreen.h"
#include "Shell/VeyraShellLook.h"
#include "Match/VeyraMatchMenu.h"
#include "Scoreboard/VeyraScoreboard.h"
#include "Engine/GameViewportClient.h"
#include "Greybox/VeyraGreyboxSettings.h"
#include "Settings/VeyraInterfacePreferences.h"
#include "Settings/VeyraSettingsScreen.h"
#include "Shell/VeyraShellStyleSettings.h"
#include "Shop/VeyraShopScreen.h"
#include "Shell/VeyraUIInputSettings.h"
#include "Tuning/VeyraMatchTuningSubsystem.h"
#include "VeyraGameState.h"
#include "VeyraPlayerController.h"
#include "VeyraSettingsSubsystem.h"
#include "VeyraUILog.h"

bool UVeyraMatchMenuSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
}

void UVeyraMatchMenuSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (UVeyraSettingsSubsystem* Player = Collection.InitializeDependency<UVeyraSettingsSubsystem>(); Player && Player->IsReady())
	{
		SettingsHandle = Player->GetStore().OnChanged.AddUObject(this, &UVeyraMatchMenuSubsystem::OnPlayerSettingChanged);
	}
	RefreshKeys();
	TArray<FString> Problems = GetDefault<UVeyraUIInputSettings>()->Validate();
	Problems.Append(GetDefault<UVeyraShellStyleSettings>()->Validate());
	for (const FString& Problem : Problems)
	{
		UE_LOG(LogVeyraUI, Error, TEXT("The in-match menu is off: %s"), *Problem);
	}
	bReady = Problems.IsEmpty();
	if (bReady)
	{
		TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UVeyraMatchMenuSubsystem::Tick));
	}
}

void UVeyraMatchMenuSubsystem::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	// Settings that ended first took their change event with them.
	if (UVeyraSettingsSubsystem* Player = GetGameInstance()->GetSubsystem<UVeyraSettingsSubsystem>(); Player && Player->IsReady())
	{
		Player->GetStore().OnChanged.Remove(SettingsHandle);
	}
	if (Menu)
	{
		Menu->RemoveFromParent();
		Menu = nullptr;
	}
	if (Settings)
	{
		Settings->RemoveFromParent();
		Settings = nullptr;
	}
	if (Shop)
	{
		Shop->RemoveFromParent();
		Shop = nullptr;
	}
	if (Chat)
	{
		Chat->RemoveFromParent();
		Chat = nullptr;
	}
	HideScoreboard();
	Super::Deinitialize();
}

bool UVeyraMatchMenuSubsystem::Tick(float /*DeltaSeconds*/)
{
	AVeyraPlayerController* Controller = Cast<AVeyraPlayerController>(GetGameInstance()->GetFirstLocalPlayerController());
	if (!Controller || Controller == BoundController.Get() || !Controller->GetWorld() || !Controller->GetWorld()->GetGameState<AVeyraGameState>())
	{
		return true;
	}
	UEnhancedInputLocalPlayerSubsystem* Input = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(Controller->GetLocalPlayer());
	if (!Input)
	{
		return true;
	}
	// A new match: the old controller, its binding and any open screen went with the old world. The
	// actions and their mapping are built at runtime, so no binary input asset exists.
	Menu = nullptr;
	Shop = nullptr;
	Scoreboard = nullptr;
	Settings = nullptr;
	Chat = nullptr;
	const UVeyraUIInputSettings& Keys = GetKeys();
	MenuAction = NewObject<UInputAction>(this, NAME_None, RF_Transient);
	MenuAction->ValueType = EInputActionValueType::Boolean;
	ShopAction = NewObject<UInputAction>(this, NAME_None, RF_Transient);
	ShopAction->ValueType = EInputActionValueType::Boolean;
	ScoreboardAction = NewObject<UInputAction>(this, NAME_None, RF_Transient);
	ScoreboardAction->ValueType = EInputActionValueType::Boolean;
	ChatAction = NewObject<UInputAction>(this, NAME_None, RF_Transient);
	ChatAction->ValueType = EInputActionValueType::Boolean;
	ShopSearchAction = NewObject<UInputAction>(this, NAME_None, RF_Transient);
	ShopSearchAction->ValueType = EInputActionValueType::Boolean;
	MenuMapping = NewObject<UInputMappingContext>(this, NAME_None, RF_Transient);
	MenuMapping->MapKey(MenuAction, Keys.MatchMenuKey);
	MenuMapping->MapKey(ShopAction, Keys.ShopKey);
	MenuMapping->MapKey(ScoreboardAction, Keys.ScoreboardKey);
	MenuMapping->MapKey(ChatAction, Keys.ChatKey);
	MenuMapping->MapKey(ShopSearchAction, Keys.FocusShopSearchKey);
	Input->AddMappingContext(MenuMapping, /*Priority*/ 1);
	UEnhancedInputComponent* Component = NewObject<UEnhancedInputComponent>(Controller, NAME_None, RF_Transient);
	Component->BindAction(MenuAction, ETriggerEvent::Started, this, &UVeyraMatchMenuSubsystem::ToggleMenu);
	Component->BindAction(ShopAction, ETriggerEvent::Started, this, &UVeyraMatchMenuSubsystem::ToggleShop);
	// Held or toggled, as the player chooses (Settings Bible #56).
	Component->BindAction(ScoreboardAction, ETriggerEvent::Started, this, &UVeyraMatchMenuSubsystem::PressScoreboardKey);
	Component->BindAction(ScoreboardAction, ETriggerEvent::Completed, this, &UVeyraMatchMenuSubsystem::ReleaseScoreboardKey);
	Component->BindAction(ChatAction, ETriggerEvent::Started, this, &UVeyraMatchMenuSubsystem::PressChatKey);
	Component->BindAction(ShopSearchAction, ETriggerEvent::Started, this, &UVeyraMatchMenuSubsystem::FocusShopSearch);
	Controller->PushInputComponent(Component);
	MenuInput = Component;
	BoundController = Controller;
	// A loading screen up before this controller was bound takes the keyboard now.
	UpdateInputMode();
	return true;
}

const UVeyraUIInputSettings& UVeyraMatchMenuSubsystem::GetKeys() const
{
	return PlayerKeys ? *PlayerKeys : *GetDefault<UVeyraUIInputSettings>();
}

void UVeyraMatchMenuSubsystem::OnPlayerSettingChanged(const FVeyraContentId& Id)
{
	const UVeyraSettingsSubsystem* Player = GetGameInstance()->GetSubsystem<UVeyraSettingsSubsystem>();
	if (Player && Player->IsReady() && Player->GetStore().GetRegistry().Bindings.Contains(Id))
	{
		RefreshKeys();
	}
}

void UVeyraMatchMenuSubsystem::RefreshKeys()
{
	PlayerKeys = NewObject<UVeyraUIInputSettings>(this, NAME_None, RF_Transient);
	const UVeyraSettingsSubsystem* Player = GetGameInstance()->GetSubsystem<UVeyraSettingsSubsystem>();
	if (Player && Player->IsReady())
	{
		VeyraSettings::ApplyBindings(*PlayerKeys, Player->GetStore());
	}
	AVeyraPlayerController* Controller = BoundController.Get();
	UEnhancedInputLocalPlayerSubsystem* Input = Controller && Controller->GetLocalPlayer() ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(Controller->GetLocalPlayer()) : nullptr;
	if (!Input || !MenuMapping)
	{
		// The next match's controller maps them.
		return;
	}
	// The same actions on the new keys, so what they are bound to stays.
	Input->RemoveMappingContext(MenuMapping);
	MenuMapping = NewObject<UInputMappingContext>(this, NAME_None, RF_Transient);
	const UVeyraUIInputSettings& Keys = GetKeys();
	const TPair<UInputAction*, FKey> Mapped[] = { { MenuAction, Keys.MatchMenuKey }, { ShopAction, Keys.ShopKey }, { ScoreboardAction, Keys.ScoreboardKey },
		{ ChatAction, Keys.ChatKey }, { ShopSearchAction, Keys.FocusShopSearchKey } };
	for (const TPair<UInputAction*, FKey>& Pair : Mapped)
	{
		if (Pair.Key && Pair.Value.IsValid())
		{
			MenuMapping->MapKey(Pair.Key, Pair.Value);
		}
	}
	Input->AddMappingContext(MenuMapping, /*Priority*/ 1);
}

void UVeyraMatchMenuSubsystem::ToggleMenu()
{
	AVeyraPlayerController* Controller = BoundController.Get();
	if (Controller && Controller->CancelPendingCast())
	{
		// The menu's key hides a waiting cast or a preview first (ADR-041 §1).
		return;
	}
	if (Settings)
	{
		// The menu's key closes Settings first.
		CloseSettings();
	}
	else if (Scoreboard && Preferences().bScoreboardToggles)
	{
		// And a scoreboard toggled open (SET-56: Escape closes).
		HideScoreboard();
	}
	else if (Shop && !Menu)
	{
		// The menu's key closes the shop first.
		CloseShop();
	}
	else if (Menu)
	{
		CloseMenu();
	}
	else
	{
		OpenMenu();
	}
}

void UVeyraMatchMenuSubsystem::ToggleShop()
{
	if (Shop)
	{
		CloseShop();
	}
	else
	{
		OpenShop();
	}
}

void UVeyraMatchMenuSubsystem::FocusShopSearch()
{
	// The shop opens if it is shut, and its search takes the keyboard (SET-58; ADR-058 §1).
	if (!Shop)
	{
		OpenShop();
	}
	if (Shop)
	{
		Shop->FocusSearch();
	}
}

void UVeyraMatchMenuSubsystem::OpenShop()
{
	AVeyraPlayerController* Controller = BoundController.Get();
	if (!Controller || !Controller->IsLocalController())
	{
		return;
	}
	// In the player's look (ADR-055 §2–§3).
	VeyraShellLook::FollowPlayer(Controller);
	Shop = CreateWidget<UVeyraShopScreen>(Controller);
	if (!Shop)
	{
		return;
	}
	Shop->Show(*Controller, [this] { CloseShop(); });
	// Centred over the match, which stays in view and in play around it. The viewport keeps these
	// only once the shop is in it, and a size resets the anchors, so they go in this order.
	const UVeyraShellStyleSettings& Style = *GetDefault<UVeyraShellStyleSettings>();
	const FVector2D Centre(0.5, 0.5);
	Shop->AddToViewport();
	Shop->SetDesiredSizeInViewport(FVector2D(Style.ShopWidth, Style.ShopHeight));
	Shop->SetAnchorsInViewport(FAnchors(Centre.X, Centre.Y));
	Shop->SetAlignmentInViewport(Centre);
	UpdateInputMode();
}

void UVeyraMatchMenuSubsystem::CloseShop()
{
	if (Shop)
	{
		Shop->RemoveFromParent();
		Shop = nullptr;
	}
	UpdateInputMode();
}

void UVeyraMatchMenuSubsystem::ShowScoreboard()
{
	AVeyraPlayerController* Controller = BoundController.Get();
	if (Scoreboard || !Controller || !Controller->IsLocalController())
	{
		return;
	}
	// In the player's look (ADR-055 §2–§3).
	VeyraShellLook::FollowPlayer(Controller);
	Scoreboard = CreateWidget<UVeyraScoreboard>(Controller);
	if (!Scoreboard)
	{
		return;
	}
	Scoreboard->Show(*Controller);
	// Under the menu; it takes no input, so the game keeps it.
	Scoreboard->AddToViewport();
}

void UVeyraMatchMenuSubsystem::HideScoreboard()
{
	if (Scoreboard)
	{
		Scoreboard->RemoveFromParent();
		Scoreboard = nullptr;
	}
}

void UVeyraMatchMenuSubsystem::OpenMenu()
{
	AVeyraPlayerController* Controller = BoundController.Get();
	if (!Controller || !Controller->IsLocalController())
	{
		return;
	}
	// In the player's look (ADR-055 §2–§3).
	VeyraShellLook::FollowPlayer(Controller);
	Menu = CreateWidget<UVeyraMatchMenu>(Controller);
	if (!Menu)
	{
		return;
	}
	// Leave Match goes through the client coordinator, which a game without a launcher lacks (ADR-053 §1).
	TFunction<void()> LeaveMatch;
	const UGameInstance* Game = Controller->GetGameInstance();
	UVeyraClientFlowSubsystem* Flow = Game ? Game->GetSubsystem<UVeyraClientFlowSubsystem>() : nullptr;
	if (Flow && Flow->GetClient().CanIssue(EVeyraClientIntent::LeaveMatch))
	{
		LeaveMatch = [WeakFlow = TWeakObjectPtr<UVeyraClientFlowSubsystem>(Flow)] {
			if (UVeyraClientFlowSubsystem* Leaving = WeakFlow.Get())
			{
				Leaving->GetClient().LeaveLiveMatch();
			}
		};
	}
	const bool bConfirmLeave = VeyraInterfacePreferences::Resolve(*GetDefault<UVeyraGreyboxSettings>(), VeyraInterfacePreferences::StoreOf(Controller)).bConfirmLeaveMatch;
	Menu->Show(*Controller, [this] { CloseMenu(); }, [this] { OpenSettings(); }, MoveTemp(LeaveMatch), bConfirmLeave);
	// Above the shop, if it is open.
	Menu->AddToViewport(/*ZOrder*/ 1);
	UpdateInputMode();
}

void UVeyraMatchMenuSubsystem::CloseMenu()
{
	if (Menu)
	{
		Menu->RemoveFromParent();
		Menu = nullptr;
	}
	UpdateInputMode();
}

void UVeyraMatchMenuSubsystem::OpenSettings()
{
	AVeyraPlayerController* Controller = BoundController.Get();
	UVeyraSettingsSubsystem* PlayerSettings = UVeyraSettingsSubsystem::Get(Controller);
	if (Settings || !Controller || !Controller->IsLocalController() || !PlayerSettings)
	{
		return;
	}
	CloseMenu();
	// In the player's look (ADR-055 §2–§3).
	VeyraShellLook::FollowPlayer(Controller);
	Settings = CreateWidget<UVeyraSettingsScreen>(Controller);
	if (!Settings)
	{
		return;
	}
	Settings->Show(*PlayerSettings, /*bInLiveMatch*/ true, [this] { CloseSettings(); });
	// Above the shop and where the menu was.
	Settings->AddToViewport(/*ZOrder*/ 2);
	UpdateInputMode();
}

void UVeyraMatchMenuSubsystem::CloseSettings()
{
	if (Settings)
	{
		Settings->RemoveFromParent();
		Settings = nullptr;
	}
	UpdateInputMode();
}

FVeyraInterfacePreferences UVeyraMatchMenuSubsystem::Preferences() const
{
	const UVeyraSettingsSubsystem* Player = GetGameInstance()->GetSubsystem<UVeyraSettingsSubsystem>();
	return VeyraInterfacePreferences::Resolve(*GetDefault<UVeyraGreyboxSettings>(), Player && Player->IsReady() ? &Player->GetStore() : nullptr);
}

void UVeyraMatchMenuSubsystem::PressScoreboardKey()
{
	if (Preferences().bScoreboardToggles && Scoreboard)
	{
		HideScoreboard();
		return;
	}
	ShowScoreboard();
}

void UVeyraMatchMenuSubsystem::ReleaseScoreboardKey()
{
	if (!Preferences().bScoreboardToggles)
	{
		HideScoreboard();
	}
}

void UVeyraMatchMenuSubsystem::PressChatKey()
{
	// With Shift held, the composer opens on All (ADR-029 §8).
	const bool bAll = FSlateApplication::IsInitialized() && FSlateApplication::Get().GetModifierKeys().IsShiftDown();
	OpenChat(bAll ? EVeyraChatChannel::All : EVeyraChatChannel::Team);
}

void UVeyraMatchMenuSubsystem::OpenChat(EVeyraChatChannel Channel)
{
	AVeyraPlayerController* Controller = BoundController.Get();
	if (Chat || Menu || Settings || !Controller || !Controller->IsLocalController())
	{
		return;
	}
	// In the player's look (ADR-055 §2–§3).
	VeyraShellLook::FollowPlayer(Controller);
	Chat = CreateWidget<UVeyraChatComposer>(Controller);
	if (!Chat)
	{
		return;
	}
	Chat->Show(Channel, UVeyraMatchTuningSubsystem::Get().Chat.MaxCharacters, [this](EVeyraChatChannel Sent, const FString& Typed) { SubmitChat(Sent, Typed); },
		[this] { CloseChat(); });
	// Under the shop, the menu and Settings; placed once in the viewport, as the shop is.
	Chat->AddToViewport();
	Chat->Place();
	UpdateInputMode();
}

void UVeyraMatchMenuSubsystem::CloseChat()
{
	if (Chat)
	{
		Chat->RemoveFromParent();
		Chat = nullptr;
	}
	UpdateInputMode();
}

void UVeyraMatchMenuSubsystem::SubmitChat(EVeyraChatChannel Channel, const FString& Typed)
{
	AVeyraPlayerController* Controller = BoundController.Get();
	if (!Controller)
	{
		return;
	}
	const FVeyraChatCommand Command = VeyraChatCommands::Parse(Typed, Channel);
	switch (Command.Kind)
	{
	case EVeyraChatCommandKind::Nothing:
		break;
	case EVeyraChatCommandKind::Send:
		Controller->RequestChat(Command.Channel, Command.Text);
		break;
	case EVeyraChatCommandKind::Mute:
	case EVeyraChatCommandKind::Unmute:
	{
		// Anyone else in the match, by the name the scoreboard shows.
		TArray<FVeyraChatParticipant> Participants;
		if (const AGameStateBase* Match = Controller->GetWorld() ? Controller->GetWorld()->GetGameState() : nullptr)
		{
			for (const APlayerState* Participant : Match->PlayerArray)
			{
				if (Participant && Participant != Controller->PlayerState)
				{
					Participants.Add({ Participant->GetPlayerId(), Participant->GetPlayerName() });
				}
			}
		}
		const TOptional<int32> Found = VeyraChatCommands::FindPlayer(Command.Name, Participants);
		if (!Found.IsSet())
		{
			Controller->NoteChat(EVeyraChatNotice::NoSuchPlayer, Command.Name);
			break;
		}
		const bool bMute = Command.Kind == EVeyraChatCommandKind::Mute;
		const FVeyraChatParticipant* Named = Participants.FindByPredicate([&Found](const FVeyraChatParticipant& Each) { return Each.PlayerId == Found.GetValue(); });
		Controller->RequestMute(Found.GetValue(), bMute);
		Controller->NoteChat(bMute ? EVeyraChatNotice::Muted : EVeyraChatNotice::Unmuted, Named ? Named->Name : Command.Name);
		break;
	}
	case EVeyraChatCommandKind::Party:
	case EVeyraChatCommandKind::Reply:
	case EVeyraChatCommandKind::Message:
		SubmitOutsideChat(*Controller, Command);
		break;
	case EVeyraChatCommandKind::Unknown:
		Controller->NoteChat(EVeyraChatNotice::UnknownCommand, Command.Name);
		break;
	}
}

void UVeyraMatchMenuSubsystem::SubmitOutsideChat(AVeyraPlayerController& Controller, const FVeyraChatCommand& Command)
{
	// The party and friends are the backend's, which the client flow reaches; a game without a launcher has none.
	const UGameInstance* Game = Controller.GetGameInstance();
	const UVeyraClientFlowSubsystem* Flow = Game ? Game->GetSubsystem<UVeyraClientFlowSubsystem>() : nullptr;
	if (!Flow)
	{
		Controller.NoteChat(EVeyraChatNotice::OutsideUnavailable, FString());
		return;
	}
	IVeyraClientIntents& Client = Flow->GetClient();
	const FVeyraClientSnapshot& Snapshot = Client.GetSnapshot();
	FString Target;
	VeyraBackendProtocol::EChatKind Kind = VeyraBackendProtocol::EChatKind::Direct;
	if (Command.Kind == EVeyraChatCommandKind::Party)
	{
		Kind = VeyraBackendProtocol::EChatKind::Party;
	}
	else if (Command.Kind == EVeyraChatCommandKind::Reply)
	{
		Target = Snapshot.Chat.LastDirectFrom;
		if (Target.IsEmpty())
		{
			Controller.NoteChat(EVeyraChatNotice::NoReplyTarget, FString());
			return;
		}
	}
	else
	{
		const VeyraBackendProtocol::FAccount* Friend = Snapshot.Social.Friends.Friends.FindByPredicate(
			[&Command](const VeyraBackendProtocol::FAccount& Account) { return Account.DisplayName.Equals(Command.Name, ESearchCase::IgnoreCase); });
		if (!Friend)
		{
			Controller.NoteChat(EVeyraChatNotice::NoSuchFriend, Command.Name);
			return;
		}
		Target = Friend->Id;
	}
	// The line shows as sending, then sent or why not, in the chat log; nothing pops up (ADR-046 §6).
	if (!Client.SendChatMessage(Kind, Target, Command.Text))
	{
		Controller.NoteChat(EVeyraChatNotice::OutsideUnavailable, FString());
	}
}

void UVeyraMatchMenuSubsystem::SetLoadingScreen(UVeyraLoadingScreen* Screen)
{
	LoadingScreen = Screen;
	UpdateInputMode();
}

void UVeyraMatchMenuSubsystem::UpdateInputMode()
{
	AVeyraPlayerController* Controller = BoundController.Get();
	if (!Controller)
	{
		return;
	}
	// The cursor stays in the window during a match unless the player lets it go (SET-83).
	const EMouseLockMode Lock = Preferences().bConfineCursor ? EMouseLockMode::LockAlways : EMouseLockMode::DoNotLock;
	if (Menu || Shop || Settings || Chat)
	{
		// The menu, Settings and the chat composer take the keyboard; the shop leaves it to the game, so
		// abilities and items still work.
		FInputModeGameAndUI Mode;
		if (Settings)
		{
			Mode.SetWidgetToFocus(Settings->TakeWidget());
		}
		else if (Menu)
		{
			Mode.SetWidgetToFocus(Menu->TakeWidget());
		}
		else if (Chat)
		{
			Mode.SetWidgetToFocus(Chat->GetFocusTarget());
		}
		Mode.SetHideCursorDuringCapture(false);
		Mode.SetLockMouseToViewportBehavior(Lock);
		Controller->SetInputMode(Mode);
	}
	else if (UVeyraLoadingScreen* Loading = LoadingScreen.Get(); Loading && Loading->IsInViewport())
	{
		// Nothing to play yet: the loading screen's Previous and Next take the keyboard (SET-117).
		FInputModeGameAndUI Mode;
		Mode.SetWidgetToFocus(Loading->GetFocusTarget());
		Mode.SetHideCursorDuringCapture(false);
		Mode.SetLockMouseToViewportBehavior(Lock);
		Controller->SetInputMode(Mode);
	}
	else
	{
		FInputModeGameOnly Mode;
		Mode.SetConsumeCaptureMouseDown(false);
		Controller->SetInputMode(Mode);
		if (UGameViewportClient* Viewport = Controller->GetLocalPlayer() ? Controller->GetLocalPlayer()->ViewportClient.Get() : nullptr)
		{
			Viewport->SetMouseLockMode(Lock);
		}
	}
}
