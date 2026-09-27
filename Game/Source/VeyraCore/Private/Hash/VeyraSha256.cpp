// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Hash/VeyraSha256.h"

#include "Containers/StringConv.h"
#include "Misc/StringBuilder.h"

THIRD_PARTY_INCLUDES_START
#include <openssl/sha.h>
THIRD_PARTY_INCLUDES_END

namespace VeyraHash
{
FString Sha256Hex(FStringView Text)
{
	const auto Utf8 = StringCast<UTF8CHAR>(Text.GetData(), Text.Len());
	uint8 Digest[SHA256_DIGEST_LENGTH];
	SHA256(reinterpret_cast<const unsigned char*>(Utf8.Get()), static_cast<size_t>(Utf8.Length()), Digest);
	TStringBuilder<2 * SHA256_DIGEST_LENGTH + 1> Hex;
	for (const uint8 Byte : Digest)
	{
		Hex.Appendf(TEXT("%02x"), Byte);
	}
	return FString(Hex.ToView());
}
}
