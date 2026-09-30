// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER

#include "DevCommands/VeyraDevCommands.h"
#include "HAL/IConsoleManager.h"
#include "Misc/StringOutputDevice.h"

namespace VeyraDevCommandTests
{
	// Veyra.Developer.DevCommandCatalog.*: the Veyra.Dev.* commands' one catalog, the console that
	// reaches them, and Veyra.Dev.Help, which lists them from it (PROJECT_STRUCTURE.md "VeyraDeveloper").
	TEST_CLASS(DevCommandCatalog, "Veyra.Developer")
	{
		TEST_METHOD(EveryCommandIsNamedOnceDescribedAndRunsOneWay)
		{
			TSet<FString> Seen;
			for (const FVeyraDevCommand& Command : VeyraDevCommands::Catalog())
			{
				const FString Name = Command.Name;
				ASSERT_THAT(IsFalse(Name.IsEmpty()));
				ASSERT_THAT(IsFalse(Seen.Contains(Name.ToLower()), *Name));
				Seen.Add(Name.ToLower());
				ASSERT_THAT(IsFalse(FString(Command.Category).IsEmpty(), *Name));
				ASSERT_THAT(IsFalse(FString(Command.Summary).IsEmpty(), *Name));
				ASSERT_THAT(IsTrue((Command.ServerRun != nullptr) != (Command.LocalRun != nullptr), *Name));
				ASSERT_THAT(IsTrue(Command.Prepare == nullptr || Command.ServerRun != nullptr, *Name));
				ASSERT_THAT(IsTrue(VeyraDevCommands::Find(Name.ToUpper()) == &Command, TEXT("found ignoring case")));
			}
			ASSERT_THAT(IsNotNull(VeyraDevCommands::Find(TEXT("Help"))));
		}

		TEST_METHOD(TheConsoleHoldsEveryCommandAndNoOtherVeyraDevCommand)
		{
			for (const FVeyraDevCommand& Command : VeyraDevCommands::Catalog())
			{
				IConsoleObject* Object = IConsoleManager::Get().FindConsoleObject(*VeyraDevCommands::ConsoleName(Command));
				ASSERT_THAT(IsTrue(Object && Object->AsCommand(), *VeyraDevCommands::ConsoleName(Command)));
			}
			// So Veyra.Dev.Help, which lists the catalog, lists every command there is.
			TArray<FString> Strays;
			IConsoleManager::Get().ForEachConsoleObjectThatStartsWith(FConsoleObjectVisitor::CreateLambda([&Strays](const TCHAR* Name, IConsoleObject*) {
				if (!VeyraDevCommands::Find(FStringView(Name).RightChop(FCString::Strlen(TEXT("Veyra.Dev.")))))
				{
					Strays.Add(Name);
				}
			}),
				TEXT("Veyra.Dev."));
			ASSERT_THAT(IsTrue(Strays.IsEmpty(), *FString::Join(Strays, TEXT(", "))));
		}

		TEST_METHOD(HelpListsEveryCommandAndFilters)
		{
			FStringOutputDevice All;
			VeyraDevCommands::WriteHelp({}, All);
			for (const FVeyraDevCommand& Command : VeyraDevCommands::Catalog())
			{
				ASSERT_THAT(IsTrue(All.Contains(VeyraDevCommands::ConsoleName(Command)), *VeyraDevCommands::ConsoleName(Command)));
				ASSERT_THAT(IsTrue(All.Contains(Command.Summary), Command.Name));
			}

			// A filter keeps the commands whose name or category contains it, ignoring case.
			FStringOutputDevice Economy;
			VeyraDevCommands::WriteHelp(TEXT("economy"), Economy);
			ASSERT_THAT(IsTrue(Economy.Contains(TEXT("Veyra.Dev.Gold "))));
			ASSERT_THAT(IsFalse(Economy.Contains(TEXT("Veyra.Dev.Kill"))));

			FStringOutputDevice Nothing;
			VeyraDevCommands::WriteHelp(TEXT("no-such-command"), Nothing);
			ASSERT_THAT(IsTrue(Nothing.Contains(TEXT("No developer command matches"))));
		}

		TEST_METHOD(ArgumentsParseStrictly)
		{
			const TArray<FString> Args = { TEXT("12.5"), TEXT("7"), TEXT("lots"), TEXT("ON") };
			ASSERT_THAT(IsTrue(VeyraDevCommands::ParseNumber(Args, 0) == TOptional<double>(12.5)));
			ASSERT_THAT(IsFalse(VeyraDevCommands::ParseNumber(Args, 2).IsSet()));
			ASSERT_THAT(IsFalse(VeyraDevCommands::ParseNumber(Args, 4).IsSet(), TEXT("missing")));
			ASSERT_THAT(IsTrue(VeyraDevCommands::ParseWhole(Args, 1) == TOptional<int32>(7)));
			ASSERT_THAT(IsFalse(VeyraDevCommands::ParseWhole(Args, 0).IsSet(), TEXT("not whole")));

			// A switch names its state, in any case, or turns the current one over.
			ASSERT_THAT(IsTrue(VeyraDevCommands::ParseSwitch(TConstArrayView<FString>(Args).RightChop(3), false) == TOptional<bool>(true)));
			ASSERT_THAT(IsTrue(VeyraDevCommands::ParseSwitch({}, true) == TOptional<bool>(false)));
			ASSERT_THAT(IsFalse(VeyraDevCommands::ParseSwitch(TConstArrayView<FString>(Args).RightChop(2), false).IsSet()));

			ASSERT_THAT(IsTrue(VeyraDevCommands::UsageReply(TEXT("Gold")).StartsWith(TEXT("Usage: Veyra.Dev.Gold <amount>"))));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
