// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER && WITH_VEYRA_UI

#include "Engine/Texture2D.h"
#include "Internationalization/StringTable.h"
#include "Internationalization/StringTableCore.h"
#include "Internationalization/StringTableRegistry.h"
#include "Shell/VeyraProfileModels.h"
#include "Shell/VeyraShellArt.h"
#include "Text/VeyraContentText.h"

namespace VeyraProfileArtTests
{
	// Veyra.UI.ProfileArt.*: the profile backgrounds' region panoramas (ADR-082 §10).
	TEST_CLASS(ProfileArt, "Veyra.UI")
	{
		TEST_METHOD(ARegionEntryNamesItsPanorama)
		{
			ASSERT_THAT(AreEqual(VeyraProfileModels::RegionOfEntry(TEXT("region_iron_march")), FString(TEXT("iron_march"))));
			ASSERT_THAT(IsTrue(VeyraProfileModels::RegionOfEntry(TEXT("vanguard_cairn")).IsEmpty() && VeyraProfileModels::RegionOfEntry(TEXT("default")).IsEmpty()));
			ASSERT_THAT(IsTrue(VeyraProfileModels::VanguardOfEntry(TEXT("region_iron_march")).IsEmpty()));
		}

		TEST_METHOD(EveryNamedRegionHasItsPanorama)
		{
			// Each region the text table names is imported, wide as a banner.
			int32 Regions = 0;
			for (const TCHAR* Region : { TEXT("iron_march"), TEXT("ember_basin"), TEXT("drowned_cantons"), TEXT("merrin"), TEXT("shatterdeep"), TEXT("wildwood"),
					 TEXT("buried_riverlands"), TEXT("reed_provinces"), TEXT("bramblehollow") })
			{
				ASSERT_THAT(IsTrue(VeyraContentText::RegionName(Region).IsFromStringTable(), Region));
				const UTexture2D* Panorama = VeyraShellArt::BackgroundOf(Region);
				ASSERT_THAT(IsNotNull(Panorama, Region));
#if WITH_EDITORONLY_DATA
				// Its source, as the editor's platform data may still be compiling.
				ASSERT_THAT(IsTrue(Panorama->Source.GetSizeX() > Panorama->Source.GetSizeY() * 2, Region));
#endif
				++Regions;
			}
			ASSERT_THAT(AreEqual(Regions, 9));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER && WITH_VEYRA_UI
