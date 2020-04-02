
#include "FlareShipyardOverlay.h"
#include "../Components/FlareSpacecraftOrderOverlayInfo.h"
#include "../../Flare.h"

#include "../../Player/FlareMenuManager.h"
#include "../../Player/FlarePlayerController.h"
#include "../../Game/FlareGame.h"
//#include "../../Game/FlareGameTools.h"
#include "../Menus/FlareOrbitalMenu.h"

#include "SBackgroundBlur.h"

#define LOCTEXT_NAMESPACE "FlareShipyardOverlay"


/*----------------------------------------------------
	Construct
----------------------------------------------------*/

void SFlareShipyardOverlay::Construct(const FArguments& InArgs)
{
	// Data
	MenuManager = InArgs._MenuManager;
	const FFlareStyleCatalog& Theme = FFlareStyleSet::GetDefaultTheme();

	// Create the layout
	ChildSlot
	.VAlign(VAlign_Fill)
	.HAlign(HAlign_Fill)
	[
		SNew(SBox)
		[
			SNew(SBackgroundBlur)
			.BlurRadius(Theme.BlurRadius)
			.BlurStrength(Theme.BlurStrength)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.Padding(FMargin(0))
			[
				SNew(SBorder)
				.HAlign(HAlign_Center)
				.Padding(Theme.ContentPadding)
				.BorderImage(&Theme.BackgroundBrush)
				[
						SNew(SVerticalBox)
						+SVerticalBox::Slot()
						[
							// List Box
							SNew(SScrollBox)
							.Style(&Theme.ScrollBoxStyle)
							.ScrollBarStyle(&Theme.ScrollBarStyle)

							+ SScrollBox::Slot()
							.Padding(Theme.ContentPadding)
							[
								SAssignNew(ShipyardList, SFlareList)
								.MenuManager(MenuManager)
							.Title(LOCTEXT("Shipyards", "Shipyards"))
							]
						]

							// Buttons 
							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(Theme.ContentPadding)
							.HAlign(HAlign_Right)
							.VAlign(VAlign_Top)

							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot()
								.AutoWidth()
								.Padding(Theme.ContentPadding)
								[
									SNew(STextBlock)
									.TextStyle(&Theme.TextFont)
								.Visibility(EVisibility::Visible)
								.Text(this, &SFlareShipyardOverlay::GetWalletText)
								]

								+ SHorizontalBox::Slot()
								.AutoWidth()
								.Padding(Theme.ContentPadding)
								[
									SNew(SFlareButton)
									.Text(LOCTEXT("Close", "Close"))
									.HelpText(LOCTEXT("CloseInfo", "Close shipyard window"))
									.OnClicked(this, &SFlareShipyardOverlay::OnClose)
								]
						]
					]
				]
			]
		];
	SetVisibility(EVisibility::Collapsed);
}


/*----------------------------------------------------
	Interaction
----------------------------------------------------*/

void SFlareShipyardOverlay::Open()
{
	SetEnabled(true);
	SetVisibility(EVisibility::Visible);
	// Init station list
	RefreshShipyardList();
}

void SFlareShipyardOverlay::RefreshShipyardList()
{
	ShipyardList->Reset();
	Shipyards = MenuManager->GetGame()->GetGameWorld()->GetShipyards();
	for (int32 ShipyardIndex = 0; ShipyardIndex < Shipyards.Num(); ShipyardIndex++)
	{
		UFlareSimulatedSpacecraft* Shipyard = Shipyards[ShipyardIndex];
		if (!Shipyard)
		{
			continue;
		}

		UFlareSimulatedSector* ShipyardSector = Shipyard->GetCurrentSector();
		if (MenuManager->GetPC()->GetCompany()->HasVisitedSector(ShipyardSector))
		{
			ShipyardList->AddShip(Shipyard);
 		}
	}
	ShipyardList->RefreshList();
}

bool SFlareShipyardOverlay::IsOpen() const
{
	return (GetVisibility() == EVisibility::Visible);
}

void SFlareShipyardOverlay::Close()
{
	SetEnabled(false);
	SetVisibility(EVisibility::Collapsed);
	ShipyardList->Reset();
}

/*----------------------------------------------------
	Content callbacks
----------------------------------------------------*/

FText SFlareShipyardOverlay::GetWalletText() const
{
	if (MenuManager->GetPC())
	{
		FCHECK(MenuManager->GetPC()->GetCompany());

		return FText::Format(LOCTEXT("CompanyCurrentWallet", "You have {0} credits available."),
			FText::AsNumber(MenuManager->GetPC()->GetCompany()->GetMoney() / 100));
	}

	return FText();
}

/*----------------------------------------------------
	Action callbacks
----------------------------------------------------*/

void SFlareShipyardOverlay::OnClose()
{
	MenuManager->GetPC()->ClientPlaySound(MenuManager->GetPC()->GetSoundManager()->NegativeClickSound);
	MenuManager->GetOrbitMenu()->SetShipyardOpen(false);
	Close();
}

#undef LOCTEXT_NAMESPACE