// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Tuning/VeyraTuning.h"

#include "Containers/StringConv.h"
#include "JsonUtils/RapidJsonUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Optional.h"
#include "Misc/Paths.h"
#include "UObject/Class.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"
#include "VeyraCoreLog.h"

namespace VeyraTuning
{
namespace
{
	using UE::Json::FDocument;
	using UE::Json::FValue;

	// Documents are TCHAR text; RapidJSON reads them as UTF-16 in the platform's byte order.
#if PLATFORM_LITTLE_ENDIAN
	using FSourceEncoding = rapidjson::UTF16LE<TCHAR>;
#else
	using FSourceEncoding = rapidjson::UTF16BE<TCHAR>;
#endif

	// Strict JSON: no comments, trailing commas, NaN or Infinity, and exact decimal conversion.
	// (UE::Json::Parse is not used because its default flags accept trailing commas.)
	constexpr unsigned StrictParseFlags = rapidjson::kParseFullPrecisionFlag;

	// The root field that versions a domain's tuning format. The domain checks it; nothing binds it.
	const TCHAR* const SchemaVersionKey = TEXT("schemaVersion");

	// The JSON Schema draft-04 keywords of the tuning dialect (Game/Tuning/README.md), by schema type.
	const TCHAR* const CommonKeywords[] = { TEXT("$schema"), TEXT("title"), TEXT("description"), TEXT("type") };
	const TCHAR* const ObjectKeywords[] = { TEXT("properties"), TEXT("required"), TEXT("additionalProperties") };
	const TCHAR* const NumberKeywords[] = { TEXT("minimum"), TEXT("maximum"), TEXT("exclusiveMinimum"), TEXT("exclusiveMaximum"), TEXT("enum") };

	bool IsOneOf(FStringView Name, TConstArrayView<const TCHAR*> Names)
	{
		return Names.ContainsByPredicate([Name](const TCHAR* Candidate) { return Name.Equals(Candidate, ESearchCase::CaseSensitive); });
	}

	bool ContainsExactly(const TArray<FString>& Names, FStringView Name)
	{
		return Names.ContainsByPredicate([Name](const FString& Candidate) { return Name.Equals(Candidate, ESearchCase::CaseSensitive); });
	}

	FString NameOf(const FValue& StringValue)
	{
		return FString(UE::Json::ValueAsStringView(StringValue));
	}

	const FValue* FindMember(const FValue& Object, const TCHAR* Name)
	{
		const FValue::ConstMemberIterator It = Object.FindMember(Name);
		return It != Object.MemberEnd() ? &It->value : nullptr;
	}

	FString ChildPointer(const FString& Parent, FStringView Key)
	{
		FString Escaped(Key);
		Escaped.ReplaceInline(TEXT("~"), TEXT("~0"), ESearchCase::CaseSensitive);
		Escaped.ReplaceInline(TEXT("/"), TEXT("~1"), ESearchCase::CaseSensitive);
		return Parent + TEXT("/") + Escaped;
	}

	FString PointerText(const FString& Pointer)
	{
		return Pointer.IsEmpty() ? FString(TEXT("(root)")) : Pointer;
	}

	// A struct's UPROPERTY "MitigationConstant" is the JSON key "mitigationConstant".
	FString JsonKeyOf(const FProperty& Property)
	{
		FString Key = Property.GetName();
		if (!Key.IsEmpty())
		{
			Key[0] = FChar::ToLower(Key[0]);
		}
		return Key;
	}

	void CheckTextHygiene(FStringView Text, const TCHAR* Label, FErrors& Errors)
	{
		constexpr TCHAR ByteOrderMark = TCHAR(0xFEFF);
		if (!Text.IsEmpty() && Text[0] == ByteOrderMark)
		{
			Errors.Add(FString::Printf(TEXT("%s: starts with a byte-order mark; save it as UTF-8 without one"), Label));
		}
		int32 Index = INDEX_NONE;
		if (Text.FindChar(TEXT('\r'), Index))
		{
			Errors.Add(FString::Printf(TEXT("%s: contains a carriage return; tuning files use LF line endings"), Label));
		}
	}

