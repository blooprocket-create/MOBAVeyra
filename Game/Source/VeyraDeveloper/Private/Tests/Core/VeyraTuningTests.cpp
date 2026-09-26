// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Hash/Blake3.h"
#include "JsonUtils/RapidJsonUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Tests/Core/VeyraTuningTestTypes.h"
#include "Tuning/VeyraTuning.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraCoreTests
{
	// Veyra.Core.Tuning.*: the tuning framework validates strictly, binds only valid documents
	// and hashes the exact file bytes (ADR-006 §6).
	TEST_CLASS(Tuning, "Veyra.Core")
	{
		// The version these tests' schemas declare (schema version markers are not tuning).
		static constexpr int32 SchemaVersion = 1;

		// Matches FVeyraTuningTestShape and TestData/TuningConformance.schema.json. Kept on single
		// lines so the text never depends on the source file's line endings.
		const FString ShapeSchema = FString(
			TEXT("{\"type\": \"object\", \"additionalProperties\": false,")
			TEXT(" \"required\": [\"schemaVersion\", \"speed\", \"count\", \"nested\"], \"properties\": {")
			TEXT(" \"schemaVersion\": {\"type\": \"integer\", \"minimum\": 1, \"enum\": [1]},")
			TEXT(" \"speed\": {\"type\": \"number\", \"minimum\": 0, \"exclusiveMinimum\": true, \"maximum\": 1000},")
			TEXT(" \"count\": {\"type\": \"integer\", \"minimum\": 0, \"maximum\": 10},")
			TEXT(" \"nested\": {\"type\": \"object\", \"additionalProperties\": false, \"required\": [\"ratio\"],")
			TEXT(" \"properties\": {\"ratio\": {\"type\": \"number\", \"minimum\": 0, \"maximum\": 1, \"exclusiveMaximum\": true}}}}}"));

		const FString ValidDocument = FString(
			TEXT("{\"schemaVersion\": 1, \"speed\": 12.5, \"count\": 3, \"nested\": {\"ratio\": 0.25}}"));

		VeyraTuning::FErrors Bind(const FString& Document, const FString& Schema)
		{
			FVeyraTuningTestShape Out;
			return VeyraTuning::ValidateAndBind(Document, Schema, SchemaVersion, Out);
		}

		VeyraTuning::FErrors Bind(const FString& Document)
		{
			return Bind(Document, ShapeSchema);
		}

		// The valid document with one substring replaced, so each test changes exactly one thing.
		FString ValidWith(const TCHAR* From, const TCHAR* To) const
		{
			return ValidDocument.Replace(From, To, ESearchCase::CaseSensitive);
		}

		FString SchemaWith(const TCHAR* From, const TCHAR* To) const
		{
			return ShapeSchema.Replace(From, To, ESearchCase::CaseSensitive);
		}

		static bool HasError(const VeyraTuning::FErrors& Errors, const TCHAR* Fragment)
		{
			return Errors.ContainsByPredicate([Fragment](const FString& Error) { return Error.Contains(Fragment, ESearchCase::CaseSensitive); });
		}

		static FString Describe(const VeyraTuning::FErrors& Errors)
		{
			return Errors.IsEmpty() ? FString(TEXT("no errors")) : FString::Join(Errors, TEXT(" | "));
		}

		// Asserts that binding Document against Schema reports an error containing Fragment.
		bool Rejects(const FString& Document, const FString& Schema, const TCHAR* Fragment)
		{
			const VeyraTuning::FErrors Errors = Bind(Document, Schema);
			return Assert.IsTrue(HasError(Errors, Fragment),
				FString::Printf(TEXT("Expected an error containing '%s', got: %s"), Fragment, *Describe(Errors)));
		}

		bool Rejects(const FString& Document, const TCHAR* Fragment)
		{
			return Rejects(Document, ShapeSchema, Fragment);
		}

		TEST_METHOD(BindsAValidDocument)
		{
			FVeyraTuningTestShape Out;
			const VeyraTuning::FErrors Errors = VeyraTuning::ValidateAndBind(ValidDocument, ShapeSchema, SchemaVersion, Out);
			ASSERT_THAT(IsTrue(Errors.IsEmpty(), Describe(Errors)));
			ASSERT_THAT(IsTrue(Out.Speed == 12.5, TEXT("speed was not bound exactly")));
			ASSERT_THAT(AreEqual(3, Out.Count));
			ASSERT_THAT(IsTrue(Out.Nested.Ratio == 0.25, TEXT("ratio was not bound exactly")));
		}

		TEST_METHOD(RejectsMissingFields)
		{
			ASSERT_THAT(IsTrue(Rejects(ValidWith(TEXT(" \"count\": 3,"), TEXT("")), TEXT("/count: missing required field"))));
			ASSERT_THAT(IsTrue(Rejects(ValidWith(TEXT("\"ratio\": 0.25"), TEXT("")), TEXT("/nested/ratio: missing required field"))));
			ASSERT_THAT(IsTrue(Rejects(ValidWith(TEXT("\"schemaVersion\": 1, "), TEXT("")), TEXT("/schemaVersion: missing required field"))));
		}

		TEST_METHOD(RejectsUnknownFields)
		{
			ASSERT_THAT(IsTrue(Rejects(ValidWith(TEXT("\"count\": 3"), TEXT("\"count\": 3, \"extra\": 1")), TEXT("/extra: unknown field"))));
			ASSERT_THAT(IsTrue(Rejects(ValidWith(TEXT("\"ratio\": 0.25"), TEXT("\"ratio\": 0.25, \"extra\": 1")), TEXT("/nested/extra: unknown field"))));
		}

		TEST_METHOD(RejectsWrongTypes)
		{
			ASSERT_THAT(IsTrue(Rejects(ValidWith(TEXT("12.5"), TEXT("\"fast\"")), TEXT("/speed: expected a number"))));
			ASSERT_THAT(IsTrue(Rejects(ValidWith(TEXT("12.5"), TEXT("true")), TEXT("/speed: expected a number"))));
			ASSERT_THAT(IsTrue(Rejects(ValidWith(TEXT("12.5"), TEXT("null")), TEXT("/speed: expected a number"))));
			ASSERT_THAT(IsTrue(Rejects(ValidWith(TEXT("{\"ratio\": 0.25}"), TEXT("3")), TEXT("/nested: expected an object"))));
		}

		TEST_METHOD(RejectsADecimalOrExponentForAnInteger)
		{
			ASSERT_THAT(IsTrue(Rejects(ValidWith(TEXT("\"count\": 3"), TEXT("\"count\": 3.0")), TEXT("/count: expected an integer"))));
			ASSERT_THAT(IsTrue(Rejects(ValidWith(TEXT("\"count\": 3"), TEXT("\"count\": 3e0")), TEXT("/count: expected an integer"))));
		}

		TEST_METHOD(RejectsOutOfRangeValues)
		{
			ASSERT_THAT(IsTrue(Rejects(ValidWith(TEXT("\"count\": 3"), TEXT("\"count\": -1")), TEXT("/count: must be at least 0"))));
			ASSERT_THAT(IsTrue(Rejects(ValidWith(TEXT("12.5"), TEXT("0")), TEXT("/speed: must be greater than 0"))));
			ASSERT_THAT(IsTrue(Rejects(ValidWith(TEXT("12.5"), TEXT("1000.5")), TEXT("/speed: must be at most 1000"))));
			ASSERT_THAT(IsTrue(Rejects(ValidWith(TEXT("0.25"), TEXT("1")), TEXT("/nested/ratio: must be less than 1"))));
			ASSERT_THAT(IsTrue(Rejects(ValidWith(TEXT("\"count\": 3"), TEXT("\"count\": 11")), TEXT("/count: must be at most 10"))));
		}

		TEST_METHOD(AcceptsInclusiveBounds)
		{
			const VeyraTuning::FErrors Errors = Bind(TEXT("{\"schemaVersion\": 1, \"speed\": 1000, \"count\": 0, \"nested\": {\"ratio\": 0}}"));
			ASSERT_THAT(IsTrue(Errors.IsEmpty(), Describe(Errors)));
		}

		TEST_METHOD(RejectsDuplicateKeys)
		{
			ASSERT_THAT(IsTrue(Rejects(ValidWith(TEXT("\"count\": 3"), TEXT("\"count\": 3, \"count\": 4")), TEXT("/count: duplicate key"))));
		}

		TEST_METHOD(RejectsNonStrictJson)
		{
			ASSERT_THAT(IsTrue(Rejects(ValidWith(TEXT("\"count\": 3,"), TEXT("\"count\": 3, // comment\n")), TEXT("document:"))));
			ASSERT_THAT(IsTrue(Rejects(ValidWith(TEXT("0.25}}"), TEXT("0.25},}")), TEXT("document:"))));
			ASSERT_THAT(IsTrue(Rejects(ValidWith(TEXT("12.5"), TEXT("NaN")), TEXT("document:"))));
			ASSERT_THAT(IsTrue(Rejects(ValidWith(TEXT("12.5"), TEXT("Infinity")), TEXT("document:"))));
			ASSERT_THAT(IsTrue(Rejects(FString(), TEXT("document:"))));
		}

		TEST_METHOD(RejectsAByteOrderMarkAndCarriageReturns)
		{
			ASSERT_THAT(IsTrue(Rejects(FString(TEXT("﻿")) + ValidDocument, TEXT("document: starts with a byte-order mark"))));
			ASSERT_THAT(IsTrue(Rejects(ValidWith(TEXT("\"count\": 3,"), TEXT("\"count\": 3,\r\n")), TEXT("document: contains a carriage return"))));
		}

		TEST_METHOD(RejectsAnEmptyObjectAndANonObjectRoot)
		{
			ASSERT_THAT(IsTrue(Rejects(TEXT("{}"), TEXT("/speed: missing required field"))));
			ASSERT_THAT(IsTrue(Rejects(TEXT("[]"), TEXT("(root): expected an object"))));
		}

		TEST_METHOD(RejectsAWrongSchemaVersion)
		{
			ASSERT_THAT(IsTrue(Rejects(ValidWith(TEXT("\"schemaVersion\": 1"), TEXT("\"schemaVersion\": 2")),
				TEXT("/schemaVersion: is 2, but this build reads version 1"))));
		}

		TEST_METHOD(RejectsSchemasOutsideTheDialect)
		{
			ASSERT_THAT(IsTrue(Rejects(ValidDocument, SchemaWith(TEXT("\"maximum\": 10}"), TEXT("\"maximum\": 10, \"pattern\": \"x\"}")),
				TEXT("keyword \"pattern\" is not supported here"))));
			ASSERT_THAT(IsTrue(Rejects(ValidDocument, SchemaWith(TEXT("\"type\": \"integer\", \"minimum\": 0, "), TEXT("\"type\": \"integer\", ")),
				TEXT("every number must declare \"minimum\""))));
			ASSERT_THAT(IsTrue(Rejects(ValidDocument, SchemaWith(TEXT("\"speed\", \"count\""), TEXT("\"speed\"")),
				TEXT("must list \"count\"; no field is optional"))));
			ASSERT_THAT(IsTrue(Rejects(ValidDocument, SchemaWith(TEXT("\"additionalProperties\": false, \"required\": [\"ratio\"]"), TEXT("\"required\": [\"ratio\"]")),
				TEXT("must declare \"additionalProperties\": false"))));
			ASSERT_THAT(IsTrue(Rejects(ValidDocument, SchemaWith(TEXT("\"exclusiveMinimum\": true"), TEXT("\"exclusiveMinimum\": 0")),
				TEXT("\"exclusiveMinimum\" must be true or false"))));
		}

		TEST_METHOD(RejectsASchemaAndStructThatDisagree)
		{
			// A schema property with no UPROPERTY behind it.
			const FString ExtraProperty = SchemaWith(TEXT("\"count\", \"nested\"]"), TEXT("\"count\", \"nested\", \"extra\"]"))
				.Replace(TEXT(" \"count\": {"), TEXT(" \"extra\": {\"type\": \"number\", \"minimum\": 0}, \"count\": {"), ESearchCase::CaseSensitive);
			ASSERT_THAT(IsTrue(Rejects(ValidWith(TEXT("\"count\": 3"), TEXT("\"count\": 3, \"extra\": 1")), ExtraProperty,
				TEXT("has no matching UPROPERTY in VeyraTuningTestShape"))));

			// A UPROPERTY the schema does not describe would silently keep its default.
			const FString MissingProperty = SchemaWith(TEXT("\"speed\", \"count\""), TEXT("\"speed\""))
				.Replace(TEXT(" \"count\": {\"type\": \"integer\", \"minimum\": 0, \"maximum\": 10},"), TEXT(""), ESearchCase::CaseSensitive);
			ASSERT_THAT(IsTrue(Rejects(ValidWith(TEXT(" \"count\": 3,"), TEXT("")), MissingProperty,
				TEXT("has no property \"count\" for VeyraTuningTestShape::Count"))));
		}

		TEST_METHOD(RejectsASchemaVersionTheBuildDoesNotRead)
		{
			ASSERT_THAT(IsTrue(Rejects(ValidWith(TEXT("\"schemaVersion\": 1"), TEXT("\"schemaVersion\": 2")),
				SchemaWith(TEXT("\"enum\": [1]"), TEXT("\"enum\": [2]")), TEXT("must declare \"enum\": [1]"))));
		}

		TEST_METHOD(CollectsEveryError)
		{
			const VeyraTuning::FErrors Errors = Bind(TEXT("{\"schemaVersion\": 1, \"speed\": 0, \"count\": 99, \"extra\": 1, \"nested\": {}}"));
			ASSERT_THAT(IsTrue(HasError(Errors, TEXT("/speed:")), Describe(Errors)));
			ASSERT_THAT(IsTrue(HasError(Errors, TEXT("/count:")), Describe(Errors)));
			ASSERT_THAT(IsTrue(HasError(Errors, TEXT("/extra:")), Describe(Errors)));
			ASSERT_THAT(IsTrue(HasError(Errors, TEXT("/nested/ratio:")), Describe(Errors)));
		}

		TEST_METHOD(LeavesTheOutputUntouchedOnFailure)
		{
			FVeyraTuningTestShape Out;
			Out.Speed = 7.0;
			Out.Count = 7;
			Out.Nested.Ratio = 0.7;
			const VeyraTuning::FErrors Errors = VeyraTuning::ValidateAndBind(ValidWith(TEXT("0.25"), TEXT("5")), ShapeSchema, SchemaVersion, Out);
			ASSERT_THAT(IsFalse(Errors.IsEmpty()));
			ASSERT_THAT(IsTrue(Out.Speed == 7.0, TEXT("speed changed")));
			ASSERT_THAT(AreEqual(7, Out.Count));
			ASSERT_THAT(IsTrue(Out.Nested.Ratio == 0.7, TEXT("ratio changed")));
		}

#if WITH_EDITOR
		// These read files from the project folder, which exists only in editor builds.

		/**
		 * Runs one corpus from TestData against its schema, binding ShapeType, and returns every case
		 * whose verdict differs from the corpus's, or a single problem with the corpus itself.
		 */
		template <typename ShapeType>
		static TArray<FString> CorpusMismatches(const TCHAR* CorpusName)
		{
			const FString Directory = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source/VeyraDeveloper/TestData"));
			FString CorpusText;
			FString SchemaText;
			if (!FFileHelper::LoadFileToString(CorpusText, *FPaths::Combine(Directory, FString(CorpusName) + TEXT(".json")))
				|| !FFileHelper::LoadFileToString(SchemaText, *FPaths::Combine(Directory, FString(CorpusName) + TEXT(".schema.json"))))
			{
				return { FString::Printf(TEXT("%s or its schema cannot be read."), CorpusName) };
			}

			TValueOrError<UE::Json::FDocument, UE::Json::FParseError> Corpus = UE::Json::Parse(CorpusText);
			const TOptional<UE::Json::FConstObject> Root = Corpus.HasValue() ? UE::Json::GetRootObject(Corpus.GetValue()) : TOptional<UE::Json::FConstObject>();
			const TOptional<UE::Json::FConstArray> Cases = Root.IsSet() ? UE::Json::GetArrayField(*Root, TEXT("cases")) : TOptional<UE::Json::FConstArray>();
			if (!Cases.IsSet() || Cases->Empty())
			{
				return { FString::Printf(TEXT("%s does not parse or has no cases."), CorpusName) };
			}

			TArray<FString> Mismatches;
			for (const UE::Json::FValue& Case : *Cases)
			{
				const TOptional<FStringView> Name = Case.IsObject() ? UE::Json::GetStringField(Case.GetObject(), TEXT("name")) : TOptional<FStringView>();
				const TOptional<FStringView> Document = Case.IsObject() ? UE::Json::GetStringField(Case.GetObject(), TEXT("document")) : TOptional<FStringView>();
				const TOptional<bool> bValid = Case.IsObject() ? UE::Json::GetBoolField(Case.GetObject(), TEXT("valid")) : TOptional<bool>();
				if (!Name.IsSet() || !Document.IsSet() || !bValid.IsSet())
				{
					Mismatches.Add(FString::Printf(TEXT("A case in %s lacks name, document or valid."), CorpusName));
					continue;
				}
				ShapeType Out;
				const VeyraTuning::FErrors Errors = VeyraTuning::ValidateAndBind(*Document, SchemaText, SchemaVersion, Out);
				if (Errors.IsEmpty() != *bValid)
				{
					Mismatches.Add(FString::Printf(TEXT("%s '%s' should be %s: %s"), CorpusName, *FString(*Name),
						*bValid ? TEXT("valid") : TEXT("invalid"), *Describe(Errors)));
				}
			}
			return Mismatches;
		}

		TEST_METHOD(ConformanceCorpus)
		{
			// Same cases as tests/test_tuning.py, so the game and CI validators cannot drift apart.
			TArray<FString> Mismatches = CorpusMismatches<FVeyraTuningTestShape>(TEXT("TuningConformance"));
			Mismatches.Append(CorpusMismatches<FVeyraTuningContentTestShape>(TEXT("TuningConformanceContent")));
			ASSERT_THAT(IsTrue(Mismatches.IsEmpty(), FString::Join(Mismatches, TEXT(" || "))));
		}

		TEST_METHOD(ReadsAndHashesTheExactFileBytes)
		{
			VeyraTuning::FDomainFiles Files;
			const VeyraTuning::FErrors Errors = VeyraTuning::ReadDomainFiles(TEXT("Combat"), Files);
			ASSERT_THAT(IsTrue(Errors.IsEmpty(), Describe(Errors)));

			TArray<uint8> Bytes;
			ASSERT_THAT(IsTrue(FFileHelper::LoadFileToArray(Bytes, *FPaths::Combine(FPaths::ProjectDir(), TEXT("Tuning/Combat.json")))));
			ASSERT_THAT(IsTrue(Files.DocumentHash == FBlake3::HashBuffer(Bytes.GetData(), Bytes.Num()), TEXT("The hash is not BLAKE3 of the file bytes.")));

			VeyraTuning::FDomainFiles Again;
			ASSERT_THAT(IsTrue(VeyraTuning::ReadDomainFiles(TEXT("Combat"), Again).IsEmpty()));
			ASSERT_THAT(IsTrue(Again.DocumentHash == Files.DocumentHash, TEXT("Reading the same file twice gave different hashes.")));
		}

		TEST_METHOD(ReportsAMissingDomainFile)
		{
			VeyraTuning::FDomainFiles Files;
			const VeyraTuning::FErrors Errors = VeyraTuning::ReadDomainFiles(TEXT("NoSuchDomain"), Files);
			ASSERT_THAT(IsTrue(HasError(Errors, TEXT("NoSuchDomain.json: cannot be read")), Describe(Errors)));
			ASSERT_THAT(IsTrue(HasError(Errors, TEXT("NoSuchDomain.schema.json: cannot be read")), Describe(Errors)));
		}
#endif // WITH_EDITOR
	};
}

#endif // WITH_AUTOMATION_WORKER
