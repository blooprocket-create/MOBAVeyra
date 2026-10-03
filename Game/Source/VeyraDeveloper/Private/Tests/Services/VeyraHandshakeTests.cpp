// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Battleground/VeyraBattlegroundMapCommandlet.h"
#include "Client/VeyraClientFlowSubsystem.h"
#include "CQTest.h"

#if WITH_AUTOMATION_WORKER

#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "FrontEnd/VeyraFrontEndMapCommandlet.h"
#include "FrontEnd/VeyraShellGameMode.h"
#include "GameFramework/WorldSettings.h"
#include "GameMapsSettings.h"
#include "HAL/PlatformProcess.h"
#include "Handoff/VeyraLaunchHandshake.h"
#include "Handoff/VeyraPipeLineReader.h"
#include "Handoff/VeyraPipeLineWriter.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace VeyraServicesTests
{
	// Veyra.Services.PipeWriter.*: lines to a pipe, the way the game tells a launcher where the launch
	// handshake is (ADR-010 §5).
	TEST_CLASS(PipeWriter, "Veyra.Services")
	{
		TEST_METHOD(WritesALineOnlyToAPipe)
		{
			FVeyraPipeLineWriter Nothing(FVeyraPipeLineWriter::FNativeHandle{});
			ASSERT_THAT(IsFalse(Nothing.IsPipe()));
			ASSERT_THAT(IsFalse(Nothing.WriteLine(TEXT("lost"))));
#if PLATFORM_WINDOWS
			void* ReadEnd = nullptr;
			void* WriteEnd = nullptr;
			ASSERT_THAT(IsTrue(FPlatformProcess::CreatePipe(ReadEnd, WriteEnd)));
			FVeyraPipeLineWriter Writer(WriteEnd);
			FVeyraPipeLineReader Reader(ReadEnd);
			ASSERT_THAT(IsTrue(Writer.IsPipe()));
			ASSERT_THAT(IsTrue(Writer.WriteLine(TEXT("veyra-handoff/1 é"))));
			FString Line;
			const EVeyraPipeRead Read = Reader.Poll(Line);
			FPlatformProcess::ClosePipe(ReadEnd, WriteEnd);
			ASSERT_THAT(IsTrue(Read == EVeyraPipeRead::Line));
			ASSERT_THAT(AreEqual(Line, FString(TEXT("veyra-handoff/1 é"))));
#endif
		}

#if PLATFORM_WINDOWS
		TEST_METHOD(AGoneReaderFailsTheWriteWithoutBlocking)
		{
			void* ReadEnd = nullptr;
			void* WriteEnd = nullptr;
			ASSERT_THAT(IsTrue(FPlatformProcess::CreatePipe(ReadEnd, WriteEnd)));
			FPlatformProcess::ClosePipe(ReadEnd, nullptr);
			FVeyraPipeLineWriter Writer(WriteEnd);
			const bool bWritten = Writer.WriteLine(TEXT("anyone there?"));
			FPlatformProcess::ClosePipe(nullptr, WriteEnd);
			ASSERT_THAT(IsFalse(bWritten));
		}
#endif
	};

	// Veyra.Services.LaunchHandshake.*: the game's handshake lines are the contract's
	// (Contracts/LaunchHandshake.json), which veyra-devlaunch and the launcher also test against.
	TEST_CLASS(LaunchHandshake, "Veyra.Services")
	{
#if WITH_EDITOR
		// The contract is read from the project folder, which exists only in editor builds.
		TEST_METHOD(TheLinesAreTheContracts)
		{
			FString Text;
			ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(Text, *VeyraLaunchHandshake::ContractPath()), VeyraLaunchHandshake::ContractPath()));
			TSharedPtr<FJsonObject> Contract;
			ASSERT_THAT(IsTrue(FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Contract) && Contract.IsValid()));
			ASSERT_THAT(AreEqual(Contract->GetStringField(TEXT("awaitingLaunchCode")), FString(VeyraLaunchHandshake::AwaitingLaunchCode)));
			ASSERT_THAT(AreEqual(Contract->GetStringField(TEXT("signedIn")), FString(VeyraLaunchHandshake::SignedIn)));
			ASSERT_THAT(AreEqual(Contract->GetStringField(TEXT("failedPrefix")), FString(VeyraLaunchHandshake::FailedPrefix)));

			TArray<FString> Codes;
			ASSERT_THAT(IsTrue(Contract->TryGetStringArrayField(TEXT("failureCodes"), Codes)));
			using VeyraLaunchHandshake::EFailure;
			TArray<FString> Ours;
			for (const EFailure Failure : { EFailure::NoLaunchCode, EFailure::InvalidLaunchCode, EFailure::SignInRefused, EFailure::BackendUnreachable,
					 EFailure::BadAnswer, EFailure::Misconfigured })
			{
				Ours.Add(VeyraLaunchHandshake::FailureCode(Failure));
			}
			ASSERT_THAT(IsTrue(Codes == Ours, FString::Join(Codes, TEXT(","))));
			ASSERT_THAT(AreEqual(VeyraLaunchHandshake::FailedLine(EFailure::SignInRefused), FString(TEXT("veyra-handoff/1 failed sign_in_refused"))));
		}