	bool ParseStrict(FStringView Text, FDocument& OutDocument, const TCHAR* Label, FErrors& Errors)
	{
		OutDocument.Parse<StrictParseFlags, FSourceEncoding>(Text.GetData(), static_cast<size_t>(Text.Len()));
		if (!OutDocument.HasParseError())
		{
			return true;
		}
		const UE::Json::FParseError ParseError{ OutDocument.GetParseError(), OutDocument.GetErrorOffset() / sizeof(TCHAR) };
		Errors.Add(FString::Printf(TEXT("%s: %s"), Label, *ParseError.CreateMessage(Text)));
		return false;
	}

	/** The numeric constraints of one number or integer schema. */
	struct FNumberRule
	{
		double Minimum = 0.0;
		bool bExclusiveMinimum = false;
		TOptional<double> Maximum;
		bool bExclusiveMaximum = false;
		TArray<double> AllowedValues;
	};

	/** Walks a schema, a document and a reflected struct together, collecting every error. */
	class FWalker
	{
	public:
		explicit FWalker(FErrors& InErrors)
			: Errors(InErrors)
		{
		}

		/** Document may be null when it is already missing; the schema is still checked. */
		void WalkObject(const FValue& Schema, const FString& SchemaPointer, const FValue* Document, const FString& Pointer,
			const UStruct& Struct, void* Memory, TOptional<int32> ExpectedSchemaVersion)
		{
			if (!CheckKeywords(Schema, SchemaPointer, ObjectKeywords))
			{
				return;
			}
			if (!TypeIs(Schema, SchemaPointer, TEXT("object")))
			{
				return;
			}

			const FValue* Properties = FindMember(Schema, TEXT("properties"));
			const FValue* Required = FindMember(Schema, TEXT("required"));
			const FValue* Additional = FindMember(Schema, TEXT("additionalProperties"));
			if (!Properties || !Properties->IsObject())
			{
				SchemaError(SchemaPointer, TEXT("an object schema must declare \"properties\" as an object"));
				return;
			}
			if (!Additional || !Additional->IsBool() || Additional->GetBool())
			{
				SchemaError(SchemaPointer, TEXT("an object schema must declare \"additionalProperties\": false"));
			}

			TArray<FString> PropertyNames;
			for (const FValue::Member& Member : Properties->GetObject())
			{
				PropertyNames.Add(NameOf(Member.name));
			}
			CheckRequiredListsEveryProperty(Required, SchemaPointer, PropertyNames);

			const bool bIsRoot = ExpectedSchemaVersion.IsSet();
			if (bIsRoot && !ContainsExactly(PropertyNames, SchemaVersionKey))
			{
				SchemaError(ChildPointer(SchemaPointer, TEXT("properties")), TEXT("the root must declare \"schemaVersion\""));
			}

			// Every UPROPERTY must be described, so nothing is left at its default.
			TArray<const FProperty*> StructProperties;
			for (TFieldIterator<FProperty> It(&Struct); It; ++It)
			{
				StructProperties.Add(*It);
				if (!ContainsExactly(PropertyNames, JsonKeyOf(**It)))
				{
					SchemaError(ChildPointer(SchemaPointer, TEXT("properties")), FString::Printf(TEXT("has no property \"%s\" for %s::%s"),
						*JsonKeyOf(**It), *Struct.GetName(), *It->GetName()));
				}
			}

			if (Document)
			{
				if (!Document->IsObject())
				{
					DocumentError(Pointer, FString::Printf(TEXT("expected an object, found %s"), UE::Json::GetValueTypeName(*Document)));
					Document = nullptr;
				}
				else
				{
					TArray<FString> SeenKeys;
					for (const FValue::Member& Member : Document->GetObject())
					{
						const FString Key = NameOf(Member.name);
						if (ContainsExactly(SeenKeys, Key))
						{
							DocumentError(ChildPointer(Pointer, Key), TEXT("duplicate key"));
						}
						SeenKeys.Add(Key);
						if (!ContainsExactly(PropertyNames, Key))
						{
							DocumentError(ChildPointer(Pointer, Key), TEXT("unknown field"));
						}
					}
				}
			}

			for (const FValue::Member& Member : Properties->GetObject())
			{
				const FString Name = NameOf(Member.name);
				const FString PropertySchemaPointer = ChildPointer(ChildPointer(SchemaPointer, TEXT("properties")), Name);
				const FString PropertyPointer = ChildPointer(Pointer, Name);
				const FValue* Value = Document ? FindMember(*Document, *Name) : nullptr;
				if (Document && !Value)
				{
					DocumentError(PropertyPointer, TEXT("missing required field"));
				}

				if (bIsRoot && Name.Equals(SchemaVersionKey, ESearchCase::CaseSensitive))
				{
					WalkSchemaVersion(Member.value, PropertySchemaPointer, Value, PropertyPointer, ExpectedSchemaVersion.GetValue());
					continue;
				}

				const FProperty* const* Property = StructProperties.FindByPredicate([&Name](const FProperty* Candidate)
				{
					return JsonKeyOf(*Candidate).Equals(Name, ESearchCase::CaseSensitive);
				});
				if (!Property)
				{
					SchemaError(PropertySchemaPointer, FString::Printf(TEXT("has no matching UPROPERTY in %s"), *Struct.GetName()));
					continue;
				}
				WalkValue(Member.value, PropertySchemaPointer, Value, PropertyPointer, **Property, Memory);
			}
		}

