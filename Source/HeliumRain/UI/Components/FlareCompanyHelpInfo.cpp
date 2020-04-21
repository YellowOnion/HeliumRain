#include "FlareCompanyHelpInfo.h"
#include "../../Flare.h"
#include "../../Game/FlareCompany.h"
#include "../../Game/FlareGameTools.h"
#include "../../Player/FlarePlayerController.h"
#include "../FlareUITypes.h"
#include "FlareButton.h"

#include "../../Player/FlareMenuManager.h"
#include "../../UI/Menus/FlareLeaderboardMenu.h"
#include "../../Data/FlareCustomizationCatalog.h"
#include "../../Data/FlareResourceCatalog.h"

#include "../../Game/AI/FlareAIBehavior.h"
#include "../Components/FlareListItem.h"

#define LOCTEXT_NAMESPACE "FlareCompanyHelpInfo"


/*----------------------------------------------------
	Construct
----------------------------------------------------*/

void SFlareCompanyHelpInfo::Construct(const FArguments& InArgs)
{
	// Data
	CompanyDescription = InArgs._CompanyDescription;
	MenuManager = InArgs._MenuManager;
	SetupEmblem();
	SetupAIPersonality();
	const FFlareStyleCatalog& Theme = FFlareStyleSet::GetDefaultTheme();
	Behavior = NewObject<UFlareAIBehavior>(UFlareAIBehavior::StaticClass());
	Behavior->Load(CompanyDescription,MenuManager->GetGame());

//using these set methods because after a while of leaving the menu open the text starts spazzing out, maybe garbage collector kills something after a while? unknown
	SetCompanyBudgetPersonality();
	SetCompanyOtherPersonality();
	SetCompanyResourcesPersonality();

	// Create the layout
	ChildSlot
	.VAlign(VAlign_Top)
	.HAlign(HAlign_Fill)
	[
		SNew(SHorizontalBox)

		// Emblem
		+ SHorizontalBox::Slot()
		.Padding(Theme.ContentPadding)
		.AutoWidth()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Top)
		[
			SNew(SImage)
			.Image(this, &SFlareCompanyHelpInfo::GetCompanyEmblem)
		]

		// Data
		+ SHorizontalBox::Slot()
		.Padding(Theme.ContentPadding)
		.HAlign(HAlign_Fill)
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(0.5 * Theme.ContentWidth)
			[
				SNew(SVerticalBox)
				
				// Name
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(Theme.SmallContentPadding)
				[
					SNew(STextBlock)
					.Text(this, &SFlareCompanyHelpInfo::GetCompanyName)
					.TextStyle(&Theme.SubTitleFont)
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("CompanyBudget", "Budget Priorities\n"))
				.TextStyle(&Theme.NameFontBold)
				]

				+ SVerticalBox::Slot()
				.Padding(Theme.SmallContentPadding)
				.HAlign(HAlign_Left)
				.AutoHeight()
				[
					SNew(SBox)
					.WidthOverride(0.7 * Theme.ContentWidth)
					[
						SNew(STextBlock)
						.Text(this, &SFlareCompanyHelpInfo::GetCompanyBudgetPersonality)
						.WrapTextAt(0.7 * Theme.ContentWidth)
						.TextStyle(&Theme.NameFont)
					]
				]
			]
		]

		// Details
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Fill)
		.Padding(Theme.ContentPadding)
		[
			SNew(SVerticalBox)

			// Description
			+ SVerticalBox::Slot()
			.Padding(Theme.SmallContentPadding)
			.HAlign(HAlign_Left)
			.AutoHeight()
			[
				SNew(SBox)
				.WidthOverride(0.7 * Theme.ContentWidth)
				[
					SNew(STextBlock)
					.Text(this, &SFlareCompanyHelpInfo::GetCompanyDescription)
					.WrapTextAt(0.55 * Theme.ContentWidth)
					.TextStyle(&Theme.NameFont)
				]
			]		
			// Description

		+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("OtherAffinities", "Other Affinities\n"))
			.TextStyle(&Theme.NameFontBold)
			]

		// Data
		+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(Theme.SmallContentPadding)
			[
				SNew(STextBlock)
				.Text(this, &SFlareCompanyHelpInfo::GetCompanyOtherPersonality)
			.TextStyle(&Theme.TextFont)
			]
		]
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Right)
			.Padding(Theme.ContentPadding)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ResourcePriorities", "Resource Priorities\n"))
				.TextStyle(&Theme.NameFontBold)
				]

				+ SVerticalBox::Slot()
				.Padding(Theme.SmallContentPadding)
				.HAlign(HAlign_Left)
				.AutoHeight()
				[
					SNew(SBox)
					.WidthOverride(0.7 * Theme.ContentWidth)
					[
						SNew(STextBlock)
						.Text(this, &SFlareCompanyHelpInfo::GetCompanyResourcesPersonality)
						.WrapTextAt(0.7 * Theme.ContentWidth)
						.TextStyle(&Theme.NameFont)
					]
				]
			]
		];
}


