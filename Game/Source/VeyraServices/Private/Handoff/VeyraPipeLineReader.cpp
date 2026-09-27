// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Handoff/VeyraPipeLineReader.h"

#include "Math/UnrealMathUtility.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#else
#include <cerrno>
#include <poll.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

FVeyraPipeLineReader FVeyraPipeLineReader::ForStandardInput()
{
#if PLATFORM_WINDOWS
	return FVeyraPipeLineReader(::GetStdHandle(STD_INPUT_HANDLE));
#else
	return FVeyraPipeLineReader(STDIN_FILENO);
#endif
}

FVeyraPipeLineReader::FVeyraPipeLineReader(FNativeHandle InHandle)
	: Handle(InHandle)
{
}

EVeyraPipeRead FVeyraPipeLineReader::Poll(FString& OutLine)
{
	if (Finished.IsSet())
	{
		return *Finished;
	}
	if (TakeLine(OutLine))
	{
		return EVeyraPipeRead::Line;
	}
	if (Finished.IsSet())
	{
		return *Finished;
	}

	const EVeyraPipeRead Read = ReadAvailable();
	if (TakeLine(OutLine))
	{
		return EVeyraPipeRead::Line;
	}
	if (Finished.IsSet())
	{
		return *Finished;
	}
	if (Buffer.Num() > MaxLineBytes)
	{
		Finished = EVeyraPipeRead::TooLong;
		return *Finished;
	}
	if (Read == EVeyraPipeRead::EndOfInput && !Buffer.IsEmpty())
	{
		// The writer closed the pipe after a line with no newline.
		Buffer.Add(static_cast<uint8>('\n'));
		TakeLine(OutLine);
		Finished = EVeyraPipeRead::EndOfInput;
		return EVeyraPipeRead::Line;
	}
	if (Read != EVeyraPipeRead::Pending)
	{
		Finished = Read;
	}
	return Read;
}

bool FVeyraPipeLineReader::TakeLine(FString& OutLine)
{
	const int32 End = Buffer.Find(static_cast<uint8>('\n'));
	if (End == INDEX_NONE)
	{
		return false;
	}
	if (End > MaxLineBytes)
	{
		Finished = EVeyraPipeRead::TooLong;
		return false;
	}
	const int32 Length = (End > 0 && Buffer[End - 1] == static_cast<uint8>('\r')) ? End - 1 : End;
	const auto Converted = StringCast<TCHAR>(reinterpret_cast<const UTF8CHAR*>(Buffer.GetData()), Length);
	OutLine = FString::ConstructFromPtrSize(Converted.Get(), Converted.Length());
	Buffer.RemoveAt(0, End + 1, EAllowShrinking::No);
	return true;
}

EVeyraPipeRead FVeyraPipeLineReader::ReadAvailable()
{
	// Never hold more than one byte past the limit, which is enough to know a line is too long.
	const int32 Room = MaxLineBytes + 1 - Buffer.Num();
	const int32 Start = Buffer.Num();

#if PLATFORM_WINDOWS
	if (Handle == nullptr || Handle == INVALID_HANDLE_VALUE || ::GetFileType(Handle) != FILE_TYPE_PIPE)
	{
		return EVeyraPipeRead::NotAPipe;
	}
	DWORD Available = 0;
	if (!::PeekNamedPipe(Handle, nullptr, 0, nullptr, &Available, nullptr))
	{
		return ::GetLastError() == ERROR_BROKEN_PIPE ? EVeyraPipeRead::EndOfInput : EVeyraPipeRead::Failed;
	}
	if (Available == 0 || Room <= 0)
	{
		return EVeyraPipeRead::Pending;
	}
	const DWORD Wanted = FMath::Min<DWORD>(Available, static_cast<DWORD>(Room));
	Buffer.AddUninitialized(static_cast<int32>(Wanted));
	DWORD Got = 0;
	const bool bRead = ::ReadFile(Handle, Buffer.GetData() + Start, Wanted, &Got, nullptr) != 0;
	const DWORD Error = bRead ? 0 : ::GetLastError();
	Buffer.SetNum(Start + static_cast<int32>(Got), EAllowShrinking::No);
	if (!bRead)
	{
		return Error == ERROR_BROKEN_PIPE ? EVeyraPipeRead::EndOfInput : EVeyraPipeRead::Failed;
	}
	return EVeyraPipeRead::Pending;
#else
	struct stat Status;
	// A container's standard input is a pipe; some runtimes hand it over as a socket.
	if (Handle < 0 || fstat(Handle, &Status) != 0 || !(S_ISFIFO(Status.st_mode) || S_ISSOCK(Status.st_mode)))
	{
		return EVeyraPipeRead::NotAPipe;
	}
	pollfd Descriptor{ Handle, POLLIN, 0 };
	const int Ready = poll(&Descriptor, 1, 0);
	if (Ready < 0)
	{
		return errno == EINTR ? EVeyraPipeRead::Pending : EVeyraPipeRead::Failed;
	}
	if (Ready == 0 || Room <= 0)
	{
		return EVeyraPipeRead::Pending;
	}
	if ((Descriptor.revents & (POLLIN | POLLHUP)) == 0)
	{
		return EVeyraPipeRead::Failed;
	}
	Buffer.AddUninitialized(Room);
	const ssize_t Got = read(Handle, Buffer.GetData() + Start, static_cast<size_t>(Room));
	const int Error = errno;
	Buffer.SetNum(Start + (Got > 0 ? static_cast<int32>(Got) : 0), EAllowShrinking::No);
	if (Got == 0)
	{
		return EVeyraPipeRead::EndOfInput;
	}
	if (Got < 0)
	{
		return (Error == EINTR || Error == EAGAIN) ? EVeyraPipeRead::Pending : EVeyraPipeRead::Failed;
	}
	return EVeyraPipeRead::Pending;
#endif
}
