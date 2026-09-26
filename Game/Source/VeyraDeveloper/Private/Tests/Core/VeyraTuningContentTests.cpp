// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Content/VeyraContentId.h"
#include "CQTest.h"
#include "Tests/Core/VeyraTuningTestTypes.h"
#include "Tuning/VeyraTuning.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraCoreTests
{
	// Veyra.Core.ContentIdFormat.*: content IDs are lowercase ASCII snake_case (ADR-006 §6).
	TEST_CLASS(ContentIdFormat, "Veyra.Core")
	{
		TEST_METHOD(AcceptsSnakeCase)
		{
			for (const TCHAR* Text : { TEXT("a"), TEXT("test_bolt"), TEXT("a1_b2_c3"), TEXT("bolt2") })
			{
				ASSERT_THAT(IsTrue(FVeyraContentId::IsValidText(Text), Text));
			}
		}

		TEST_METHOD(RejectsEverythingElse)
		{
			for (const TCHAR* Text : { TEXT(""), TEXT("Test_Bolt"), TEXT("1bolt"), TEXT("_bolt"), TEXT("bolt_"), TEXT("test__bolt"),
				TEXT("test-bolt"), TEXT("test bolt"), TEXT("bolt\n"), TEXT("bölt") })
			{
				ASSERT_THAT(IsFalse(FVeyraContentId::IsValidText(Text), Text));
				ASSERT_THAT(IsFalse(FVeyraContentId::FromText(Text).IsSet(), Text));
			}
		}

		TEST_METHOD(EqualIdsHashAlike)
		{
			const FVeyraContentId First = FVeyraContentId::FromText(TEXT("test_bolt")).GetValue();
			const FVeyraContentId Second = FVeyraContentId::FromText(TEXT("test_bolt")).GetValue();
			ASSERT_THAT(IsTrue(First == Second));
			ASSERT_THAT(IsTrue(GetTypeHash(First) == GetTypeHash(Second)));
			ASSERT_THAT(AreEqual(First.ToString(), FString(TEXT("test_bolt"))));
			ASSERT_THAT(IsFalse(FVeyraContentId().IsValid()));
		}
	};

	// Veyra.Core.TuningContent.*: enum strings, content IDs and maps keyed by content ID bind
	// through the tuning framework, and their schemas must use the dialect's exact forms.
	TEST_CLASS(TuningContent, "Veyra.Core")
	{
		static constexpr int32 SchemaVersion = 1;

		// Matches FVeyraTuningContentTestShape and TestData/TuningConformanceContent.schema.json.
		const FString Schema = FString(
			TEXT("{\"type\": \"object\", \"additionalProperties\": false, \"required\": [\"schemaVersion\", \"kind\", \"target\", \"entries\"],")
			TEXT(" \"properties\": {\"schemaVersion\": {\"type\": \"integer\", \"minimum\": 1, \"enum\": [1]},")
			TEXT(" \"kind\": {\"type\": \"string\", \"enum\": [\"Alpha\", \"Beta\"]},")
			TEXT(" \"target\": {\"type\": \"string\", \"pattern\": \"^[a-z][a-z0-9]*(_[a-z0-9]+)*$\"},")
			TEXT(" \"entries\": {\"type\": \"object\", \"additionalProperties\": false, \"patternProperties\": {\"^[a-z][a-z0-9]*(_[a-z0-9]+)*$\":")
			TEXT(" {\"type\": \"object\", \"additionalProperties\": false, \"required\": [\"weight\"], \"properties\": {\"weight\": {\"type\": \"number\", \"minimum\": 0}}}}}}}"));

		const FString Document = FString(
			TEXT("{\"schemaVersion\": 1, \"kind\": \"Beta\", \"target\": \"test_bolt\", \"entries\": {\"first\": {\"weight\": 1.5}, \"second_2\": {\"weight\": 0}}}"));

		VeyraTuning::FErrors Bind(const FString& SchemaText, FVeyraTuningContentTestShape& Out) const
		{
			return VeyraTuning::ValidateAndBind(Document, SchemaText, SchemaVersion, Out);
		}

		VeyraTuning::FErrors BindWithSchema(const TCHAR* From, const TCHAR* To) const
		{
			FVeyraTuningContentTestShape Out;
			return Bind(Schema.Replace(From, To, ESearchCase::CaseSensitive), Out);
		}

		static bool HasError(const VeyraTuning::FErrors& Errors, const TCHAR* Fragment)
		{
			return Errors.ContainsByPredicate([Fragment](const FString& Error) { return Error.Contains(Fragment, ESearchCase::CaseSensitive); });
		}

		TEST_METHOD(BindsEnumsContentIdsAndMaps)
		{
			FVeyraTuningContentTestShape Out;
			const VeyraTuning::FErrors Errors = Bind(Schema, Out);
			ASSERT_THAT(IsTrue(Errors.IsEmpty(), FString::Join(Errors, TEXT(" | "))));
			ASSERT_THAT(IsTrue(Out.Kind == EVeyraTuningTestKind::Beta));
			ASSERT_THAT(AreEqual(Out.Target.ToString(), FString(TEXT("test_bolt"))));
			ASSERT_THAT(AreEqual(Out.Entries.Num(), 2));
			const FVeyraTuningTestEntry* First = Out.Entries.Find(FVeyraContentId::FromText(TEXT("first")).GetValue());
			const FVeyraTuningTestEntry* Second = Out.Entries.Find(FVeyraContentId::FromText(TEXT("second_2")).GetValue());
			ASSERT_THAT(IsTrue(First && First->Weight == 1.5));
			ASSERT_THAT(IsTrue(Second && Second->Weight == 0.0));
		}

		TEST_METHOD(EnumSchemaListsExactlyTheEnumValues)
		{
			const TCHAR* ListedValues = TEXT("[\"Alpha\", \"Beta\"]");
			ASSERT_THAT(IsTrue(HasError(BindWithSchema(ListedValues, TEXT("[\"Alpha\"]")), TEXT("listing exactly the values of EVeyraTuningTestKind"))));
			ASSERT_THAT(IsTrue(HasError(BindWithSchema(ListedValues, TEXT("[\"Alpha\", \"Beta\", \"Gamma\"]")), TEXT("listing exactly"))));
			ASSERT_THAT(IsTrue(HasError(BindWithSchema(ListedValues, TEXT("[\"alpha\", \"beta\"]")), TEXT("listing exactly"))));
			ASSERT_THAT(IsTrue(HasError(BindWithSchema(ListedValues, TEXT("[\"Alpha\", \"Alpha\"]")), TEXT("listing exactly"))));
		}

		TEST_METHOD(ContentIdSchemaUsesTheFormat)
		{
			const VeyraTuning::FErrors Errors = BindWithSchema(TEXT("\"target\": {\"type\": \"string\", \"pattern\": \"^[a-z][a-z0-9]*(_[a-z0-9]+)*$\"}"),
				TEXT("\"target\": {\"type\": \"string\", \"pattern\": \"^[a-z]+$\"}"));
			ASSERT_THAT(IsTrue(HasError(Errors, TEXT("the content ID format"))));
		}

		TEST_METHOD(MapSchemaUsesTheFormatAsItsOnlyKey)
		{
			const VeyraTuning::FErrors Errors = BindWithSchema(TEXT("\"patternProperties\": {\"^[a-z][a-z0-9]*(_[a-z0-9]+)*$\":"),
				TEXT("\"patternProperties\": {\"^.*$\":"));
			ASSERT_THAT(IsTrue(HasError(Errors, TEXT("one entry, the content ID format"))));
		}

		TEST_METHOD(MapValueSchemaMustMatchTheValueType)
		{
			// The value schema is checked even though the document's map is valid.
			const VeyraTuning::FErrors Errors = BindWithSchema(TEXT("{\"weight\": {\"type\": \"number\", \"minimum\": 0}}"),
				TEXT("{\"weight\": {\"type\": \"integer\", \"minimum\": 0}}"));
			ASSERT_THAT(IsTrue(HasError(Errors, TEXT("must be an int32"))));
		}

		TEST_METHOD(StringsBindOnlyToEnumsAndContentIds)
		{
			const VeyraTuning::FErrors Errors = BindWithSchema(TEXT("\"weight\": {\"type\": \"number\", \"minimum\": 0}"),
				TEXT("\"weight\": {\"type\": \"string\", \"pattern\": \"^[a-z][a-z0-9]*(_[a-z0-9]+)*$\"}"));
			ASSERT_THAT(IsTrue(HasError(Errors, TEXT("must be an enum class UENUM or an FVeyraContentId"))));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