/*----------------------------------------------------
	Interaction
----------------------------------------------------*/

/*----------------------------------------------------
	Callbacks
----------------------------------------------------*/

FText SFlareCompanyHelpInfo::GetCompanyName() const
{
	if (CompanyDescription)
	{
		return CompanyDescription->Name;
	}
	return FText();
}

FText SFlareCompanyHelpInfo::GetCompanyDescription() const
{
	FText Result;
	if (CompanyDescription)
	{
		Result = CompanyDescription->Description;
	}

	return Result;
}


FText SFlareCompanyHelpInfo::GetCompanyOtherPersonality() const
{
	return CompanyOtherPersonality;
}

void SFlareCompanyHelpInfo::SetCompanyOtherPersonality()
{
	FText PersonalityInfo;
	FText ShipyardAffility;
	FText ConsumerAffility;
	FText MaintenanceAffility;

	if (Behavior->ShipyardAffility <= 0)
	{
		ShipyardAffility = LOCTEXT("ShipyardAffinityNone", "Shipyard - None");
	}
	else if (Behavior->ShipyardAffility > 0 && Behavior->ShipyardAffility <= 0.50)
	{
		ShipyardAffility = LOCTEXT("ShipyardAffinityLow", "Shipyard - Low");
	}
	else if (Behavior->ShipyardAffility > 0.50 && Behavior->ShipyardAffility <= 1.00)
	{
		ShipyardAffility = LOCTEXT("ShipyardAffinityModerate", "Shipyard - Moderate");
	}
	else if (Behavior->ShipyardAffility > 1 && Behavior->ShipyardAffility <= 3)
	{
		ShipyardAffility = LOCTEXT("ShipyardAffinityHigh", "Shipyard - High");
	}
	else if (Behavior->ShipyardAffility > 3)
	{
		ShipyardAffility = LOCTEXT("ShipyardAffinityVeryHigh", "Shipyard - Very High");
	}

	if (Behavior->ConsumerAffility <= 0)
	{
		ConsumerAffility = LOCTEXT("ConsumerAffinityNone", "Consumer - None");
	}
	else if (Behavior->ConsumerAffility > 0 && Behavior->ConsumerAffility <= 0.50)
	{
		ConsumerAffility = LOCTEXT("ConsumerAffinityLow", "Consumer - Low");
	}
	else if (Behavior->ConsumerAffility > 0.50 && Behavior->ConsumerAffility <= 1.00)
	{
		ConsumerAffility = LOCTEXT("ConsumerAffinityModerate", "Consumer - Moderate");
	}
	else if (Behavior->ConsumerAffility > 1 && Behavior->ConsumerAffility <= 3)
	{
		ConsumerAffility = LOCTEXT("ConsumerAffinityHigh", "Consumer - High");
	}
	else if (Behavior->ConsumerAffility > 3)
	{
		ConsumerAffility = LOCTEXT("ConsumerAffinityVeryHigh", "Consumer - Very High");
	}

	if (Behavior->MaintenanceAffility <= 0)
	{
		MaintenanceAffility = LOCTEXT("MaintenanceAffinityNone", "Maintenance - None");
	}
	else if (Behavior->MaintenanceAffility > 0 && Behavior->MaintenanceAffility <= 0.50)
	{
		MaintenanceAffility = LOCTEXT("MaintenanceAffinityLow", "Maintenance - Low");
	}
	else if (Behavior->MaintenanceAffility > 0.50 && Behavior->MaintenanceAffility <= 1.00)
	{
		MaintenanceAffility = LOCTEXT("MaintenanceAffinityModerate", "Maintenance - Moderate");
	}
	else if (Behavior->MaintenanceAffility > 1 && Behavior->MaintenanceAffility <= 3)
	{
		MaintenanceAffility = LOCTEXT("MaintenanceAffinityHigh", "Maintenance - High");
	}
	else if (Behavior->MaintenanceAffility > 3)
	{
		MaintenanceAffility = LOCTEXT("MaintenanceAffinityVeryHigh", "Maintenance - Very High");
	}

	PersonalityInfo = FText::Format(LOCTEXT("CompanyOtherSub", "\
	\u2022 {0}\n\
	\u2022 {1}\n\
	\u2022 {2}"),
	ShipyardAffility,
	ConsumerAffility,
	MaintenanceAffility);

	CompanyOtherPersonality = FText::Format(LOCTEXT("CompanyOtherPersonality", "{0}"),
	PersonalityInfo);

}

