// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "DevCommands/VeyraDevCommands.h"

#include "Algo/StableSort.h"
#include "Developer/VeyraDeveloperCommandRoute.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Misc/OutputDevice.h"
#include "VeyraPlayerController.h"
#include "VeyraPlayerState.h"

namespace VeyraDevCommands
{
	namespace
	{
		const TCHAR* const ConsolePrefix = TEXT("Veyra.Dev.");

		/** The console objects Register made, which Unregister removes. */
		TArray<IConsoleObject*>& RegisteredObjects()
		{
			static TArray<IConsoleObject*> Objects;
			return Objects;
		}

		/** A command's name and arguments as typed, such as "Veyra.Dev.Gold <amount>". */
		FString Synopsis(const FVeyraDevCommand& Command)
		{
			return *Command.Usage ? ConsoleName(Command) + TEXT(" ") + Command.Usage : ConsoleName(Command);
		}

		bool MatchesFilter(const FVeyraDevCommand& Command, FStringView Filter)
		{
			return Filter.IsEmpty() || FStringView(Command.Name).Contains(Filter, ESearchCase::IgnoreCase)
				|| FStringView(Command.Category).Contains(Filter, ESearchCase::IgnoreCase);
		}

		void RunHelp(UWorld* /*World*/, TConstArrayView<FString> Args, FOutputDevice& Output)
		{
			WriteHelp(Args.IsEmpty() ? FStringView() : FStringView(Args[0]), Output);
		}
	}

	TConstArrayView<FVeyraDevCommand> Catalog()
	{
		static const TArray<FVeyraDevCommand> Commands = [] {
			TArray<FVeyraDevCommand> All;
			AddVanguardCommands(All);
			AddEconomyCommands(All);
			AddMatchCommands(All);
			AddHelpCommands(All);
			// Help lists by category, so each category's commands sit together, in the order they were added.
			TArray<const TCHAR*> Categories;
			for (const FVeyraDevCommand& Command : All)
			{
				if (!Categories.ContainsByPredicate([&Command](const TCHAR* Seen) { return FCString::Strcmp(Seen, Command.Category) == 0; }))
				{
					Categories.Add(Command.Category);
				}
			}
			Algo::StableSortBy(All, [&Categories](const FVeyraDevCommand& Command) {
				return Categories.IndexOfByPredicate([&Command](const TCHAR* Category) { return FCString::Strcmp(Category, Command.Category) == 0; });
			});
			return All;
		}();
		return Commands;
	}

	const FVeyraDevCommand* Find(FStringView Name)
	{
		return Catalog().FindByPredicate([Name](const FVeyraDevCommand& Command) { return Name.Equals(Command.Name, ESearchCase::IgnoreCase); });
	}

	FString ConsoleName(const FVeyraDevCommand& Command)
	{
		return FString(ConsolePrefix) + Command.Name;
	}

	FString UsageReply(FStringView Name)
	{
		const FVeyraDevCommand* Command = Find(Name);
		return Command ? TEXT("Usage: ") + Synopsis(*Command) : FString(TEXT("Veyra.Dev.Help lists the developer commands."));
	}

	void Register()
	{
		IConsoleManager& Console = IConsoleManager::Get();
		for (const FVeyraDevCommand& Command : Catalog())
		{
			// The catalog is built once and never changes, so its rows outlive the console's delegates.
			const FVeyraDevCommand* Row = &Command;
			const FString Help = FString::Printf(TEXT("%s Usage: %s"), Command.Summary, *Synopsis(Command));
			IConsoleObject* Object = Console.RegisterConsoleCommand(*ConsoleName(Command), *Help,
				FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda([Row](const TArray<FString>& Args, UWorld* World, FOutputDevice& Output) {
					Execute(*Row, Args, World, Output);
				}),
				ECVF_Cheat);
			if (Object)
			{
				RegisteredObjects().Add(Object);
			}
		}
		VeyraDeveloperCommandRoute::SetHandler([](AVeyraPlayerController& Requester, const FString& Name, const TArray<FString>& Args) {
			return RunOnServer(Requester, Name, Args);
		});
	}

	void Unregister()
	{
		VeyraDeveloperCommandRoute::SetHandler({});
		for (IConsoleObject* Object : RegisteredObjects())
		{
			IConsoleManager::Get().UnregisterConsoleObject(Object, /*bKeepState*/ false);
		}
		RegisteredObjects().Reset();
	}