	private:
		void SchemaError(const FString& SchemaPointer, const FString& Message)
		{
			Errors.Add(FString::Printf(TEXT("schema %s: %s"), *PointerText(SchemaPointer), *Message));
		}

		void DocumentError(const FString& Pointer, const FString& Message)
		{
			Errors.Add(FString::Printf(TEXT("%s: %s"), *PointerText(Pointer), *Message));
		}

		/** Rejects keywords outside the dialect or outside the schema's type. */
		bool CheckKeywords(const FValue& Schema, const FString& SchemaPointer, TConstArrayView<const TCHAR*> TypeKeywords)
		{
			if (!Schema.IsObject())
			{
				SchemaError(SchemaPointer, TEXT("must be an object"));
				return false;
			}
			for (const FValue::Member& Member : Schema.GetObject())
			{
				const FString Keyword = NameOf(Member.name);
				if (!IsOneOf(Keyword, CommonKeywords) && !IsOneOf(Keyword, TypeKeywords))
				{
					SchemaError(SchemaPointer, FString::Printf(TEXT("keyword \"%s\" is not supported here"), *Keyword));
				}
			}
			return true;
		}

		bool TypeIs(const FValue& Schema, const FString& SchemaPointer, const TCHAR* Expected)
		{
			const FValue* Type = FindMember(Schema, TEXT("type"));
			if (!Type || !Type->IsString())
			{
				SchemaError(SchemaPointer, TEXT("must declare \"type\" as a string"));
				return false;
			}
			if (!NameOf(*Type).Equals(Expected, ESearchCase::CaseSensitive))
			{
				SchemaError(SchemaPointer, FString::Printf(TEXT("expected \"type\": \"%s\""), Expected));
				return false;
			}
			return true;
		}

		void CheckRequiredListsEveryProperty(const FValue* Required, const FString& SchemaPointer, const TArray<FString>& PropertyNames)
		{
			const FString RequiredPointer = ChildPointer(SchemaPointer, TEXT("required"));
			if (!Required || !Required->IsArray())
			{
				SchemaError(RequiredPointer, TEXT("must be an array listing every property; no field is optional"));
				return;
			}
			TArray<FString> RequiredNames;
			for (const FValue& Entry : Required->GetArray())
			{
				if (!Entry.IsString())
				{
					SchemaError(RequiredPointer, TEXT("must contain only strings"));
					continue;
				}
				RequiredNames.Add(NameOf(Entry));
			}
			for (const FString& Name : PropertyNames)
			{
				if (!ContainsExactly(RequiredNames, Name))
				{
					SchemaError(RequiredPointer, FString::Printf(TEXT("must list \"%s\"; no field is optional"), *Name));
				}
			}
			for (const FString& Name : RequiredNames)
			{
				if (!ContainsExactly(PropertyNames, Name))
				{
					SchemaError(RequiredPointer, FString::Printf(TEXT("lists \"%s\", which is not a property"), *Name));
				}
			}
		}