FText SFlareCompanyHelpInfo::GetCompanyBudgetPersonality() const
{
	return CompanyBudgetPersonality;
}

void SFlareCompanyHelpInfo::SetCompanyBudgetPersonality()
{
	FText PersonalityInfo;
	FText TechBudget;
	FText MilitaryBudget;
	FText StationsBudget;
	FText TradingBudget;

	if(Behavior->BudgetTechnologyWeight <= 0)
	{
		TechBudget = LOCTEXT("TechBudgetNone", "Technology - None");
	}
	else if (Behavior->BudgetTechnologyWeight > 0 && Behavior->BudgetTechnologyWeight <= 0.25)
	{
		TechBudget = LOCTEXT("TechBudgetLow", "Technology - Low");
	}
	else if (Behavior->BudgetTechnologyWeight > 0.25 && Behavior->BudgetTechnologyWeight <= 0.50)
	{
		TechBudget = LOCTEXT("TechBudgetModerate", "Technology - Moderate");
	}
	else if (Behavior->BudgetTechnologyWeight > 0.50 && Behavior->BudgetTechnologyWeight <= 0.75)
	{
		TechBudget = LOCTEXT("TechBudgetHigh", "Technology - High");
	}
	else if (Behavior->BudgetTechnologyWeight > 0.75)
	{
		TechBudget = LOCTEXT("TechBudgetVeryHigh", "Technology - Very High");
	}

	if (Behavior->BudgetMilitaryWeight <= 0)
	{
		MilitaryBudget = LOCTEXT("MilitaryBudgetNone", "Military - None");
	}
	else if (Behavior->BudgetMilitaryWeight > 0 && Behavior->BudgetMilitaryWeight <= 0.25)
	{
		MilitaryBudget = LOCTEXT("MilitaryBudgetLow", "Military - Low");
	}
	else if (Behavior->BudgetMilitaryWeight > 0.25 && Behavior->BudgetMilitaryWeight <= 0.50)
	{
		MilitaryBudget = LOCTEXT("MilitaryBudgetModerate", "Military - Moderate");
	}
	else if (Behavior->BudgetMilitaryWeight > 0.50 && Behavior->BudgetMilitaryWeight <= 0.75)
	{
		MilitaryBudget = LOCTEXT("MilitaryBudgetHigh", "Military - High");
	}
	else if (Behavior->BudgetMilitaryWeight > 0.75)
	{
		MilitaryBudget = LOCTEXT("MilitaryBudgetVeryHigh", "Military - Very High");
	}

	if (Behavior->BudgetStationWeight <= 0)
	{
		StationsBudget = LOCTEXT("StationsBudgetNone", "Stations - None");
	}
	else if (Behavior->BudgetStationWeight > 0 && Behavior->BudgetStationWeight <= 0.25)
	{
		StationsBudget = LOCTEXT("StationsBudgetLow", "Stations - Low");
	}
	else if (Behavior->BudgetStationWeight > 0.25 && Behavior->BudgetStationWeight <= 0.50)
	{
		StationsBudget = LOCTEXT("StationsBudgetModerate", "Stations - Moderate");
	}
	else if (Behavior->BudgetStationWeight > 0.50 && Behavior->BudgetStationWeight <= 0.75)
	{
		StationsBudget = LOCTEXT("StationsBudgetHigh", "Stations - High");
	}
	else if (Behavior->BudgetStationWeight > 0.75)
	{
		StationsBudget = LOCTEXT("StationsBudgetVeryHigh", "Stations - Very High");
	}

	if (Behavior->BudgetStationWeight <= 0)
	{
		TradingBudget = LOCTEXT("TradingBudgetNone", "Trading - None");
	}
	else if (Behavior->BudgetStationWeight > 0 && Behavior->BudgetStationWeight <= 0.25)
	{
		TradingBudget = LOCTEXT("TradingBudgetLow", "Trading - Low");
	}
	else if (Behavior->BudgetStationWeight > 0.25 && Behavior->BudgetStationWeight <= 0.50)
	{
		TradingBudget = LOCTEXT("TradingBudgetModerate", "Trading - Moderate");
	}
	else if (Behavior->BudgetStationWeight > 0.50 && Behavior->BudgetStationWeight <= 0.75)
	{
		TradingBudget = LOCTEXT("TradingBudgetHigh", "Trading - High");
	}
	else if (Behavior->BudgetStationWeight > 0.75)
	{
		TradingBudget = LOCTEXT("TradingBudgetVeryHigh", "Trading - Very High");
	}

	PersonalityInfo = FText::Format(LOCTEXT("CompanyBudgetSub", "\
	\u2022 {0}\n\
	\u2022 {1}\n\
	\u2022 {2}\n\
	\u2022 {3}"),
	TechBudget,
	MilitaryBudget,
	StationsBudget,
	TradingBudget);

	CompanyBudgetPersonality = FText::Format(LOCTEXT("CompanyBudgetPersonality", "{0}"),
	PersonalityInfo);
}

