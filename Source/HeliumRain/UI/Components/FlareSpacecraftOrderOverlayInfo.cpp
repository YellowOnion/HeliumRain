
#include "FlareSpacecraftOrderOverlayInfo.h"
#include "FlareSpacecraftOrderOverlay.h"
#include "../../Flare.h"
#include "../../Player/FlareMenuManager.h"
#include "../../Player/FlarePlayerController.h"
#include "../FlareUITypes.h"
#include "../../Game/FlareGameTools.h"

#define LOCTEXT_NAMESPACE "FlareSpacecraftOrderOverlayInfo"

/*----------------------------------------------------
	Construct
----------------------------------------------------*/

void SFlareSpaceCraftOverlayInfo::Construct(const FArguments& InArgs)
{
	// Data
	MenuManager = InArgs._MenuManager;
	TargetSector = InArgs._TargetSector;	
	Desc = InArgs._Desc;
	TargetShipyard = InArgs._TargetShipyard;
	TargetSkirmish = InArgs._TargetSkirmish;
	OrderIsConfig = InArgs._OrderIsConfig;

	if (TargetShipyard)
	{
		SpaceCraftData = TargetShipyard->GetData();
	}

	const FFlareStyleCatalog& Theme = FFlareStyleSet::GetDefaultTheme();

	uint32 Width = 32;

	if (OrderIsConfig)
	{
		Width = 24;
	}

	if (TargetShipyard || TargetSkirmish)
	{
		// Regular ship-buying (no skirmish)
		if (TargetShipyard)
		{
			if (!OrderIsConfig)
			{
				// Production time
				ProductionTime = TargetShipyard->GetShipProductionTime(Desc->Identifier);
				ProductionTime += TargetShipyard->GetEstimatedQueueAndProductionDuration(Desc->Identifier, -1);
			}
		}
	}
	// Station-building
	else
	{
		FCHECK(TargetSector);
		ProductionTime = Desc->CycleCost.ProductionTime;
	}

	//create the layout
	ChildSlot
		.VAlign(VAlign_Top)
		.HAlign(HAlign_Fill)
		[
		SNew(SHorizontalBox)

			// Picture
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(Theme.ContentPadding)
		.VAlign(VAlign_Top)
		[
			SNew(SImage)
			.Image(&Desc->MeshPreviewBrush)
		]

	// Main infos
	+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(Theme.ContentPadding)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(Theme.SmallContentPadding)
		[
			SNew(STextBlock)
			.Text(Desc->Name)
		.TextStyle(&Theme.NameFont)
		]

	+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(Theme.SmallContentPadding)
		[
			SNew(SBox)
			.WidthOverride(Theme.ContentWidth)
			[
				SNew(STextBlock)
				.Text(Desc->Description)
			.TextStyle(&Theme.TextFont)
			.WrapTextAt(Theme.ContentWidth)
			]
		]

	+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(Theme.SmallContentPadding)
			[
				SNew(STextBlock)
				.Text(this, &SFlareSpaceCraftOverlayInfo::GetSpacecraftInfo)
			.TextStyle(&Theme.TextFont)
			]
		]

	// Costs
	+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(Theme.ContentPadding)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ProductionCost", "Production cost & duration"))
			.Visibility(!OrderIsConfig ? EVisibility::Visible : EVisibility::Collapsed)
			.TextStyle(&Theme.NameFont)
		]

	+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SAssignNew(TextBlock, STextBlock)
//			SNew(STextBlock)
			.Text(this, &SFlareSpaceCraftOverlayInfo::GetProductionText)

		.WrapTextAt(0.65 * Theme.ContentWidth)
		.TextStyle(&Theme.TextFont)
		]

	+ SVerticalBox::Slot()
		.AutoHeight()
		[
		SNew(STextBlock)
		.Text(this, &SFlareSpaceCraftOverlayInfo::GetProductionTime)
		.Visibility(this, &SFlareSpaceCraftOverlayInfo::GetProductionTimeVisibility)
//		.Visibility((!TargetSkirmish && !OrderIsConfig) ? EVisibility::Visible : EVisibility::Collapsed)
//		.Visibility((!TargetSkirmish && !OrderIsConfig && ProductionTime > 0) ? EVisibility::Visible : EVisibility::Collapsed)
		.WrapTextAt(0.65 * Theme.ContentWidth)
		.TextStyle(&Theme.TextFont)
		]
	]
];
}

void SFlareSpaceCraftOverlayInfo::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	if (TargetShipyard)
	{
		if (OrderIsConfig)
		{
			SpaceCraftData = TargetShipyard->GetData();
		}
	}
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
}