		void WalkValue(const FValue& Schema, const FString& SchemaPointer, const FValue* Value, const FString& Pointer,
			const FProperty& Property, void* ContainerMemory)
		{
			if (!Schema.IsObject())
			{
				SchemaError(SchemaPointer, TEXT("must be an object"));
				return;
			}
			const FValue* Type = FindMember(Schema, TEXT("type"));
			const FString TypeName = (Type && Type->IsString()) ? NameOf(*Type) : FString();

			if (TypeName.Equals(TEXT("object"), ESearchCase::CaseSensitive))
			{
				const FStructProperty* StructProperty = CastField<FStructProperty>(&Property);
				if (!StructProperty)
				{
					SchemaError(SchemaPointer, FString::Printf(TEXT("is an object, but %s is not a struct"), *Property.GetName()));
					return;
				}
				WalkObject(Schema, SchemaPointer, Value, Pointer, *StructProperty->Struct,
					StructProperty->ContainerPtrToValuePtr<void>(ContainerMemory), TOptional<int32>());
				return;
			}

			const bool bInteger = TypeName.Equals(TEXT("integer"), ESearchCase::CaseSensitive);
			const bool bNumber = TypeName.Equals(TEXT("number"), ESearchCase::CaseSensitive);
			if (!bInteger && !bNumber)
			{
				CheckKeywords(Schema, SchemaPointer, NumberKeywords);
				SchemaError(SchemaPointer, TypeName.IsEmpty()
					? FString(TEXT("must declare \"type\" as a string"))
					: FString::Printf(TEXT("type \"%s\" is not supported"), *TypeName));
				return;
			}
			CheckKeywords(Schema, SchemaPointer, NumberKeywords);

			const FDoubleProperty* DoubleProperty = CastField<FDoubleProperty>(&Property);
			const FIntProperty* IntProperty = CastField<FIntProperty>(&Property);
			if (bNumber && !DoubleProperty)
			{
				SchemaError(SchemaPointer, FString::Printf(TEXT("is a number, so %s must be a double"), *Property.GetName()));
				return;
			}
			if (bInteger && !IntProperty)
			{
				SchemaError(SchemaPointer, FString::Printf(TEXT("is an integer, so %s must be an int32"), *Property.GetName()));
				return;
			}

			TOptional<FNumberRule> Rule = ReadNumberRule(Schema, SchemaPointer, bInteger);
			if (!Value)
			{
				return;
			}
			TOptional<double> Number = ReadNumber(*Value, Pointer, bInteger);
			if (!Number.IsSet() || !Rule.IsSet() || !SatisfiesRule(Number.GetValue(), Rule.GetValue(), Pointer))
			{
				return;
			}

			if (bInteger)
			{
				const int64 Integer = Value->GetInt64();
				if (Integer < MIN_int32 || Integer > MAX_int32)
				{
					DocumentError(Pointer, TEXT("does not fit in a 32-bit integer"));
					return;
				}
				IntProperty->SetPropertyValue_InContainer(ContainerMemory, static_cast<int32>(Integer));
			}
			else
			{
				DoubleProperty->SetPropertyValue_InContainer(ContainerMemory, Number.GetValue());
			}
		}

		void WalkSchemaVersion(const FValue& Schema, const FString& SchemaPointer, const FValue* Value, const FString& Pointer, int32 Expected)
		{
			if (!CheckKeywords(Schema, SchemaPointer, NumberKeywords) || !TypeIs(Schema, SchemaPointer, TEXT("integer")))
			{
				return;
			}
			TOptional<FNumberRule> Rule = ReadNumberRule(Schema, SchemaPointer, true);
			if (Rule.IsSet() && (Rule->AllowedValues.Num() != 1 || Rule->AllowedValues[0] != static_cast<double>(Expected)))
			{
				SchemaError(SchemaPointer, FString::Printf(TEXT("must declare \"enum\": [%d], the version this build reads"), Expected));
			}
			if (!Value)
			{
				return;
			}
			TOptional<double> Number = ReadNumber(*Value, Pointer, true);
			if (Number.IsSet() && Value->GetInt64() != Expected)
			{
				DocumentError(Pointer, FString::Printf(TEXT("is %lld, but this build reads version %d"), Value->GetInt64(), Expected));
			}
		}

