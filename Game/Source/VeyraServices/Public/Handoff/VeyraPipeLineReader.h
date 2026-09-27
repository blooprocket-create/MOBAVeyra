// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "HAL/Platform.h"
#include "Misc/Optional.h"

/** What one look at a pipe found. */
enum class EVeyraPipeRead : uint8
{
	/** No complete line yet. */
	Pending,
	/** A line was read. */
	Line,
	/** The writer closed the pipe, and no line is left. */
	EndOfInput,
	/** A line grew past the size limit without ending. */
	TooLong,
	/** The handle is not a pipe: a console, a file or nothing at all. */
	NotAPipe,
	/** The platform reported an error. */
	Failed,
};

/**
 * Reads lines from a pipe without blocking. A game receives its launch code (ADR-005 L3) and a
 * match server its assignment (ADR-007 §5) as one line on standard input, which must be a pipe: a
 * console would block and a file would leave the secret at rest.
 */
class VEYRASERVICES_API FVeyraPipeLineReader
{
public:
#if PLATFORM_WINDOWS
	/** A Windows HANDLE. */
	using FNativeHandle = void*;
#else
	/** A file descriptor. */
	using FNativeHandle = int;
#endif

	/** The longest line accepted, in bytes: a protocol limit far above any assignment. */
	static constexpr int32 MaxLineBytes = 64 * 1024;

	/** A reader of this process's standard input. */
	static FVeyraPipeLineReader ForStandardInput();

	/** A reader of Handle, which the caller owns and keeps open while reading. */
	explicit FVeyraPipeLineReader(FNativeHandle InHandle);

	/**
	 * Reads what the pipe holds and returns the next line in OutLine, decoded from UTF-8 and without
	 * its LF or CR LF. A final line that the writer ends by closing the pipe counts as a line. After
	 * anything but Pending or Line, every later call returns the same.
	 */
	EVeyraPipeRead Poll(FString& OutLine);

private:
	/** Appends what the pipe holds to Buffer. Pending means it may have read something and the pipe is still open. */
	EVeyraPipeRead ReadAvailable();

	/** Moves the first complete line out of Buffer. */
	bool TakeLine(FString& OutLine);

	FNativeHandle Handle;
	TArray<uint8> Buffer;
	TOptional<EVeyraPipeRead> Finished;
};