FText SFlareCompanyHelpInfo::GetCompanyPersonality() const
{
	FText PersonalityInfo;
	return FText::Format(LOCTEXT("CompanyPersonality", "{0}"),
	PersonalityInfo);
}

FText SFlareCompanyHelpInfo::GetCompanyResourcesPersonality() const
{
	return CompanyResourcesPersonality;
}

void SFlareCompanyHelpInfo::SetCompanyResourcesPersonality()
{
	FText PersonalityInfo;
	FText TechBudget;
	FString ResourceString;

	for (int32 ResourceIndex = 0; ResourceIndex < MenuManager->GetGame()->GetResourceCatalog()->Resources.Num(); ResourceIndex++)
	{
		FFlareResourceDescription* Resource = &MenuManager->GetGame()->GetResourceCatalog()->Resources[ResourceIndex]->Data;
		float ResourceAffinity = Behavior->GetResourceAffility(Resource);

		ResourceString += FString::Printf(TEXT("%s"), *Resource->Name.ToString());

		if (ResourceAffinity <= 0)
		{
			ResourceString += ": None";
		}
		else if (ResourceAffinity > 0 && ResourceAffinity <= 0.75)
		{
			ResourceString += ": Low";
		}
		else if (ResourceAffinity > 0.75 && ResourceAffinity <= 1.75)
		{
			ResourceString += ": Moderate";
		}
		else if (ResourceAffinity > 1.75 && ResourceAffinity <= 3)
		{
			ResourceString += ": High";
		}
		else if (ResourceAffinity > 3)
		{
			ResourceString += ": Very High";
		}
		ResourceString += "\n";
	}

	PersonalityInfo = FText::Format(LOCTEXT("CompanyResourcesSub", "\
{0}"),
FText::FromString(ResourceString));

	CompanyResourcesPersonality = FText::Format(LOCTEXT("CompanyResourcesPersonality", "{0}"),
	PersonalityInfo);
}

const FSlateBrush* SFlareCompanyHelpInfo::GetCompanyEmblem() const
{
	return &CompanyEmblemBrush;
}

void SFlareCompanyHelpInfo::SetupAIPersonality()
{}

void SFlareCompanyHelpInfo::SetupEmblem()
{
	// Create the parameter
	FVector2D EmblemSize = 128 * FVector2D::UnitVector;
	UMaterial* BaseEmblemMaterial = Cast<UMaterial>(FFlareStyleSet::GetIcon("CompanyEmblem")->GetResourceObject());
	CompanyEmblem = UMaterialInstanceDynamic::Create(BaseEmblemMaterial, MenuManager->GetWorld());
//	ForcedEmblemReference.Add(CompanyEmblem);

	UFlareCustomizationCatalog* Catalog = MenuManager->GetGame()->GetCustomizationCatalog();

	//  AddToRoot works for preventing garbage collection (crash), but ForcedEmblemReference is probably better...
	//  except it doesn't seem to respect the UPROPERTY() line
	CompanyEmblem->AddToRoot();

	// Setup the material
	CompanyEmblem->SetTextureParameterValue("Emblem", CompanyDescription->Emblem);
	CompanyEmblem->SetVectorParameterValue("BasePaintColor", CompanyDescription->CustomizationBasePaintColor);
	CompanyEmblem->SetVectorParameterValue("PaintColor", CompanyDescription->CustomizationPaintColor);
	CompanyEmblem->SetVectorParameterValue("OverlayColor", CompanyDescription->CustomizationOverlayColor);
	CompanyEmblem->SetVectorParameterValue("GlowColor", CompanyDescription->CustomizationLightColor);

	// Create the brush dynamically
	CompanyEmblemBrush.ImageSize = EmblemSize;
	CompanyEmblemBrush.SetResourceObject(CompanyEmblem);
}

#undef LOCTEXT_NAMESPACE
