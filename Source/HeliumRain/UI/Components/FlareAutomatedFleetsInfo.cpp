
#include "FlareAutomatedFleetsInfo.h"
#include "../../Flare.h"
#include "../../Game/FlareGameTools.h"
#include "../../Game/FlareCompany.h"
#include "../../Player/FlareMenuManager.h"
#include "../../Player/FlarePlayerController.h"
#include "../../Game/FlareGame.h"

#define LOCTEXT_NAMESPACE "FlareAutomatedFleetsInfo"


/*----------------------------------------------------
	Construct
----------------------------------------------------*/

void SFlareAutomatedFleetsInfo::Construct(const FArguments& InArgs)
{
	// Data
	MenuManager = InArgs._MenuManager;
	const FFlareStyleCatalog& Theme = FFlareStyleSet::GetDefaultTheme();
	
	// Build structure
	ChildSlot
	.HAlign(HAlign_Left)
	.VAlign(VAlign_Fill)
	[
		SNew(SBox)
		.WidthOverride(0.6 * Theme.ContentWidth)
		.HAlign(HAlign_Fill)
		[
			SNew(SVerticalBox)

			// Automated fleets title
			+ SVerticalBox::Slot()
			.Padding(Theme.TitlePadding)
			.AutoHeight()
			[
				UFlareUITypes::Header(LOCTEXT("AutomatedFleets", "Automated fleets"))
			]

			// Automated fleets list
			+ SVerticalBox::Slot()
			.HAlign(HAlign_Left)
			.AutoHeight()
			[
				SAssignNew(AutomatedFleetList, SVerticalBox)
			]
		]
	];
}

/*----------------------------------------------------
	Interaction
----------------------------------------------------*/

void SFlareAutomatedFleetsInfo::Update()
{
	Clear();
	const FFlareStyleCatalog& Theme = FFlareStyleSet::GetDefaultTheme();

	// List auto-trading fleets
	int32 AutoTradingFleetCount = 0;
	for (UFlareFleet* Fleet : MenuManager->GetPC()->GetCompany()->GetCompanyFleets())
	{
		if (Fleet->IsAutoTrading())
		{
			AutoTradingFleetCount++;

			AutomatedFleetList->AddSlot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			.Padding(Theme.ContentPadding)
			[
				SNew(SVerticalBox)

				// Buttons
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)

					// Inspect
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SFlareButton)
						.Width(6)
						.Text(Fleet->GetFleetName())
						.HelpText(FText(LOCTEXT("InspectFleetHelp", "Inspect this fleet")))
						.OnClicked(this, &SFlareAutomatedFleetsInfo::OnInspectFleetClicked, Fleet)
					]
				]

				// Infos
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(Theme.SmallContentPadding)
				[
					SNew(SRichTextBlock)
					.TextStyle(&Theme.TextFont)
					.Text(this, &SFlareAutomatedFleetsInfo::GetFleetDetailText, Fleet)
					.WrapTextAt(Theme.ContentWidth / 2)
					.DecoratorStyleSet(&FFlareStyleSet::Get())
				]
			];
		}
	}

	// Default text
	if (AutoTradingFleetCount == 0)
	{
		AutomatedFleetList->AddSlot()
		.AutoHeight()
		.HAlign(HAlign_Right)
		.Padding(Theme.ContentPadding)
		[
			SNew(STextBlock)
			.TextStyle(&Theme.TextFont)
			.Text(LOCTEXT("NoAutoFleet", "No fleet."))
		];
	}
}

void SFlareAutomatedFleetsInfo::Clear()
{
	AutomatedFleetList->ClearChildren();
}

FText SFlareAutomatedFleetsInfo::GetFleetDetailText(UFlareFleet* Fleet) const
{
	int32 CreditsGain = (Fleet->GetData()->AutoTradeStatsDays > 0) ? (FMath::RoundToInt(0.01f * float(Fleet->GetData()->AutoTradeStatsMoneySell - Fleet->GetData()->AutoTradeStatsMoneyBuy) / float(Fleet->GetData()->AutoTradeStatsDays))) : 0;
	
	// Format result
	if (CreditsGain > 0)
	{
		return UFlareGameTools::AddLeadingSpace(FText::Format(LOCTEXT("FleetDetailsGain", "<TradeText>{0} credits per day</>"),
			FText::AsNumber(CreditsGain)),
			3);
	}
	else
	{
		return UFlareGameTools::AddLeadingSpace(FText::Format(LOCTEXT("FleetDetailsLoss", "<WarningText>{0} credits per day</>"),
			FText::AsNumber(CreditsGain)),
			3);
	}
}

void SFlareAutomatedFleetsInfo::OnInspectFleetClicked(UFlareFleet* Fleet)
{
	FFlareMenuParameterData Data;
	Data.Fleet = Fleet;
	MenuManager->OpenMenu(EFlareMenu::MENU_Fleet, Data);
}


#undef LOCTEXT_NAMESPACE