EVisibility SFlareSpaceCraftOverlayInfo::GetProductionTimeVisibility() const
{
	AFlareMenuManager* MenuManager = AFlareMenuManager::GetSingleton();

	if (IsEnabled() && ProductionTime > 0)
	{
		if (!TargetSkirmish && !OrderIsConfig)
		{
			return EVisibility::Visible;
		}
	}
	return EVisibility::Collapsed;
}

FText SFlareSpaceCraftOverlayInfo::GetSpacecraftInfo() const
{
	// Ship-building
	if (TargetShipyard || TargetSkirmish)
	{
		// Station
		if (Desc->OrbitalEngineCount == 0)
		{
			return FText::Format(LOCTEXT("FactoryStationFormat", "(Station, {0} factories)"),
				FText::AsNumber(Desc->Factories.Num()));
		}

		// Ship description
		else if (Desc->TurretSlots.Num())
		{
			uint32 CargoBayCount = Desc->CargoBayCount;
			uint32 CargoBayCapacity = Desc->CargoBayCapacity;
			if (CargoBayCount&&CargoBayCapacity && !TargetSkirmish)
			{
				return FText::Format(LOCTEXT("FactoryWeaponGunFormatCargo", "(Military ship, {0} turrets, {1} combat value, {2}x{3} cargo units)"),
					FText::AsNumber(Desc->TurretSlots.Num()), FText::AsNumber(Desc->CombatPoints),
					FText::AsNumber(CargoBayCount),
					FText::AsNumber(CargoBayCapacity));
			}
			else
			{
				return FText::Format(LOCTEXT("FactoryWeaponTurretFormat", "(Military ship, {0} turrets, {1} combat value)"),
					FText::AsNumber(Desc->TurretSlots.Num()), FText::AsNumber(Desc->CombatPoints));
			}
		}
		else if (Desc->GunSlots.Num())
		{
			uint32 CargoBayCount = Desc->CargoBayCount;
			uint32 CargoBayCapacity = Desc->CargoBayCapacity;
			if (CargoBayCount && CargoBayCapacity && !TargetSkirmish)
			{
				return FText::Format(LOCTEXT("FactoryWeaponGunFormatCargo", "(Military ship, {0} gun slots, {1} combat value, {2}x{3} cargo units)"),
					FText::AsNumber(Desc->GunSlots.Num()), FText::AsNumber(Desc->CombatPoints),
					FText::AsNumber(CargoBayCount),
					FText::AsNumber(CargoBayCapacity));
			}
			else
			{
				return FText::Format(LOCTEXT("FactoryWeaponGunFormat", "(Military ship, {0} gun slots, {1} combat value)"),
					FText::AsNumber(Desc->GunSlots.Num()), FText::AsNumber(Desc->CombatPoints));
			}
		}
		else
		{
			return FText::Format(LOCTEXT("FactoryTraderFormat", "(Trading ship, {0}x{1} cargo units)"),
				FText::AsNumber(Desc->CargoBayCount),
				FText::AsNumber(Desc->CargoBayCapacity));
		}
	}

	// Station-building
	else
	{
		FCHECK(TargetSector);
		return FText::Format(LOCTEXT("StationInfoFormat", "(Station, {0} factories)"), FText::AsNumber(Desc->Factories.Num()));
	}
}

FText SFlareSpaceCraftOverlayInfo::GetProductionTime() const
{
	// Ship-building

	return FText::Format(LOCTEXT("ProductionTimeFormat", "\u2022 {0} days"),
	FText::AsNumber(ProductionTime));
}

