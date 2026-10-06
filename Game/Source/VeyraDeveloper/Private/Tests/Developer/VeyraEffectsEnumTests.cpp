// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER

#include "Effects/VeyraEffectsEnum.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/PolyglotTextData.h"
#include "Internationalization/TextLocalizationManager.h"

namespace VeyraEffectsEnumTests
{
	// Veyra.Developer.EffectsEnum.*: the effects generator names a static switch's entry as it was authored, so an
	// editor running in another culture, which shows the entries translated, builds the same systems (ADR-063 §4).
	TEST_CLASS(EffectsEnum, "Veyra.Developer")
	{
		TEST_METHOD(AnEntryIsFoundByItsAuthoredNameWhateverTheCulture)
		{
			FInternationalization& Internationalization = FInternationalization::Get();
			FInternationalization::FCultureStateSnapshot Before;
			Internationalization.BackupCultureState(Before);
			const bool bChanged = Internationalization.SetCurrentCulture(TEXT("fr"));
			// A display name with a translation, as an engine enum's has in an editor running in that culture: editor
			// text, which follows the editor's own culture.
			FPolyglotTextData Polyglot(ELocalizedTextSourceCategory::Editor, TEXT("VeyraEffectsEnumTests"), TEXT("DirectSet"), TEXT("Direct Set"));
			Polyglot.AddLocalizedString(TEXT("fr"), TEXT("Reglage direct"));
			FTextLocalizationManager::Get().RegisterPolyglotTextData(Polyglot);
			const FText Translated = Polyglot.GetText();
			const TArray<FText> Names = { FText::AsCultureInvariant(TEXT("Random")), Translated };
			const FString Shown = Translated.ToString();
			const int32 Found = VeyraEffects::FindByAuthoredName(Names, TEXT("Direct Set"));
			const int32 FoundByTranslation = VeyraEffects::FindByAuthoredName(Names, TEXT("Reglage direct"));
			Internationalization.RestoreCultureState(Before);

			ASSERT_THAT(IsTrue(bChanged, TEXT("a culture to translate to")));
			ASSERT_THAT(AreEqual(FString(TEXT("Reglage direct")), Shown, TEXT("the entry shows translated in it")));
			ASSERT_THAT(AreEqual(1, Found, TEXT("found by its authored name")));
			ASSERT_THAT(AreEqual(INDEX_NONE, FoundByTranslation, TEXT("never by its translation")));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
