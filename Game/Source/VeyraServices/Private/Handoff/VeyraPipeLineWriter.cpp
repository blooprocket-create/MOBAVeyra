// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Handoff/VeyraPipeLineWriter.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

FVeyraPipeLineWriter FVeyraPipeLineWriter::ForStandardOutput()
{
#if PLATFORM_WINDOWS
	return FVeyraPipeLineWriter(::GetStdHandle(STD_OUTPUT_HANDLE));
#else
	return FVeyraPipeLineWriter(-1);
#endif
}

FVeyraPipeLineWriter::FVeyraPipeLineWriter(FNativeHandle InHandle)
	: Handle(InHandle)
{
}

bool FVeyraPipeLineWriter::IsPipe() const
{
#if PLATFORM_WINDOWS
	return Handle != nullptr && Handle != INVALID_HANDLE_VALUE && ::GetFileType(Handle) == FILE_TYPE_PIPE;
#else
	// Writing to a pipe whose reader has gone raises SIGPIPE on these platforms; nothing here needs it.
	return false;
#endif
}

bool FVeyraPipeLineWriter::WriteLine(FStringView Line)
{
	if (!IsPipe())
	{
		return false;
	}
#if PLATFORM_WINDOWS
	const FTCHARToUTF8 Encoded(Line.GetData(), Line.Len());
	TArray<uint8> Bytes(reinterpret_cast<const uint8*>(Encoded.Get()), Encoded.Length());
	Bytes.Add(static_cast<uint8>('\n'));
	DWORD Written = 0;
	// A reader that has gone makes this fail (ERROR_NO_DATA or ERROR_BROKEN_PIPE) rather than block.
	return ::WriteFile(Handle, Bytes.GetData(), static_cast<DWORD>(Bytes.Num()), &Written, nullptr) != 0 && Written == static_cast<DWORD>(Bytes.Num());
#else
	return false;
#endif
}
