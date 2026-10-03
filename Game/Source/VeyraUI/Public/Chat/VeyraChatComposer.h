// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "Chat/VeyraChatTypes.h"

#include "VeyraChatComposer.generated.h"

class UEditableTextBox;
class UTextBlock;

/**
 * The line a player types chat into, over the match at the bottom left, under the chat log (ADR-029
 * §5). Enter sends, Escape closes without sending, and Tab switches between Team and All. While it is
 * open, typed keys never reach the game, and a click on the battleground leaves the keyboard with it.
 */
UCLASS()
class VEYRAUI_API UVeyraChatComposer : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;

	/**
	 * Opens on Channel, taking at most MaxCharacters. Submit takes the channel and the typed line as the
	 * player sends it; Close runs after a send, and when the player closes it without one.
	 */
	void Show(EVeyraChatChannel InChannel, int32 InMaxCharacters, TFunction<void(EVeyraChatChannel, const FString&)> InSubmit, TFunction<void()> InClose);

	EVeyraChatChannel GetChannel() const { return Channel; }

	/** Switches between Team and All, as Tab does. */
	void SwitchChannel();

	FString GetTyped() const;

	/** Its field's type size, at the player's Chat Text Size (ADR-059 §5). */
	int32 GetFieldFontSize() const;
	void SetTyped(const FString& Text);

	/** Sends the typed line, as Enter does, and closes. */
	void Send();

	/** Closes without sending, as Escape does. */
	void Cancel();

	/** The widget that takes the keyboard while the composer is open. */
	TSharedRef<SWidget> GetFocusTarget();

	/** Places the composer where the HUD's chat frame puts it, on its player's screen. */
	void Place();

	/** The channel as the composer names it. */
	static FText ChannelLabel(EVeyraChatChannel Channel);

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	UFUNCTION()
	void HandleTextChanged(const FText& Text);

	UPROPERTY(Transient)
	TObjectPtr<UEditableTextBox> Field;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Label;

	TFunction<void(EVeyraChatChannel, const FString&)> Submit;
	TFunction<void()> Close;
	EVeyraChatChannel Channel = EVeyraChatChannel::Team;
	int32 MaxCharacters = 0;
	/** The chat type size its field was last styled at, so Chat Text Size restyles it only when it changes. */
	int32 SizedForChat = 0;
	/** Where it was last placed, so it moves only when the screen or the player's HUD settings move it. */
	FBox2D PlacedAt = FBox2D(ForceInit);
};
