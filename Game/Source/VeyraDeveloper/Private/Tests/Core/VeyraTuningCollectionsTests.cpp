// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Content/VeyraContentId.h"
#include "CQTest.h"
#include "Tests/Core/VeyraTuningTestTypes.h"
#include "Tuning/VeyraTuning.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraCoreTests
{
	// Veyra.Core.TuningCollections.*: text in a declared format and arrays bind through the tuning
	// framework, and their schemas must use the dialect's exact forms (Game/Tuning/README.md).
	TEST_CLASS(TuningCollections, "Veyra.Core")
	{
		static constexpr int32 SchemaVersion = 1;

		// Matches FVeyraTuningCollectionsTestShape and TestData/TuningConformanceCollections.schema.json.
		const FString Schema = FString(
			TEXT("{\"type\": \"object\", \"additionalProperties\": false, \"required\": [\"schemaVersion\", \"label\", \"tags\", \"points\", \"scores\"],")
			TEXT(" \"properties\": {\"schemaVersion\": {\"type\": \"integer\", \"minimum\": 1, \"enum\": [1]},")
			TEXT(" \"label\": {\"type\": \"string\", \"pattern\": \"^[A-Za-z][A-Za-z0-9 ]{0,15}$\"},")
			TEXT(" \"tags\": {\"type\": \"array\", \"minItems\": 0, \"maxItems\": 3, \"items\": {\"type\": \"string\", \"pattern\": \"^[a-z][a-z0-9]*(_[a-z0-9]+)*$\"}},")
			TEXT(" \"points\": {\"type\": \"array\", \"minItems\": 1, \"items\": {\"type\": \"object\", \"additionalProperties\": false, \"required\": [\"x\", \"name\"],")
			TEXT(" \"properties\": {\"x\": {\"type\": \"number\", \"minimum\": 0}, \"name\": {\"type\": \"string\", \"pattern\": \"^[0-9a-f]{4}$\"}}}},")
			TEXT(" \"scores\": {\"type\": \"array\", \"minItems\": 1, \"maxItems\": 4, \"items\": {\"type\": \"integer\", \"minimum\": 0, \"maximum\": 100}}}}"));

		const FString Document = FString(
			TEXT("{\"schemaVersion\": 1, \"label\": \"Dev One\", \"tags\": [\"alpha\", \"beta_2\"],")
			TEXT(" \"points\": [{\"x\": 1.5, \"name\": \"00ff\"}, {\"x\": 2, \"name\": \"abcd\"}], \"scores\": [1, 100]}"));

		VeyraTuning::FErrors Bind(const FString& SchemaText, const FString& DocumentText, FVeyraTuningCollectionsTestShape& Out) const
		{
			return VeyraTuning::ValidateAndBind(DocumentText, SchemaText, SchemaVersion, Out);
		}

		VeyraTuning::FErrors BindWithSchema(const TCHAR* From, const TCHAR* To) const
		{
			FVeyraTuningCollectionsTestShape Out;
			return Bind(Schema.Replace(From, To, ESearchCase::CaseSensitive), Document, Out);
		}

		VeyraTuning::FErrors BindWithDocument(const TCHAR* From, const TCHAR* To) const
		{
			FVeyraTuningCollectionsTestShape Out;
			return Bind(Schema, Document.Replace(From, To, ESearchCase::CaseSensitive), Out);
		}

		static bool HasError(const VeyraTuning::FErrors& Errors, const TCHAR* Fragment)
		{
			return Errors.ContainsByPredicate([Fragment](const FString& Error) { return Error.Contains(Fragment, ESearchCase::CaseSensitive); });
		}

		TEST_METHOD(BindsTextAndArrays)
		{
			FVeyraTuningCollectionsTestShape Out;
			const VeyraTuning::FErrors Errors = Bind(Schema, Document, Out);
			ASSERT_THAT(IsTrue(Errors.IsEmpty(), FString::Join(Errors, TEXT(" | "))));
			ASSERT_THAT(AreEqual(Out.Label, FString(TEXT("Dev One"))));
			ASSERT_THAT(AreEqual(Out.Tags.Num(), 2));
			ASSERT_THAT(AreEqual(Out.Tags[1].ToString(), FString(TEXT("beta_2"))));
			ASSERT_THAT(AreEqual(Out.Points.Num(), 2));
			ASSERT_THAT(IsTrue(Out.Points[0].X == 1.5));
			ASSERT_THAT(AreEqual(Out.Points[1].Name, FString(TEXT("abcd"))));
			ASSERT_THAT(IsTrue(Out.Scores == TArray<int32>({ 1, 100 })));
		}

		TEST_METHOD(ErrorsPointAtTheItem)
		{
			const VeyraTuning::FErrors Errors = BindWithDocument(TEXT("\"name\": \"abcd\""), TEXT("\"name\": \"ABCD\""));
			ASSERT_THAT(IsTrue(HasError(Errors, TEXT("/points/1/name: does not match the format"))));
		}

		TEST_METHOD(TextPatternMustBeAnchored)
		{
			const VeyraTuning::FErrors Errors = BindWithSchema(TEXT("\"^[A-Za-z][A-Za-z0-9 ]{0,15}$\""), TEXT("\"[A-Za-z][A-Za-z0-9 ]{0,15}\""));
			ASSERT_THAT(IsTrue(HasError(Errors, TEXT("must be anchored"))));
		}

		TEST_METHOD(TextCannotUseTheContentIdFormat)
		{
			const VeyraTuning::FErrors Errors = BindWithSchema(TEXT("\"^[A-Za-z][A-Za-z0-9 ]{0,15}$\""), TEXT("\"^[a-z][a-z0-9]*(_[a-z0-9]+)*$\""));
			ASSERT_THAT(IsTrue(HasError(Errors, TEXT("uses the content ID format, so Label must be an FVeyraContentId"))));
		}

		TEST_METHOD(EveryArrayDeclaresItsMinimumLength)
		{
			ASSERT_THAT(IsTrue(HasError(BindWithSchema(TEXT("\"minItems\": 1, \"maxItems\": 4,"), TEXT("\"maxItems\": 4,")),
				TEXT("every array must declare \"minItems\""))));
			ASSERT_THAT(IsTrue(HasError(BindWithSchema(TEXT("\"minItems\": 1, \"maxItems\": 4,"), TEXT("\"minItems\": 5, \"maxItems\": 4,")),
				TEXT("\"maxItems\" must be an integer no smaller than \"minItems\""))));
			ASSERT_THAT(IsTrue(HasError(BindWithSchema(TEXT("\"minItems\": 1, \"maxItems\": 4,"), TEXT("\"minItems\": 1, \"uniqueItems\": true,")),
				TEXT("keyword \"uniqueItems\" is not supported"))));
		}

		TEST_METHOD(ArraysBindOnlyToTArrays)
		{
			const VeyraTuning::FErrors Errors = BindWithSchema(TEXT("\"x\": {\"type\": \"number\", \"minimum\": 0}"),
				TEXT("\"x\": {\"type\": \"array\", \"minItems\": 0, \"items\": {\"type\": \"number\", \"minimum\": 0}}"));
			ASSERT_THAT(IsTrue(HasError(Errors, TEXT("is an array, so X must be a TArray"))));
		}

		TEST_METHOD(ItemSchemaMustMatchTheItemType)
		{
			// Checked even though the document's scores are valid integers.
			const VeyraTuning::FErrors Errors = BindWithSchema(TEXT("\"items\": {\"type\": \"integer\", \"minimum\": 0, \"maximum\": 100}"),
				TEXT("\"items\": {\"type\": \"number\", \"minimum\": 0, \"maximum\": 100}"));
			ASSERT_THAT(IsTrue(HasError(Errors, TEXT("must be a double"))));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
