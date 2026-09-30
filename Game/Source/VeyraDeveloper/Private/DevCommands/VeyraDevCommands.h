// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/StringView.h"
#include "Containers/UnrealString.h"
#include "Misc/Optional.h"

class AVeyraPlayerController;
class AVeyraPlayerState;
class FOutputDevice;
class UAbilitySystemComponent;
class UWorld;

/**
 * One developer command: Veyra.Dev.<Name> in the console (PROJECT_STRUCTURE.md "VeyraDeveloper").
 *
 * A server command is typed on a player's machine and runs on the server, with its authority, for
 * that player: through AVeyraPlayerController's developer request and VeyraDeveloperCommandRoute.
 * The server's reply prints in the player's console. A local command runs where it is typed.
 * Every command is a row of the one catalog, so Veyra.Dev.Help lists every one.
 *
 * Commands change the match through the owners' own verbs (Gold through Economy, damage through
 * Combat's pipeline, items through the shop), never by writing their state. Amounts come from the
 * command's arguments or the tuning, never from constants here.
 */
struct FVeyraDevCommand
{
	/** Server: runs the command for Requester, and returns what to tell them. */
	using FServerRun = FString (*)(AVeyraPlayerController& Requester, TConstArrayView<FString> Args);

	/** The typing machine: runs a local command, writing what it says to Output. */
	using FLocalRun = void (*)(UWorld* World, TConstArrayView<FString> Args, FOutputDevice& Output);

	/**
	 * The typing machine, before a server command travels: adds to Args what only this machine knows,
	 * such as the ground under the cursor. Returns why the command cannot go, or nothing.
	 */
	using FPrepare = TOptional<FString> (*)(AVeyraPlayerController& Typist, TArray<FString>& Args);

	/** The name after "Veyra.Dev.". */
	const TCHAR* Name = TEXT("");

	/** The heading Veyra.Dev.Help lists it under. */
	const TCHAR* Category = TEXT("");

	/** Its arguments, such as "<amount>", with optional ones in brackets; empty for none. */
	const TCHAR* Usage = TEXT("");

	/** One sentence on what it does. */
	const TCHAR* Summary = TEXT("");

	/** Exactly one of these is set. */
	FServerRun ServerRun = nullptr;
	FLocalRun LocalRun = nullptr;

	/** Server commands only, and optional. */
	FPrepare Prepare = nullptr;

	static FVeyraDevCommand Server(const TCHAR* InName, const TCHAR* InCategory, const TCHAR* InUsage, const TCHAR* InSummary, FServerRun InRun,
		FPrepare InPrepare = nullptr)
	{
		return FVeyraDevCommand{ InName, InCategory, InUsage, InSummary, InRun, nullptr, InPrepare };
	}

	static FVeyraDevCommand Local(const TCHAR* InName, const TCHAR* InCategory, const TCHAR* InUsage, const TCHAR* InSummary, FLocalRun InRun)
	{
		return FVeyraDevCommand{ InName, InCategory, InUsage, InSummary, nullptr, InRun, nullptr };
	}
};

namespace VeyraDevCommands
{
	/** Every developer command, in the order Veyra.Dev.Help lists them: by category, then as added. */
	TConstArrayView<FVeyraDevCommand> Catalog();

	/** The command called Name, without "Veyra.Dev.", ignoring case; null if there is none. */
	const FVeyraDevCommand* Find(FStringView Name);

	/** What the console calls Command: "Veyra.Dev." and its name. */
	FString ConsoleName(const FVeyraDevCommand& Command);

	/** "Usage: Veyra.Dev.<Name> <usage>", the reply to arguments a command cannot use. */
	FString UsageReply(FStringView Name);

	/**
	 * Adds every command to the console, and installs the server's end of the route. The module calls
	 * it as it starts, and Unregister as it shuts down.
	 */
	void Register();
	void Unregister();

	/** Writes the list Veyra.Dev.Help prints: every command whose name or category contains Filter, or all. */
	void WriteHelp(FStringView Filter, FOutputDevice& Output);

	/** Runs Command as typing it into World's console would. */
	void Execute(const FVeyraDevCommand& Command, TConstArrayView<FString> Args, UWorld* World, FOutputDevice& Output);

	/** Server: runs the server command called Name for Requester, and returns its reply. The route calls it. */
	FString RunOnServer(AVeyraPlayerController& Requester, FStringView Name, TConstArrayView<FString> Args);

	/**
	 * Owning client: asks the server to run the command called Name with Args for Typist, as typing it
	 * would, without its Prepare step. For scripted players and tests.
	 */
	void Request(AVeyraPlayerController& Typist, const TCHAR* Name, TArray<FString> Args = {});

	// Argument parsing. Each returns nothing when the argument is missing or unusable.

	/** Args[Index] as a finite number. */
	TOptional<double> ParseNumber(TConstArrayView<FString> Args, int32 Index);

	/** Args[Index] as a whole number. */
	TOptional<int32> ParseWhole(TConstArrayView<FString> Args, int32 Index);

	/**
	 * An on-off switch: "on", "off", "1", "0", "true" or "false" in Args[0], or no argument, which
	 * turns Current over.
	 */
	TOptional<bool> ParseSwitch(TConstArrayView<FString> Args, bool Current);

	/** The player controller of the player typing into World's console, or null where no local player plays a match. */
	AVeyraPlayerController* LocalTypist(const UWorld* World);

	// What a server command acts on: the requester's own participant.

	AVeyraPlayerState* ParticipantOf(const AVeyraPlayerController& Requester);
	UAbilitySystemComponent* AbilitySystemOf(const AVeyraPlayerController& Requester);

	// The catalog's parts, one per category, each in its own file.

	void AddHelpCommands(TArray<FVeyraDevCommand>& Out);
	void AddVanguardCommands(TArray<FVeyraDevCommand>& Out);
	void AddEconomyCommands(TArray<FVeyraDevCommand>& Out);
	void AddMatchCommands(TArray<FVeyraDevCommand>& Out);
}