FText SFlareSpaceCraftOverlayInfo::GetProductionText() const
{

	FText ProductionCost;

	// Ship-building
	if (TargetShipyard || TargetSkirmish)
	{
		// Regular ship-buying (no skirmish)
		if (TargetShipyard)
		{
			if (OrderIsConfig)
			{
				if (SpaceCraftData.ShipyardOrderExternalConfig.Find(Desc->Identifier) != INDEX_NONE)
				{
					ProductionCost = LOCTEXT("ConfigEnabled", "Enabled");
				}
				else
				{
					ProductionCost = LOCTEXT("ConfigDisabled", "Disabled");
				}
			}
			else
			{
//				// Production time
//				ProductionTime = TargetShipyard->GetShipProductionTime(Desc->Identifier);
//				ProductionTime += TargetShipyard->GetEstimatedQueueAndProductionDuration(Desc->Identifier, -1);

				// Production cost
				if (MenuManager->GetPC()->GetCompany() == TargetShipyard->GetCompany())
				{
					ProductionCost = FText::Format(LOCTEXT("FactoryProductionResourcesFormat", "\u2022 {0}"), TargetShipyard->GetShipCost(Desc->Identifier));
				}
				else
				{
					int32 CycleProductionCost = UFlareGameTools::ComputeSpacecraftPrice(Desc->Identifier, TargetShipyard->GetCurrentSector(), true);
					ProductionCost = FText::Format(LOCTEXT("FactoryProductionCostFormat", "\u2022 {0} credits"), FText::AsNumber(UFlareGameTools::DisplayMoney(CycleProductionCost)));
				}
			}
		}

		// Skirmish
		else
		{
			ProductionCost = FText::Format(LOCTEXT("SkirmishCombatCost", "\u2022 {0}"), FText::AsNumber(Desc->CombatPoints));
		}
	}

	// Station-building
	else
	{
		FCHECK(TargetSector);

		// Add resources
		FString ResourcesString;
		for (int ResourceIndex = 0; ResourceIndex < Desc->CycleCost.InputResources.Num(); ResourceIndex++)
		{
			FFlareFactoryResource* FactoryResource = &Desc->CycleCost.InputResources[ResourceIndex];
			if (ResourcesString.Len())
			{
				ResourcesString += ", ";
			}
			ResourcesString += FString::Printf(TEXT("%u %s"), FactoryResource->Quantity, *FactoryResource->Resource->Data.Name.ToString()); // FString needed here
		}

		// Constraints
		FString ConstraintString;
		if (Desc->BuildConstraint.Contains(EFlareBuildConstraint::FreeAsteroid))
		{
			if (ConstraintString.Len())
			{
				ConstraintString += ", ";
			}
			ConstraintString += LOCTEXT("AsteroidNeeded", "a free asteroid").ToString();
		}
		if (Desc->BuildConstraint.Contains(EFlareBuildConstraint::SunExposure))
		{
			if (ConstraintString.Len())
			{
				ConstraintString += ", ";
			}
			ConstraintString += LOCTEXT("SunNeeded", "good sun exposure").ToString();
		}
		if (Desc->BuildConstraint.Contains(EFlareBuildConstraint::GeostationaryOrbit))
		{
			if (ConstraintString.Len())
			{
				ConstraintString += ", ";
			}
			ConstraintString += LOCTEXT("GeostationaryNeeded", "a geostationary orbit").ToString();
		}
		if (Desc->BuildConstraint.Contains(EFlareBuildConstraint::HideOnIce))
		{
			if (ConstraintString.Len())
			{
				ConstraintString += ", ";
			}
			ConstraintString += LOCTEXT("NonIcyNeeded", "a non-icy sector").ToString();
		}
		if (Desc->BuildConstraint.Contains(EFlareBuildConstraint::HideOnNoIce))
		{
			if (ConstraintString.Len())
			{
				ConstraintString += ", ";
			}
			ConstraintString += LOCTEXT("IcyNeeded", "an icy sector").ToString();
		}
		if (Desc->BuildConstraint.Contains(EFlareBuildConstraint::NoComplex))
		{
			if (ConstraintString.Len())
			{
				ConstraintString += ", ";
			}
			ConstraintString += LOCTEXT("NoComplexNeeded", "regular sector building").ToString();
		}
		if (Desc->BuildConstraint.Contains(EFlareBuildConstraint::SpecialSlotInComplex))
		{
			if (ConstraintString.Len())
			{
				ConstraintString += ", ";
			}
			ConstraintString += LOCTEXT("SpecialComplexNeeded", "a central complex slot").ToString();
		}
		if (ConstraintString.Len())
		{
			ConstraintString = LOCTEXT("ConstructioNRequirement", "\n\u2022 Requires").ToString() + " " + ConstraintString;
		}

		int32 CompanyStationCountInSector = 0;
		for (UFlareSimulatedSpacecraft* Station : TargetSector->GetSectorStations())
		{
			if (Station->GetCompany() == MenuManager->GetPC()->GetCompany())
			{
				++CompanyStationCountInSector;
			}
		}

		// Final text
		ProductionCost = FText::Format(LOCTEXT("StationCostFormat", "\u2022 Costs {0} credits ({1} stations in this sector) {3}\n\u2022 Completion requires {2}"),
			FText::AsNumber(UFlareGameTools::DisplayMoney(TargetSector->GetStationConstructionFee(Desc->CycleCost.ProductionCost, MenuManager->GetPC()->GetCompany()))),
			FText::AsNumber(CompanyStationCountInSector),
			FText::FromString(ResourcesString),
			FText::FromString(ConstraintString));
	}

	return ProductionCost;
}

/*----------------------------------------------------
	Interaction
----------------------------------------------------*/

/*----------------------------------------------------
	Callbacks
----------------------------------------------------*/

#undef LOCTEXT_NAMESPACE
