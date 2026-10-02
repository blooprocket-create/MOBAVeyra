// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER && WITH_VEYRA_UI

#include "Blueprint/UserWidget.h"
#include "Components/ActorTestSpawner.h"
#include "Shell/VeyraProfileModels.h"
#include "Shell/VeyraShellButton.h"
#include "Shell/VeyraShellScreen.h"
#include "Tests/Services/VeyraClientFlowTestRig.h"

namespace VeyraNameScreenTests
{
	using namespace VeyraClientFlowTests;
	using namespace VeyraProfileModels;

	// Fixture answers, independent of the committed backend configuration.
	FString ShownName(const TCHAR* Name, bool bFree, const TCHAR* NextJson = TEXT("null"))
	{
		return FString::Printf(TEXT("{\"displayName\":{\"name\":\"%s\",\"freeChangeAvailable\":%s,\"nextChangeAt\":%s,\"renameRequired\":false,")
								   TEXT("\"price\":{\"flux\":6000,\"refinedFlux\":600}}}"),
			Name, bFree ? TEXT("true") : TEXT("false"), NextJson);
	}

	const TCHAR* const NamePageChoices = TEXT("{\"settings\":{\"icon\":\"default\",\"background\":\"default\",\"featuredVanguardId\":null,\"showMatchHistory\":false},")
										 TEXT("\"catalog\":{\"icons\":[\"default\"],\"backgrounds\":[\"default\"],\"defaultIcon\":\"default\",\"defaultBackground\":\"default\",")
										 TEXT("\"featuredChoices\":[]}}");

	// Veyra.UI.NameScreen.*: the Profile page's Display Name section and the Choose Your Name screen (ADR-049 §4, §6),
	// clicked as the player would click them.
	TEST_CLASS(NameScreen, "Veyra.UI")
	{
		FActorTestSpawner Spawner;
		FClientFlowTestRig Rig;
		UVeyraShellScreen* Screen = nullptr;

		AFTER_EACH()
		{
			if (Screen)
			{
				Screen->Unbind();
			}
		}

		void Show()
		{
			Screen = CreateWidget<UVeyraShellScreen>(&Spawner.GetWorld());
			Screen->Bind(*Rig.Flow);
		}

		bool Press(const FText& Label)
		{
			UVeyraShellButton* Found = Screen->FindButton(Label);
			if (!Found || !Found->GetIsEnabled())
			{
				return false;
			}
			Found->Press();
			return true;
		}

		/** The Profile page, its choices, preview and name read, the name as Status says. */
		bool OpenProfilePage(const FString& Status)
		{
			return Press(PageLabel()) && Rig.Backend.Answer(TEXT("GET"), TEXT("/v1/me/profile-settings"), 200, NamePageChoices)
				&& Rig.Backend.Answer(TEXT("GET"), TEXT("/v1/me/display-name"), 200, Status);
		}

		TEST_METHOD(TheProfilePageChangesTheNameBehindAConfirmation)
		{
			ASSERT_THAT(IsTrue(Rig.ReachShell()));
			Show();
			ASSERT_THAT(IsTrue(OpenProfilePage(ShownName(TEXT("DevOne"), true))));
			ASSERT_THAT(IsTrue(Screen->DescribeText().Contains(TEXT("Your first change is free."))));
			const FVeyraDisplayNameModel Before = DescribeName(Rig.Flow->GetSnapshot(), FDateTime::UtcNow());
			ASSERT_THAT(AreEqual(1, Before.Offers.Num()));
			Screen->SetNameDraft(TEXT("Oneiric"));
			// The first click only asks, naming the price and the risk.
			ASSERT_THAT(IsTrue(Press(Before.Offers[0].Label)));
			ASSERT_THAT(IsNull(Rig.Backend.Find(TEXT("PUT"), TEXT("/v1/me/display-name"))));
			ASSERT_THAT(IsTrue(Screen->DescribeText().Contains(TEXT("becomes available to anyone at once"))));
			ASSERT_THAT(IsTrue(Press(ConfirmNameChangeLabel())));
			const FFlowTestBackend::FRequest* Request = Rig.Backend.Find(TEXT("PUT"), TEXT("/v1/me/display-name"));
			ASSERT_THAT(IsTrue(Request && Request->Body.Contains(TEXT("\"name\":\"Oneiric\"")) && Request->Body.Contains(TEXT("\"currency\":\"\""))));
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("PUT"), TEXT("/v1/me/display-name"), 200, ShownName(TEXT("Oneiric"), false, TEXT("\"2099-01-01T00:00:00Z\"")))));
			ASSERT_THAT(IsTrue(Screen->DescribeText().Contains(TEXT("Your name is changed."))));
			// Paid from now on, and not again until the cooldown ends.
			const FVeyraDisplayNameModel After = DescribeName(Rig.Flow->GetSnapshot(), FDateTime::UtcNow());
			ASSERT_THAT(IsTrue(After.Offers.Num() == 2 && !After.bCanChange));
			ASSERT_THAT(IsTrue(Screen->FindButton(After.Offers[0].Label) && !Screen->FindButton(After.Offers[0].Label)->GetIsEnabled()));
		}

		TEST_METHOD(APaidChangeNamesItsCurrency)
		{
			ASSERT_THAT(IsTrue(Rig.ReachShell()));
			Show();
			ASSERT_THAT(IsTrue(OpenProfilePage(ShownName(TEXT("DevOne"), false))));
			const FVeyraDisplayNameModel Model = DescribeName(Rig.Flow->GetSnapshot(), FDateTime::UtcNow());
			ASSERT_THAT(IsTrue(Model.bCanChange && Model.Offers.Num() == 2 && Model.Offers[1].Currency == TEXT("refinedFlux")));
			Screen->SetNameDraft(TEXT("Oneiric"));
			ASSERT_THAT(IsTrue(Press(Model.Offers[1].Label)));
			ASSERT_THAT(IsTrue(Press(ConfirmNameChangeLabel())));
			ASSERT_THAT(IsTrue(Rig.Backend.Find(TEXT("PUT"), TEXT("/v1/me/display-name"))->Body.Contains(TEXT("\"currency\":\"refinedFlux\""))));
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("PUT"), TEXT("/v1/me/display-name"), 409, ErrorBody(TEXT("insufficient_balance")))));
			ASSERT_THAT(IsTrue(Screen->DescribeText().Contains(TEXT("Not enough to pay for the change."))));
		}

		TEST_METHOD(AClaimedAccountChoosesANewNameBeforeTheShell)
		{
			Rig.bRenameRequired = true;
			ASSERT_THAT(IsTrue(Rig.ReachShell()));
			Show();
			ASSERT_THAT(IsTrue(Screen->DescribeText().Contains(TEXT("Choose a new name"))));
			ASSERT_THAT(IsNull(Screen->FindButton(PageLabel()), TEXT("nothing else until a name is chosen")));
			Screen->SetNameDraft(TEXT("Returned"));
			ASSERT_THAT(IsTrue(Press(ChooseNameLabel())));
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("PUT"), TEXT("/v1/me/display-name"), 200, ShownName(TEXT("Returned"), true))));
			ASSERT_THAT(IsNotNull(Screen->FindButton(PageLabel()), TEXT("the shell, once chosen")));
		}
	};
}

#endif