	void WriteHelp(FStringView Filter, FOutputDevice& Output)
	{
		TArray<const FVeyraDevCommand*> Listed;
		int32 Width = 0;
		for (const FVeyraDevCommand& Command : Catalog())
		{
			if (MatchesFilter(Command, Filter))
			{
				Listed.Add(&Command);
				Width = FMath::Max(Width, Synopsis(Command).Len());
			}
		}
		if (Listed.IsEmpty())
		{
			Output.Logf(TEXT("No developer command matches \"%.*s\". Veyra.Dev.Help lists them all."), Filter.Len(), Filter.GetData());
			return;
		}

		Output.Log(TEXT("Veyra developer commands, in development builds only. Server commands act on your own Vanguard,"));
		Output.Log(TEXT("and the server's reply prints here. <required> [optional]; \"Veyra.Dev.Help <word>\" filters."));
		const TCHAR* Category = nullptr;
		for (const FVeyraDevCommand* Command : Listed)
		{
			if (!Category || FCString::Strcmp(Category, Command->Category) != 0)
			{
				Category = Command->Category;
				Output.Logf(TEXT("%s"), Category);
			}
			Output.Logf(TEXT("  %s  %s"), *Synopsis(*Command).RightPad(Width), Command->Summary);
		}
	}

	void Execute(const FVeyraDevCommand& Command, TConstArrayView<FString> Args, UWorld* World, FOutputDevice& Output)
	{
		if (Command.LocalRun)
		{
			Command.LocalRun(World, Args, Output);
			return;
		}
		AVeyraPlayerController* Typist = LocalTypist(World);
		if (!Typist)
		{
			Output.Logf(TEXT("%s acts on your Vanguard, so it needs a match with a local player."), *ConsoleName(Command));
			return;
		}
		TArray<FString> Sent(Args.GetData(), Args.Num());
		if (Command.Prepare)
		{
			if (const TOptional<FString> Refusal = Command.Prepare(*Typist, Sent))
			{
				Output.Log(*Refusal.GetValue());
				return;
			}
		}
		Typist->RequestDeveloperCommand(Command.Name, Sent);
	}

	FString RunOnServer(AVeyraPlayerController& Requester, FStringView Name, TConstArrayView<FString> Args)
	{
		const FVeyraDevCommand* Command = Find(Name);
		if (!Command || !Command->ServerRun)
		{
			return FString::Printf(TEXT("This server has no developer command %s%.*s."), ConsolePrefix, Name.Len(), Name.GetData());
		}
		return Command->ServerRun(Requester, Args);
	}

	void Request(AVeyraPlayerController& Typist, const TCHAR* Name, TArray<FString> Args)
	{
		Typist.RequestDeveloperCommand(Name, Args);
	}

	TOptional<double> ParseNumber(TConstArrayView<FString> Args, int32 Index)
	{
		if (!Args.IsValidIndex(Index) || !Args[Index].IsNumeric())
		{
			return {};
		}
		const double Value = FCString::Atod(*Args[Index]);
		return FMath::IsFinite(Value) ? TOptional<double>(Value) : TOptional<double>();
	}

	TOptional<int32> ParseWhole(TConstArrayView<FString> Args, int32 Index)
	{
		if (!Args.IsValidIndex(Index) || !Args[Index].IsNumeric() || Args[Index].Contains(TEXT(".")))
		{
			return {};
		}
		const int64 Value = FCString::Atoi64(*Args[Index]);
		return Value >= TNumericLimits<int32>::Min() && Value <= TNumericLimits<int32>::Max() ? TOptional<int32>(static_cast<int32>(Value)) : TOptional<int32>();
	}

	TOptional<bool> ParseSwitch(TConstArrayView<FString> Args, bool Current)
	{
		if (Args.IsEmpty())
		{
			return !Current;
		}
		// FString's == ignores case.
		const FString& Word = Args[0];
		if (Word == TEXT("on") || Word == TEXT("1") || Word == TEXT("true"))
		{
			return true;
		}
		if (Word == TEXT("off") || Word == TEXT("0") || Word == TEXT("false"))
		{
			return false;
		}
		return {};
	}

	AVeyraPlayerController* LocalTypist(const UWorld* World)
	{
		// A dedicated server's first controller is a remote player's, which no one types for here.
		AVeyraPlayerController* Controller = World ? Cast<AVeyraPlayerController>(World->GetFirstPlayerController()) : nullptr;
		return Controller && Controller->IsLocalController() ? Controller : nullptr;
	}

	AVeyraPlayerState* ParticipantOf(const AVeyraPlayerController& Requester)
	{
		return Requester.GetPlayerState<AVeyraPlayerState>();
	}

	UAbilitySystemComponent* AbilitySystemOf(const AVeyraPlayerController& Requester)
	{
		const AVeyraPlayerState* Participant = ParticipantOf(Requester);
		return Participant ? Participant->GetAbilitySystemComponent() : nullptr;
	}

	void AddHelpCommands(TArray<FVeyraDevCommand>& Out)
	{
		Out.Add(FVeyraDevCommand::Local(TEXT("Help"), TEXT("Help"), TEXT("[filter]"),
			TEXT("Lists the developer commands, or those whose name or category contains the filter."), &RunHelp));
	}
}