		TOptional<FNumberRule> ReadNumberRule(const FValue& Schema, const FString& SchemaPointer, bool bInteger)
		{
			FNumberRule Rule;
			bool bValid = true;

			const FValue* Minimum = FindMember(Schema, TEXT("minimum"));
			if (!Minimum || !Minimum->IsNumber())
			{
				SchemaError(SchemaPointer, TEXT("every number must declare \"minimum\""));
				bValid = false;
			}
			else
			{
				Rule.Minimum = Minimum->GetDouble();
			}

			if (const FValue* Maximum = FindMember(Schema, TEXT("maximum")))
			{
				if (Maximum->IsNumber())
				{
					Rule.Maximum = Maximum->GetDouble();
				}
				else
				{
					SchemaError(SchemaPointer, TEXT("\"maximum\" must be a number"));
					bValid = false;
				}
			}

			bValid &= ReadFlag(Schema, SchemaPointer, TEXT("exclusiveMinimum"), Rule.bExclusiveMinimum);
			bValid &= ReadFlag(Schema, SchemaPointer, TEXT("exclusiveMaximum"), Rule.bExclusiveMaximum);
			if (Rule.bExclusiveMaximum && !Rule.Maximum.IsSet())
			{
				SchemaError(SchemaPointer, TEXT("\"exclusiveMaximum\" needs \"maximum\""));
				bValid = false;
			}

			if (const FValue* Enum = FindMember(Schema, TEXT("enum")))
			{
				if (!Enum->IsArray() || Enum->Empty())
				{
					SchemaError(SchemaPointer, TEXT("\"enum\" must be a non-empty array"));
					bValid = false;
				}
				else
				{
					for (const FValue& Entry : Enum->GetArray())
					{
						if (!Entry.IsNumber() || (bInteger && !Entry.IsInt64()))
						{
							SchemaError(SchemaPointer, bInteger ? TEXT("\"enum\" must contain only integers") : TEXT("\"enum\" must contain only numbers"));
							bValid = false;
							break;
						}
						Rule.AllowedValues.Add(Entry.GetDouble());
					}
				}
			}
			return bValid ? TOptional<FNumberRule>(Rule) : TOptional<FNumberRule>();
		}

		bool ReadFlag(const FValue& Schema, const FString& SchemaPointer, const TCHAR* Keyword, bool& OutFlag)
		{
			const FValue* Flag = FindMember(Schema, Keyword);
			if (!Flag)
			{
				return true;
			}
			if (!Flag->IsBool())
			{
				SchemaError(SchemaPointer, FString::Printf(TEXT("\"%s\" must be true or false (draft-04)"), Keyword));
				return false;
			}
			OutFlag = Flag->GetBool();
			return true;
		}

		TOptional<double> ReadNumber(const FValue& Value, const FString& Pointer, bool bInteger)
		{
			if (!Value.IsNumber())
			{
				DocumentError(Pointer, FString::Printf(TEXT("expected %s, found %s"), bInteger ? TEXT("an integer") : TEXT("a number"),
					UE::Json::GetValueTypeName(Value)));
				return {};
			}
			if (bInteger && !Value.IsInt64())
			{
				DocumentError(Pointer, TEXT("expected an integer, found a number with a fraction or exponent"));
				return {};
			}
			return Value.GetDouble();
		}

		bool SatisfiesRule(double Number, const FNumberRule& Rule, const FString& Pointer)
		{
			bool bSatisfied = true;
			if (Rule.bExclusiveMinimum ? Number <= Rule.Minimum : Number < Rule.Minimum)
			{
				DocumentError(Pointer, FString::Printf(TEXT("must be %s %s"), Rule.bExclusiveMinimum ? TEXT("greater than") : TEXT("at least"),
					*FString::SanitizeFloat(Rule.Minimum, 0)));
				bSatisfied = false;
			}
			if (Rule.Maximum.IsSet() && (Rule.bExclusiveMaximum ? Number >= Rule.Maximum.GetValue() : Number > Rule.Maximum.GetValue()))
			{
				DocumentError(Pointer, FString::Printf(TEXT("must be %s %s"), Rule.bExclusiveMaximum ? TEXT("less than") : TEXT("at most"),
					*FString::SanitizeFloat(Rule.Maximum.GetValue(), 0)));
				bSatisfied = false;
			}
			if (!Rule.AllowedValues.IsEmpty() && !Rule.AllowedValues.Contains(Number))
			{
				DocumentError(Pointer, TEXT("is not one of the values the schema's \"enum\" allows"));
				bSatisfied = false;
			}
			return bSatisfied;
		}