#endif
	};

	// Veyra.Services.FrontEndMap.*: a game starts in the generated front end, whose game mode is the
	// shell's, and a server in the grey-box map (ADR-010 §3). When the map is missing or stale, run
	// Game/Scripts/BuildFrontEndMap.ps1.
	TEST_CLASS(FrontEndMap, "Veyra.Services")
	{
		static FString ObjectPath(const TCHAR* PackageName)
		{
			return FString(PackageName) + TEXT(".") + FPackageName::GetShortName(PackageName);
		}

		TEST_METHOD(TheSavedMapRunsTheShell)
		{
			const UWorld* Map = LoadObject<UWorld>(nullptr, *ObjectPath(UVeyraFrontEndMapCommandlet::MapPackageName));
			ASSERT_THAT(IsNotNull(Map));
			ASSERT_THAT(IsTrue(Map->GetWorldSettings()->DefaultGameMode == AVeyraShellGameMode::StaticClass()));
		}

		TEST_METHOD(GamesStartThereServersOnTheBattlegroundAndEveryMapIsCooked)
		{
			// The engine reports the default map by its package name.
			ASSERT_THAT(AreEqual(UGameMapsSettings::GetGameDefaultMap(), FString(UVeyraFrontEndMapCommandlet::MapPackageName)));
			FString ServerMap;
			GConfig->GetString(TEXT("/Script/EngineSettings.GameMapsSettings"), TEXT("ServerDefaultMap"), ServerMap, GEngineIni);
			ASSERT_THAT(AreEqual(ServerMap, ObjectPath(UVeyraBattlegroundMapCommandlet::MapPackageName)));
			TArray<FString> Cooked;
			GConfig->GetArray(TEXT("/Script/UnrealEd.ProjectPackagingSettings"), TEXT("MapsToCook"), Cooked, GGameIni);
			const FString Joined = FString::Join(Cooked, TEXT("|"));
			ASSERT_THAT(IsTrue(Joined.Contains(UVeyraFrontEndMapCommandlet::MapPackageName), Joined));
			ASSERT_THAT(IsTrue(Joined.Contains(UVeyraBattlegroundMapCommandlet::MapPackageName), Joined));
			// Development matches still load the grey box.
			ASSERT_THAT(IsTrue(Joined.Contains(TEXT("/Game/Veyra/Developer/Maps/L_Greybox")), Joined));
		}
	};

	// Veyra.Services.BackendAddress.*: a game started by the launcher uses the launcher's backend, which
	// -VeyraBackendUrl= names, over its own ini's (ADR-057 §5).
	TEST_CLASS(BackendAddress, "Veyra.Services")
	{
		TEST_METHOD(TheLaunchersBackendComesFirstAndMustBeABaseUrl)
		{
			const FString Ini = TEXT("http://127.0.0.1:8080");
			const TOptional<FString> Plain = UVeyraClientFlowSubsystem::BackendBaseUrlFor(TEXT("-VeyraLaunchCode=stdin"), Ini);
			ASSERT_THAT(IsTrue(Plain.IsSet() && Plain.GetValue() == Ini, TEXT("without the switch, the ini's")));
			const TOptional<FString> Public =
				UVeyraClientFlowSubsystem::BackendBaseUrlFor(TEXT("-VeyraLaunchCode=stdin -VeyraBackendUrl=https://veyra.blooprocket.workers.dev -windowed"), Ini);
			ASSERT_THAT(IsTrue(Public.IsSet() && Public.GetValue() == TEXT("https://veyra.blooprocket.workers.dev"), TEXT("the launcher's")));
			ASSERT_THAT(IsFalse(UVeyraClientFlowSubsystem::BackendBaseUrlFor(TEXT("-VeyraBackendUrl=https://veyra.example/api"), Ini).IsSet(), TEXT("no path")));
			ASSERT_THAT(IsFalse(UVeyraClientFlowSubsystem::BackendBaseUrlFor(TEXT("-VeyraBackendUrl=ftp://veyra.example"), Ini).IsSet()));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
