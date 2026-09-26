// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Hash/VeyraSha256.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraCoreTests
{
	// Veyra.Core.Sha256.*: the SHA-256 match servers use to check join tickets (ADR-007 §3).
	TEST_CLASS(Sha256, "Veyra.Core")
	{
		TEST_METHOD(MatchesTheStandardVectors)
		{
			// FIPS 180-2 test vectors.
			ASSERT_THAT(AreEqual(VeyraHash::Sha256Hex(TEXT("")), FString(TEXT("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"))));
			ASSERT_THAT(AreEqual(VeyraHash::Sha256Hex(TEXT("abc")), FString(TEXT("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"))));
		}

		TEST_METHOD(MatchesTheBackendsTicketHash)
		{
			// The fixed ticket vector in Backend/internal/match/ticket_test.go, so the game and the
			// backend hash tickets the same way.
			ASSERT_THAT(AreEqual(VeyraHash::Sha256Hex(TEXT("vjt_xdMWyGQJg9xC_-yn9b-5ZYoh8_KKDRs9Bfjlh7WgwKQ")),
				FString(TEXT("f9de9702e8e9c925cd96f865fcd36d6bdf45a76ca195068cd024ff2e28c9e737"))));
		}

		TEST_METHOD(HashesUtf8)
		{
			// "é" is two UTF-8 bytes, C3 A9. Expected value from Python's hashlib.
			ASSERT_THAT(AreEqual(VeyraHash::Sha256Hex(TEXT("é")), FString(TEXT("4a99557e4033c3539de2eb65472017cad5f9557f7a0625a09f1c3f6e2ba69c4c"))));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