		FErrors& Errors;
	};
}

FErrors ValidateAndBind(FStringView DocumentText, FStringView SchemaText, int32 ExpectedSchemaVersion, const UScriptStruct& Struct, void* OutStruct)
{
	FErrors Errors;
	CheckTextHygiene(SchemaText, TEXT("schema"), Errors);
	CheckTextHygiene(DocumentText, TEXT("document"), Errors);

	FDocument Schema;
	FDocument Document;
	const bool bSchemaParsed = ParseStrict(SchemaText, Schema, TEXT("schema"), Errors);
	const bool bDocumentParsed = ParseStrict(DocumentText, Document, TEXT("document"), Errors);
	if (!bSchemaParsed || !bDocumentParsed)
	{
		return Errors;
	}

	// Bind into scratch memory so OutStruct never holds a partly valid result.
	FStructOnScope Scratch(&Struct);
	FWalker(Errors).WalkObject(Schema, FString(), &Document, FString(), Struct, Scratch.GetStructMemory(), ExpectedSchemaVersion);
	if (Errors.IsEmpty())
	{
		Struct.CopyScriptStruct(OutStruct, Scratch.GetStructMemory());
	}
	return Errors;
}

FString DecodeUtf8(TConstArrayView<uint8> Bytes)
{
	const auto Converted = StringCast<TCHAR>(reinterpret_cast<const UTF8CHAR*>(Bytes.GetData()), Bytes.Num());
	return FString::ConstructFromPtrSize(Converted.Get(), Converted.Length());
}

FErrors ReadDomainFiles(FStringView Domain, FDomainFiles& OutFiles)
{
	FErrors Errors;
	const FString DomainName(Domain);
	const FString TuningDir = FPaths::Combine(FPaths::ProjectDir(), TEXT("Tuning"));
	const FString DocumentPath = FPaths::Combine(TuningDir, DomainName + TEXT(".json"));
	const FString SchemaPath = FPaths::Combine(TuningDir, TEXT("Schemas"), DomainName + TEXT(".schema.json"));

	TArray<uint8> DocumentBytes;
	TArray<uint8> SchemaBytes;
	if (!FFileHelper::LoadFileToArray(DocumentBytes, *DocumentPath, FILEREAD_Silent))
	{
		Errors.Add(FString::Printf(TEXT("%s: cannot be read"), *DocumentPath));
	}
	if (!FFileHelper::LoadFileToArray(SchemaBytes, *SchemaPath, FILEREAD_Silent))
	{
		Errors.Add(FString::Printf(TEXT("%s: cannot be read"), *SchemaPath));
	}
	if (Errors.IsEmpty())
	{
		OutFiles.DocumentText = DecodeUtf8(DocumentBytes);
		OutFiles.SchemaText = DecodeUtf8(SchemaBytes);
		OutFiles.DocumentHash = FBlake3::HashBuffer(DocumentBytes.GetData(), DocumentBytes.Num());
	}
	return Errors;
}

namespace
{
	TMap<FString, FBlake3Hash>& LoadedDomainHashes()
	{
		static TMap<FString, FBlake3Hash> Hashes;
		return Hashes;
	}
}

void RecordLoadedDomain(FStringView Domain, const FBlake3Hash& DocumentHash)
{
	check(IsInGameThread());
	const FString DomainName(Domain);
	LoadedDomainHashes().Add(DomainName, DocumentHash);
	UE_LOG(LogVeyraCore, Log, TEXT("Tuning %s loaded, BLAKE3 %s."), *DomainName, *LexToString(DocumentHash));
}

TMap<FString, FBlake3Hash> GetLoadedDomainHashes()
{
	check(IsInGameThread());
	return LoadedDomainHashes();
}

FBlake3Hash GetCompositeHash()
{
	check(IsInGameThread());
	TArray<FString> Domains;
	LoadedDomainHashes().GetKeys(Domains);
	Domains.Sort();

	FString Lines;
	for (const FString& DomainName : Domains)
	{
		Lines += FString::Printf(TEXT("%s=%s\n"), *DomainName, *LexToString(LoadedDomainHashes().FindChecked(DomainName)));
	}
	const FTCHARToUTF8 Utf8(*Lines);
	return FBlake3::HashBuffer(Utf8.Get(), Utf8.Length());
}

void ReportLoadFailure(FStringView Domain, const FErrors& Errors)
{
	const FString DomainName(Domain);
	for (const FString& Error : Errors)
	{
		UE_LOG(LogVeyraCore, Error, TEXT("Tuning %s: %s"), *DomainName, *Error);
	}
#if !WITH_EDITOR
	UE_LOG(LogVeyraCore, Fatal, TEXT("Tuning %s failed to load with %d error(s); see the errors above."), *DomainName, Errors.Num());
#endif
}
}
