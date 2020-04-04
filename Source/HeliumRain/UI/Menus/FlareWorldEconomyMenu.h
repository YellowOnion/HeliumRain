#pragma once

#include "../../Flare.h"
#include "../Components/FlareButton.h"
#include "../Components/FlareDropList.h"
#include "../Components/FlareList.h"
#include "../Components/FlareListItem.h"
#include "../../Game/FlareWorldHelper.h"
#include "../../Data/FlareResourceCatalogEntry.h"
#include "../FlareUITypes.h"


class UFlareSimulatedSector;
class AFlareMenuManager;
struct FFlareResourceDescription;
class UFlareResourceCatalogEntry;

class SFlareWorldEconomyMenu : public SCompoundWidget
{
	/*----------------------------------------------------
		Slate arguments
	----------------------------------------------------*/

	SLATE_BEGIN_ARGS(SFlareWorldEconomyMenu){}

	SLATE_ARGUMENT(TWeakObjectPtr<class AFlareMenuManager>, MenuManager)

	SLATE_END_ARGS()


public:

	/*----------------------------------------------------
		Interaction
	----------------------------------------------------*/

	/** Create the widget */
	void Construct(const FArguments& InArgs);

	/** Setup the widget */
	void Setup();

	/** Enter this menu */
	void Enter(FFlareResourceDescription* Resource, UFlareSimulatedSector* Sector);

	/** Exit this menu */
	void Exit();

	void GenerateSectorList();


protected:

	/*----------------------------------------------------
		Callbacks
	----------------------------------------------------*/

	/** Get the current sort icon to use */
	const FSlateBrush* GetSortIcon(EFlareEconomySort::Type Type) const;

	/** Set the current sort type to use */
	void ToggleSortType(EFlareEconomySort::Type Type);

	FSlateColor GetPriceColor(UFlareSimulatedSector* Sector) const;

	/** Get the resource price info */
	FText GetResourceDescription() const;

	/** Get the company desc */
	FText GetCompanyDescription() const;

	/** Get the resource name */
	FText GetResourceName() const;

	/** Get the resource icon */
	const FSlateBrush* GetResourceIcon() const;

	/** Get the company icon */
	const FSlateBrush* GetCompanyEmblem() const;

	/** Get the resource usage & production info */
	FText GetResourceInfo() const;

	/** Get the sector info */
	FText GetSectorText(UFlareSimulatedSector* Sector) const;

	/** Get the sector info color */
	FSlateColor GetSectorTextColor(UFlareSimulatedSector* Sector) const;

	/** Get the resource production info */
	FText GetResourceProductionInfo(UFlareSimulatedSector* Sector) const;

	/** Get the resource consumption info */
	FText GetResourceConsumptionInfo(UFlareSimulatedSector* Sector) const;

	/** Get the resource stock info */
	FText GetResourceStockInfo(UFlareSimulatedSector* Sector) const;

	/** Get the resource capacity info */
	FText GetResourceCapacityInfo(UFlareSimulatedSector* Sector) const;

	/** Get the resource price info */
	FText GetResourcePriceInfo(UFlareSimulatedSector* Sector) const;

	/** Get the resource price variation info */
	FText GetResourcePriceVariationInfo(UFlareSimulatedSector* Sector, TSharedPtr<int32> MeanDuration) const;

	TSharedRef<SWidget> OnGenerateResourceComboLine(UFlareResourceCatalogEntry* Item);
	void OnResourceComboLineSelectionChanged(UFlareResourceCatalogEntry* Item, ESelectInfo::Type SelectInfo, bool Station=false);
	FText OnGetCurrentResourceComboLine() const;

	TSharedRef<SWidget> OnGenerateCompanyComboLine(UFlareCompany* Item);
	void OnCompanyComboLineSelectionChanged(UFlareCompany* Item, ESelectInfo::Type SelectInfo);
	FText OnGetCurrentCompanyComboLine() const;

	void OnOpenSector(UFlareSimulatedSector* Sector);

	void OnOpenSectorPrices(UFlareSimulatedSector* Sector);

	void OnIncludeTradingHubsToggle();

	/** Generate Station List */
	void GenerateStationList();
	
	/** Refresh Station List */
	void RefreshStationList();

	/** Update filters */
	void OnToggleShowFlags();

	/** Change input/output button */
	void OnInputOutput();

	/** Hide the station filters */
	EVisibility GetStationFiltersVisibility() const;

	/** Hide the station filters */
	EVisibility GetStationFiltersResourceVisibility() const;

	/** Hide the station filters */
	EVisibility GetStationFiltersCompanyVisibility() const;

protected:

	/*----------------------------------------------------
		Protected data
	----------------------------------------------------*/

	// Menu data
	TSharedPtr<class SFlareTabView>          TabView;

	// Target data
	TWeakObjectPtr<class AFlareMenuManager>         MenuManager;
	FFlareResourceDescription*                      TargetResource;
	UFlareCompany*									TargetCompany;
	TMap<FFlareResourceDescription*, WorldHelper::FlareResourceStats> WorldStats;

	// Slate data
	TSharedPtr<SVerticalBox>                        SectorList;
	TSharedPtr<SFlareDropList<UFlareResourceCatalogEntry*>> ResourceSelector;
	TSharedPtr<SFlareButton>                        IncludeTradingHubsButton;

	// Data
	bool                                            IsCurrentSortDescending;
	TEnumAsByte<EFlareEconomySort::Type>            CurrentSortType;

	TSharedPtr<SFlareDropList<UFlareResourceCatalogEntry*>> StationResourceSelector;
	TSharedPtr<SFlareDropList<UFlareCompany*>>              StationCompanySelector;
	TArray<UFlareCompany*>						            CompanyList;


	TArray<UFlareSimulatedSpacecraft*>						  StationsArray;
	TSharedPtr<SFlareList>									  StationList;

	TSharedPtr<SFlareButton>                                  FiltersButton;
	TSharedPtr<SFlareButton>                                  ResourceFiltersButton;
	TSharedPtr<SFlareButton>                                  CompanyFiltersButton;
	TSharedPtr<SFlareButton>                                  StorageHubButton;
	TSharedPtr<SFlareButton>                                  QuantityButton;
	TSharedPtr<SFlareButton>                                  DistanceButton;
	TSharedPtr<SFlareButton>                                  InputOutputButton;
	bool												      InputOutputMode;
};
