// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "VeyraSettingsDocument.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace VeyraSettingsDocument
{
namespace
{
	const TCHAR* const SchemaVersionField = TEXT("schemaVersion");
	const TCHAR* const RevisionField = TEXT("revision");
	const TCHAR* const ValuesField = TEXT("values");
	const TCHAR* const UnsentField = TEXT("unsent");
}

FString Write(const FVeyraAccountSettingsDocument& Document, bool bForCache)
{
	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(SchemaVersionField, SchemaVersion);
	Root->SetNumberField(RevisionField, static_cast<double>(Document.Revision));
	const TSharedRef<FJsonObject> Values = MakeShared<FJsonObject>();
	TArray<FString> Ids;
	Document.Values.GetKeys(Ids);
	Ids.Sort();
	for (const FString& Id : Ids)
	{
		Values->SetStringField(Id, Document.Values[Id]);
	}
	Root->SetObjectField(ValuesField, Values);
	if (bForCache)
	{
		Root->SetBoolField(UnsentField, Document.bUnsent);
	}
	FString Text;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Text);
	FJsonSerializer::Serialize(Root, Writer);
	return Text;
}

bool Read(FStringView Json, FVeyraAccountSettingsDocument& Out, FString& OutProblem)
{
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<TCHAR>::Create(FString(Json)), Root) || !Root.IsValid())
	{
		OutProblem = TEXT("not a JSON object");
		return false;
	}
	int32 Version = 0;
	if (!Root->TryGetNumberField(SchemaVersionField, Version) || Version != SchemaVersion)
	{
		OutProblem = FString::Printf(TEXT("schemaVersion must be %d"), SchemaVersion);
		return false;
	}
	int64 Revision = 0;
	if (!Root->TryGetNumberField(RevisionField, Revision) || Revision < 0)
	{
		OutProblem = TEXT("revision must be a number of at least 0");
		return false;
	}
	const TSharedPtr<FJsonObject>* Values = nullptr;
	if (!Root->TryGetObjectField(ValuesField, Values) || !Values || !Values->IsValid())
	{
		OutProblem = TEXT("values must be an object");
		return false;
	}
	FVeyraAccountSettingsDocument Document;
	Document.Revision = Revision;
	for (const auto& Entry : (*Values)->Values)
	{
		// Only text: a JSON number would read as text too, and a value is always text (FVeyraSettingsStore).
		FString Value;
		if (!Entry.Value.IsValid() || Entry.Value->Type != EJson::String || !Entry.Value->TryGetString(Value))
		{
			OutProblem = FString::Printf(TEXT("values/%s must be text"), *FString(Entry.Key));
			return false;
		}
		Document.Values.Add(FString(Entry.Key), Value);
	}
	Root->TryGetBoolField(UnsentField, Document.bUnsent);
	Out = MoveTemp(Document);
	return true;
}
}
