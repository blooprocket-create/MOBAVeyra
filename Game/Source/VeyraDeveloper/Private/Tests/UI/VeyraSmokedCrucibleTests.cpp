// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER && WITH_VEYRA_UI

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/ActorTestSpawner.h"
#include "Components/TextBlock.h"
#include "Fonts/CompositeFont.h"
#include "Loading/VeyraLoadingScreen.h"
#include "Misc/Paths.h"
#include "Shell/VeyraShellBrushes.h"
#include "Shell/VeyraShellButton.h"
#include "Shell/VeyraShellFonts.h"
#include "Shell/VeyraShellLook.h"
#include "Shell/VeyraShellStyleSettings.h"
#include "Shell/VeyraShellSurface.h"
#include "Styling/CoreStyle.h"

namespace VeyraSmokedCrucibleTests
{
	// Veyra.UI.SmokedCrucible.*: the client's type and surfaces (ADR-078): the fonts the game ships, the roles set in them,
	// and the chamfered, brass-edged faces every surface and button draws.
	TEST_CLASS(SmokedCrucible, "Veyra.UI")
	{
		FActorTestSpawner Spawner;

		/** A screen's Slate tree, held so its widgets outlive the call that builds them. */
		TSharedPtr<SWidget> Slate;

		AFTER_EACH()
		{
			VeyraShellLook::Use(FVeyraShellLook());
		}

		TEST_METHOD(EachFamilyIsTheFontsTheGameShipsAndKeepsTheEnginesOtherScripts)
		{
			const UVeyraShellStyleSettings& Style = *GetDefault<UVeyraShellStyleSettings>();
			const TSharedRef<const FCompositeFont> Engine = FCoreStyle::GetDefaultFont();
			for (const EVeyraFontFamily Family : { EVeyraFontFamily::Display, EVeyraFontFamily::Interface, EVeyraFontFamily::Numeric })
			{
				const TArray<FVeyraFontFace>& Faces = Style.FacesOf(Family);
				const TSharedRef<const FCompositeFont> Font = VeyraShellFonts::FamilyFont(Family);
				ASSERT_THAT(IsTrue(!Faces.IsEmpty() && Font->DefaultTypeface.Fonts.Num() == Faces.Num(), TEXT("one typeface for each face")));
				for (const FVeyraFontFace& Face : Faces)
				{
					ASSERT_THAT(IsTrue(FPaths::FileExists(FPaths::ProjectContentDir() / Face.File), *Face.File));
					ASSERT_THAT(IsTrue(Font->DefaultTypeface.Fonts.ContainsByPredicate([&Face](const FTypefaceEntry& Entry) { return Entry.Name == Face.Typeface; })));
				}
				// A name in another alphabet still draws: the engine's other scripts and fallback stay.
				ASSERT_THAT(AreEqual(Font->SubTypefaces.Num(), Engine->SubTypefaces.Num()));
				ASSERT_THAT(IsTrue(VeyraShellFonts::FamilyFont(Family) == Font, TEXT("built once")));
			}
		}

		TEST_METHOD(AButtonsLabelIsSetInItsRolesFace)
		{
			const UVeyraShellStyleSettings& Style = *GetDefault<UVeyraShellStyleSettings>();
			UVeyraLoadingScreen* Screen = CreateWidget<UVeyraLoadingScreen>(&Spawner.GetWorld());
			ASSERT_THAT(IsNotNull(Screen));
			Slate = Screen->TakeWidget();
			const TArray<UVeyraShellButton*> Buttons = Screen->GetButtons();
			ASSERT_THAT(IsFalse(Buttons.IsEmpty()));
			const UTextBlock* Label = Cast<UTextBlock>(Buttons[0]->GetContent());
			ASSERT_THAT(IsNotNull(Label));
			const FSlateFontInfo Font = Label->GetFont();
			ASSERT_THAT(IsTrue(Font.CompositeFont == VeyraShellFonts::FamilyFont(Style.ButtonFace.Family), TEXT("the functional family")));
			ASSERT_THAT(IsTrue(Font.TypefaceFontName == Style.ButtonFace.Typeface));
		}

		TEST_METHOD(AFaceIsCutAtTwoCornersEdgedAndFilled)
		{
			FVeyraFaceLook Look;
			Look.Top = FLinearColor(1.0f, 0.0f, 0.0f, 1.0f);
			Look.Bottom = Look.Top;
			Look.Edge = FLinearColor(0.0f, 1.0f, 0.0f, 1.0f);
			Look.Tick = FLinearColor(0.0f, 0.0f, 1.0f, 1.0f);
			Look.Chamfer = 8.0f;
			const FVeyraFaceTexels Face = VeyraShellBrushes::Rasterise(Look);
			ASSERT_THAT(IsTrue(Face.Width > 0 && Face.Height > 0 && Face.Texels.Num() == Face.Width * Face.Height));
			// The top-left and bottom-right corners are cut away; the other two are square.
			ASSERT_THAT(AreEqual(static_cast<int32>(Face.At(0, 0).A), 0));
			ASSERT_THAT(AreEqual(static_cast<int32>(Face.At(Face.Width - 1, Face.Height - 1).A), 0));
			ASSERT_THAT(IsTrue(Face.At(Face.Width - 1, 0).A > 200 && Face.At(0, Face.Height - 1).A > 200));
			// The edge along a straight side, the tick over the cut, the fill inside.
			const FColor Side = Face.At(Face.Width / 2, 0);
			ASSERT_THAT(IsTrue(Side.G > 200 && Side.R < 40 && Side.B < 40, TEXT("the edge")));
			const int32 OnCut = FMath::RoundToInt32(Look.Chamfer * Face.TexelsPerUnit / 2.0f);
			const FColor Cut = Face.At(OnCut, OnCut);
			ASSERT_THAT(IsTrue(Cut.B > Cut.G && Cut.B > Cut.R && Cut.A > 0, TEXT("the tick")));
			const FColor Inside = Face.At(Face.Width / 2, Face.Height / 2);
			ASSERT_THAT(IsTrue(Inside.R > 200 && Inside.G < 40 && Inside.A == 255, TEXT("the fill")));
		}

		TEST_METHOD(AFaceIsMadeOnceAndItsCornersKeepTheirSize)
		{
			const int32 Before = VeyraShellBrushes::TextureCount();
			const FSlateBrush First = VeyraShellBrushes::Mask(10.0f, 0.5f);
			const FSlateBrush Again = VeyraShellBrushes::Mask(10.0f, 0.5f);
			ASSERT_THAT(IsTrue(First.GetResourceObject() != nullptr && First.GetResourceObject() == Again.GetResourceObject(), TEXT("shared")));
			ASSERT_THAT(IsTrue(VeyraShellBrushes::TextureCount() <= Before + 1));
			// Drawn as a nine-sliced box whose corners keep the cut, the edge and a unit more.
			ASSERT_THAT(IsTrue(First.DrawAs == ESlateBrushDrawType::Box));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(First.Margin.Left * First.ImageSize.X, 11.0f, 0.01f) && First.Margin.Left < 0.5f && First.Margin.Top < 0.5f));
			// A face says what it draws, so the focus outline can draw the same face with its own edge.
			FVeyraFaceLook Look;
			Look.Top = FLinearColor::White;
			Look.Bottom = FLinearColor::Gray;
			Look.Chamfer = 6.0f;
			const FVeyraFaceLook* Known = VeyraShellBrushes::LookOf(VeyraShellBrushes::Face(Look));
			ASSERT_THAT(IsTrue(Known && *Known == Look));
			ASSERT_THAT(IsNull(VeyraShellBrushes::LookOf(First), TEXT("a mask is not a face")));
		}

		TEST_METHOD(ASurfaceDrawsItsOutlineOverAFillTintedItsColour)
		{
			UVeyraLoadingScreen* Screen = CreateWidget<UVeyraLoadingScreen>(&Spawner.GetWorld());
			ASSERT_THAT(IsNotNull(Screen));
			UVeyraShellSurface* Surface = Screen->WidgetTree->ConstructWidget<UVeyraShellSurface>(UVeyraShellSurface::StaticClass());
			const FLinearColor Glass(0.1f, 0.2f, 0.3f, 0.8f);
			FSlateBrush Fill = VeyraShellBrushes::Mask(10.0f, 0.5f);
			Fill.TintColor = FSlateColor(Glass);
			Surface->SetBrush(Fill);
			Surface->SetOutline(VeyraShellBrushes::Outline(10.0f, FLinearColor::Yellow, FLinearColor::Yellow));
			const TSharedRef<SWidget> Built = Surface->TakeWidget();
			ASSERT_THAT(IsTrue(Surface->Background.TintColor.GetSpecifiedColor().Equals(Glass), TEXT("the fill's tint is the surface's colour")));
			ASSERT_THAT(IsTrue(Surface->GetOutline().GetResourceObject() != nullptr && Surface->GetOutline().DrawAs == ESlateBrushDrawType::Box));
			ASSERT_THAT(IsTrue(Built->GetType() != FName(TEXT("SBorder")), TEXT("its own widget, which draws the outline")));
		}

		TEST_METHOD(TheTypeAndTheSurfacesAreValidated)
		{
			const TArray<FString> Problems = GetDefault<UVeyraShellStyleSettings>()->Validate();
			ASSERT_THAT(IsTrue(Problems.IsEmpty(), FString::Join(Problems, TEXT(" | "))));
			// A new object starts from the project's values, so these are broken by hand.
			UVeyraShellStyleSettings* Broken = NewObject<UVeyraShellStyleSettings>(GetTransientPackage());
			Broken->DisplayFontFaces.Reset();
			Broken->NumericFontFaces.Add(FVeyraFontFace{ TEXT("Heavy"), TEXT("Veyra/Fonts/Missing.ttf") });
			Broken->BodyFace.Typeface = TEXT("Nonexistent");
			Broken->PanelChamfer = 0.0f;
			Broken->EdgeColor = FLinearColor::Transparent;
			Broken->SurfaceShade = 1.0f;
			const FString Named = FString::Join(Broken->Validate(), TEXT(" | "));
			for (const TCHAR* Field : { TEXT("DisplayFontFaces"), TEXT("NumericFontFaces"), TEXT("BodyFace"), TEXT("TitleFace"), TEXT("PanelChamfer"), TEXT("EdgeColor"),
					 TEXT("SurfaceShade") })
			{
				ASSERT_THAT(IsTrue(Named.Contains(Field), FString::Printf(TEXT("%s is not named in: %s"), Field, *Named)));
			}
		}
	};
}

#endif // WITH_AUTOMATION_WORKER && WITH_VEYRA_UI
