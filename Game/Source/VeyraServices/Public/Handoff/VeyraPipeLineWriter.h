// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Containers/StringView.h"
#include "Handoff/VeyraPipeLineReader.h"

/**
 * Writes lines to a pipe: the game's launch-handshake lines on its standard output (ADR-010 §5). It
 * writes only to a pipe, so a game started from a console or with no standard output writes nothing,
 * and it never blocks on or fails because of a launcher that has already gone. Windows only, where
 * the game client runs; on other platforms nothing is a pipe to it.
 */
class VEYRASERVICES_API FVeyraPipeLineWriter
{
public:
	using FNativeHandle = FVeyraPipeLineReader::FNativeHandle;

	/** A writer of this process's standard output. */
	static FVeyraPipeLineWriter ForStandardOutput();

	/** A writer of Handle, which the caller owns and keeps open while writing. */
	explicit FVeyraPipeLineWriter(FNativeHandle InHandle);

	/** Whether the handle is a pipe. */
	bool IsPipe() const;

	/** Writes Line, encoded as UTF-8, and an LF. False if the handle is not a pipe or the write failed. */
	bool WriteLine(FStringView Line);

private:
	FNativeHandle Handle;
};
