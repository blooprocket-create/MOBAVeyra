// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Backend/VeyraChatProtocol.h"

#include "Dom/JsonObject.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace VeyraBackendProtocol
{
namespace
{
	/** The largest whole number a JSON double carries exactly: 2^53. */
	constexpr double ChatMaxExactWhole = 9007199254740992.0;

	TSharedPtr<FJsonObject> ChatParseBody(const FString& Body)
	{
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<TCHAR>::Create(Body);
		return FJsonSerializer::Deserialize(Reader, Root) ? Root : nullptr;
	}

	const FJsonObject* ChatChild(const FJsonObject& Object, const TCHAR* Name)
	{
		const TSharedPtr<FJsonObject>* Child = nullptr;
		return Object.HasTypedField<EJson::Object>(Name) && Object.TryGetObjectField(Name, Child) && Child->IsValid() ? Child->Get() : nullptr;
	}

	/** A whole, non-negative sequence. */
	bool ChatSequence(const FJsonObject& Object, const TCHAR* Name, int64& Out)
	{
		double Value = 0.0;
		if (!Object.HasTypedField<EJson::Number>(Name) || !Object.TryGetNumberField(Name, Value) || !FMath::IsFinite(Value) || Value < 0.0
			|| Value != FMath::FloorToDouble(Value) || Value > ChatMaxExactWhole)
		{
			return false;
		}
		Out = static_cast<int64>(Value);
		return true;
	}

	bool ChatText(const FJsonObject& Object, const TCHAR* Name, FString& Out, bool bAllowEmpty = false)
	{
		return Object.HasTypedField<EJson::String>(Name) && Object.TryGetStringField(Name, Out) && (bAllowEmpty || !Out.IsEmpty());
	}

	bool ChatKindFrom(const FString& Name, EChatKind& Out)
	{
		for (const EChatKind Kind : { EChatKind::Party, EChatKind::Direct, EChatKind::Select, EChatKind::PostMatch })
		{
			if (Name == ChatKindName(Kind))
			{
				Out = Kind;
				return true;
			}
		}
		return false;
	}

	bool ReadChatMessage(const FJsonObject& Object, FChatMessage& Out, FString& Problem)
	{
		FString Kind;
		const FJsonObject* Sender = ChatChild(Object, TEXT("sender"));
		if (!ChatSequence(Object, TEXT("seq"), Out.Seq) || Out.Seq == 0 || !ChatText(Object, TEXT("kind"), Kind) || !ChatKindFrom(Kind, Out.Kind)
			|| !ChatText(Object, TEXT("conversation"), Out.Conversation) || !Sender || !ChatText(*Sender, TEXT("id"), Out.SenderId)
			|| !ChatText(*Sender, TEXT("displayName"), Out.SenderName, true) || !ChatText(Object, TEXT("text"), Out.Text)
			|| !ChatText(Object, TEXT("clientId"), Out.ClientId))
		{
			Problem = TEXT("a message lacks its sequence, kind, conversation, sender, text or client ID");
			return false;
		}
		// A direct message names its recipient; no other kind has one.
		Out.RecipientId.Reset();
		if (Out.Kind == EChatKind::Direct ? !ChatText(Object, TEXT("recipientId"), Out.RecipientId) : !Object.HasTypedField<EJson::Null>(TEXT("recipientId")))
		{
			Problem = TEXT("a message's recipient does not fit its kind");
			return false;
		}
		return true;
	}
}

const TCHAR* ChatKindName(EChatKind Kind)
{
	switch (Kind)
	{
	case EChatKind::Party:
		return TEXT("party");
	case EChatKind::Direct:
		return TEXT("direct");
	case EChatKind::Select:
		return TEXT("select");
	case EChatKind::PostMatch:
		return TEXT("postmatch");
	}
	return TEXT("party");
}

bool ParseChatPage(const FString& Body, FChatPage& Out, FString& Problem)
{
	const TSharedPtr<FJsonObject> Root = ChatParseBody(Body);
	const TArray<TSharedPtr<FJsonValue>>* Messages = nullptr;
	FChatPage Page;
	if (!Root.IsValid() || !Root->HasTypedField<EJson::Array>(TEXT("messages")) || !Root->TryGetArrayField(TEXT("messages"), Messages)
		|| !ChatSequence(*Root, TEXT("next"), Page.Next) || !Root->HasTypedField<EJson::Boolean>(TEXT("more")) || !Root->TryGetBoolField(TEXT("more"), Page.bMore))
	{
		Problem = TEXT("the chat page lacks its messages, cursor or more flag");
		return false;
	}
	int64 Previous = 0;
	for (const TSharedPtr<FJsonValue>& Value : *Messages)
	{
		const TSharedPtr<FJsonObject>* Object = nullptr;
		FChatMessage Message;
		if (!Value.IsValid() || !Value->TryGetObject(Object) || !Object->IsValid() || !ReadChatMessage(**Object, Message, Problem))
		{
			if (Problem.IsEmpty())
			{
				Problem = TEXT("a chat message is not an object");
			}
			return false;
		}
		// Oldest first, and never past the cursor the page hands back.
		if (Message.Seq <= Previous || Message.Seq > Page.Next)
		{
			Problem = TEXT("the chat page's messages are out of order or past its cursor");
			return false;
		}
		Previous = Message.Seq;
		Page.Messages.Add(MoveTemp(Message));
	}
	Out = MoveTemp(Page);
	return true;
}

bool ParseChatSent(const FString& Body, FChatMessage& Out, FString& Problem)
{
	const TSharedPtr<FJsonObject> Root = ChatParseBody(Body);
	const FJsonObject* Message = Root.IsValid() ? ChatChild(*Root, TEXT("message")) : nullptr;
	if (!Message)
	{
		Problem = TEXT("the send's answer lacks its message");
		return false;
	}
	return ReadChatMessage(*Message, Out, Problem);
}

FString BuildChatBody(const FString& ClientId, const FString& Text)
{
	FString Body;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Body);
	Writer->WriteObjectStart();
	Writer->WriteValue(TEXT("clientId"), ClientId);
	Writer->WriteValue(TEXT("text"), Text);
	Writer->WriteObjectEnd();
	Writer->Close();
	return Body;
}
}
