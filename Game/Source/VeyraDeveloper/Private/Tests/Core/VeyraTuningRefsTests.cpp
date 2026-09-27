// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Tests/Core/VeyraTuningTestTypes.h"
#include "Tuning/VeyraTuning.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraCoreTests
{
	// Veyra.Core.TuningRefs.*: shared records declared once under the root's "definitions" and used
	// through "$ref" (ADR-008 §7). The document cases are in TestData/TuningConformanceRefs.json.
	TEST_CLASS(TuningRefs, "Veyra.Core")
	{
		static constexpr int32 SchemaVersion = 1;

		// FVeyraTuningTestShape with its nested record and a number declared as definitions. The
		// placeholders %NESTED% and %DEFINITIONS% let each test change one part.
		const FString SchemaTemplate = FString(
			TEXT("{\"type\": \"object\", \"additionalProperties\": false,")
			TEXT(" \"required\": [\"schemaVersion\", \"speed\", \"count\", \"nested\"], \"definitions\": {%DEFINITIONS%}, \"properties\": {")
			TEXT(" \"schemaVersion\": {\"type\": \"integer\", \"minimum\": 1, \"enum\": [1]},")
			TEXT(" \"speed\": {\"type\": \"number\", \"minimum\": 0},")
			TEXT(" \"count\": {\"type\": \"integer\", \"minimum\": 0},")
			TEXT(" \"nested\": %NESTED%}}"));

		const FString Record = FString(
			TEXT("\"record\": {\"type\": \"object\", \"additionalProperties\": false, \"required\": [\"ratio\"],")
			TEXT(" \"properties\": {\"ratio\": {\"$ref\": \"#/definitions/fraction\"}}}"));

		const FString Fraction = FString(TEXT("\"fraction\": {\"type\": \"number\", \"minimum\": 0, \"maximum\": 1}"));

		const FString Document = FString(TEXT("{\"schemaVersion\": 1, \"speed\": 2, \"count\": 3, \"nested\": {\"ratio\": 0.5}}"));

		FString Schema(const FString& Nested, const FString& Definitions) const
		{
			return SchemaTemplate.Replace(TEXT("%NESTED%"), *Nested).Replace(TEXT("%DEFINITIONS%"), *Definitions);
		}

		FString UsualDefinitions() const
		{
			return Record + TEXT(", ") + Fraction;
		}

		bool Rejects(const FString& SchemaText, const TCHAR* Fragment)
		{
			FVeyraTuningTestShape Out;
			const VeyraTuning::FErrors Errors = VeyraTuning::ValidateAndBind(Document, SchemaText, SchemaVersion, Out);
			const bool bFound = Errors.ContainsByPredicate([Fragment](const FString& Error) { return Error.Contains(Fragment, ESearchCase::CaseSensitive); });
			return Assert.IsTrue(bFound, FString::Printf(TEXT("Expected an error containing '%s', got: %s"), Fragment,
				Errors.IsEmpty() ? TEXT("no errors") : *FString::Join(Errors, TEXT(" | "))));
		}

		TEST_METHOD(ARecordDeclaredOnceBindsThroughItsReference)
		{
			FVeyraTuningTestShape Out;
			const VeyraTuning::FErrors Errors = VeyraTuning::ValidateAndBind(Document, Schema(TEXT("{\"$ref\": \"#/definitions/record\"}"), UsualDefinitions()),
				SchemaVersion, Out);
			ASSERT_THAT(IsTrue(Errors.IsEmpty(), FString::Join(Errors, TEXT(" | "))));
			ASSERT_THAT(IsTrue(Out.Nested.Ratio == 0.5));
		}

		TEST_METHOD(AReferenceMayCarryADescription)
		{
			FVeyraTuningTestShape Out;
			const FString Nested = TEXT("{\"description\": \"The shared record.\", \"$ref\": \"#/definitions/record\"}");
			ASSERT_THAT(IsTrue(VeyraTuning::ValidateAndBind(Document, Schema(Nested, UsualDefinitions()), SchemaVersion, Out).IsEmpty()));
		}

		TEST_METHOD(RejectsAReferenceToNothing)
		{
			Rejects(Schema(TEXT("{\"$ref\": \"#/definitions/ghost\"}"), UsualDefinitions()), TEXT("\"definitions\" does not declare"));
		}

		TEST_METHOD(RejectsAReferenceOutsideTheDefinitions)
		{
			Rejects(Schema(TEXT("{\"$ref\": \"#/properties/speed\"}"), UsualDefinitions()), TEXT("must name a definition"));
		}

		TEST_METHOD(RejectsKeywordsBesideAReference)
		{
			Rejects(Schema(TEXT("{\"$ref\": \"#/definitions/record\", \"type\": \"object\"}"), UsualDefinitions()), TEXT("cannot stand beside \"$ref\""));
		}

		TEST_METHOD(RejectsACycleOfReferences)
		{
			const FString Cycle = UsualDefinitions() + TEXT(", \"first\": {\"$ref\": \"#/definitions/second\"}, \"second\": {\"$ref\": \"#/definitions/first\"}");
			Rejects(Schema(TEXT("{\"$ref\": \"#/definitions/first\"}"), Cycle), TEXT("cycle of definitions"));
		}

		TEST_METHOD(RejectsADefinitionNothingUses)
		{
			const FString Extra = UsualDefinitions() + TEXT(", \"spare\": {\"type\": \"number\", \"minimum\": 0}");
			Rejects(Schema(TEXT("{\"$ref\": \"#/definitions/record\"}"), Extra), TEXT("is never used"));
		}

		TEST_METHOD(RejectsDefinitionsBelowTheRoot)
		{
			const FString Nested = TEXT("{\"type\": \"object\", \"additionalProperties\": false, \"required\": [\"ratio\"], \"definitions\": {},")
				TEXT(" \"properties\": {\"ratio\": {\"$ref\": \"#/definitions/fraction\"}}}");
			Rejects(Schema(Nested, Fraction), TEXT("keyword \"definitions\" is not supported here"));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
