#pragma once

#include "../../Flare.h"
#include "../Components/FlareButton.h"
#include "../Components/FlareListItem.h"
#include "../Components/FlareList.h"
#include "../Components/FlareConfirmationBox.h"
#include "../../Spacecrafts/FlareSpacecraftComponent.h"
#include "../../Player/FlarePlayerController.h"
#include "../Components/FlareSpacecraftInfo.h"


class SFlareShipMenu : public SCompoundWidget
{
	/*----------------------------------------------------
		Slate arguments
	----------------------------------------------------*/

	SLATE_BEGIN_ARGS(SFlareShipMenu){}

	SLATE_ARGUMENT(TWeakObjectPtr<class AFlareMenuManager>, MenuManager)
	
	SLATE_END_ARGS()


public:

	/*----------------------------------------------------
		Interaction
	----------------------------------------------------*/

	/** Create the widget */
	void Construct(const FArguments& InArgs);

	/** Setup the widget with saved data */
	void Setup();

	/** Enter this menu */
	void Enter(UFlareSimulatedSpacecraft* Target, bool IsEditable = false);

	/** Exit this menu */
	void Exit();

	/** Load a spacecraft data and setup the interface */
	void LoadTargetSpacecraft();

	/** Load a ship */
	void LoadPart(FName InternalName);

	/** Update shipyard */
	void UpdateShipyard();

	/** Update complex production breakdown*/
	void UpdateProductionBreakdown();

	void CheckDockedShips(TArray<AFlareSpacecraft*> DockedShips);

protected:

	/*----------------------------------------------------
		Updates
	----------------------------------------------------*/

	/** Update the part list, preselect an item */
	void UpdatePartList(FFlareSpacecraftComponentDescription* SelectItem);

	/** Generate the factory menus */
	void UpdateFactoryList();

	/** Generate upgrade box */
	void UpdateUpgradeBox();


	/*----------------------------------------------------
		Complex
	----------------------------------------------------*/

	/** Generate complex box */
	void UpdateComplexList();

	/** Build a station */
	void OnBuildElementClicked(FName ConnectorName);

	/** Station selected */
	void OnBuildStationSelected(FFlareSpacecraftDescription* NewStationDescription);

	/** Scrap station */
	void OnScrapComplexElement(UFlareSimulatedSpacecraft* Spacecraft);

	/** Scrap station */
	void OnScrapConfirmed(UFlareSimulatedSpacecraft* Spacecraft);
	

	/*----------------------------------------------------
		Shipyards
	----------------------------------------------------*/

	/** Add the shipyard information */
	void UpdateShipyardList();

	/** Visibility of the ship-building interface */
	EVisibility GetShipyardVisibility() const;

	/** Visibility of the configure order button */
	EVisibility GetShipyardAllowExternalOrderConfigurationVisibility() const;

	/** Visibility of the allow external order button */
	EVisibility GetShipyardAllowExternalOrderVisibility() const;

	/** Visibility of the allow auto construction button */
	EVisibility GetAutoConstructionVisibility() const;

	/** Visibility of the S ship selector */
	bool IsShipSSelectorDisabled() const;

	/** Visibility of the L ship selector */
	bool IsShipLSelectorDisabled() const;

	/** Visibility of the cancel ship button */
	EVisibility GetCancelShipOrderVisibility(int32 Index) const;

	/** Get the text for light ship order */
	FText GetLightShipTextInfo() const;

	/** Get the text for heavy ship order */
	FText GetHeavyShipTextInfo() const;

	/** Get the status text for next order */
	FText GetShipOrderStatus(int32 Index) const;

	/** Get the text for configuration button */
	FText GetConfigurationText() const;

	/** Get the text for configuration button */
	FText GetConfigurationHelp() const;

	/** Get the text for externalorders button */
	FText GetExternalordersText() const;
	/** Get the text for externalorders button */
	FText GetExternalOrdersHelp() const;

	/** Get the text for autoconstruction button */
	FText GetAutoConstructionText() const;
	/** Get the text for autoconstruction button */
	FText GetAutoConstructionHelp() const;

	/** Order a spacecraft */
	void OnOpenSpacecraftOrder(bool IsHeavy);

	/** Change external config exceptions/allowances */
	void OnOpenExternalConfig();

	/** Cancel a spacecraft */
	void OnCancelSpacecraftOrder(int32 Index);

	/** Toggle external orders */
	void OnToggleAllowExternalOrders();

	/** Toggle automatic construction */
	void OnToggleAllowAutoConstruction();

	void OnSelectWhiteList();
	void OnRemoveWhiteList();

	/*----------------------------------------------------
		Content callbacks
	----------------------------------------------------*/
	
	/** Ship name */
	FText GetShipName() const;

	/** Can we validate new details */
	bool IsRenameDisabled() const;
	bool IsWhiteListSelectDisabled() const;
	bool IsWhiteListRemoveDisabled() const;

	/** Title icon callback */
	const FSlateBrush* GetTitleIcon() const;

	/** Title callback */
	FText GetTitleText() const;
	
	/** Can we edit */
	EVisibility GetFactoryControlsVisibility() const;

	/** Is it a station */
	EVisibility GetEngineVisibility() const;

	/** Is it editable  */
	EVisibility GetEditVisibility() const;

	/** Get a Slate brush for the RCS icon */
	const FSlateBrush* GetRCSIcon() const;

	/** RCS callback */
	FText GetRCSText() const;

	/** RCS health */
	FSlateColor GetRCSHealthColor() const;

	/** Get a Slate brush for the engine icon */
	const FSlateBrush* GetEngineIcon() const;

	/** Engine callback */
	FText GetEngineText() const;

	/** Engine health */
	FSlateColor GetEnginesHealthColor() const;

	/** Get a Slate brush for the weapon icons */
	const FSlateBrush* GetWeaponIcon(TSharedPtr<int32> Index) const;

	/** Weapon callback */
	FText GetWeaponText(TSharedPtr<int32> Index) const;

	/** Weapon health */
	FSlateColor GetWeaponHealthColor() const;

	/** Part list generator */
	TSharedRef<ITableRow> GeneratePartInfo(TSharedPtr<FInterfaceContainer> Item, const TSharedRef<STableViewBase>& OwnerTable);
	TSharedRef<SWidget> OnGenerateWhiteListComboLine(UFlareCompanyWhiteList* Item);
	void OnWhiteListComboLineSelectionChanged(UFlareCompanyWhiteList* Item, ESelectInfo::Type SelectInfo);

	/** Get upgrade info */
	static FText GetUpgradeInfo(UFlareSimulatedSpacecraft* Spacecraft);
	static FText GetUpgradeHelpInfo(UFlareSimulatedSpacecraft* Spacecraft);

	bool IsUpgradeStationDisabled(UFlareSimulatedSpacecraft* Spacecraft) const;


	/*----------------------------------------------------
		Action callbacks
	----------------------------------------------------*/

	/** Confirm details */
	void OnRename();

	/** Show engines */
	void ShowEngines();

	/** Show RCSs */
	void ShowRCSs();

	/** Show weapons */
	void ShowWeapons(TSharedPtr<int32> WeaponIndex);

	/** Chose a part */
	void OnPartPicked(TSharedPtr<FInterfaceContainer> Item, ESelectInfo::Type SelectInfo);

	/** Confirmed a part */
	void OnPartConfirmed();

	/** Cancelled a part */
	void OnPartCancelled();

	/** Cancelled an upgrade */
	void OnCancelUpgrade();

	/** Upgrade station */
	void OnUpgradeStationClicked(UFlareSimulatedSpacecraft* Spacecraft);

	FText GetShipUpgradeDetails() const;

protected:

	/*----------------------------------------------------
		Protected data
	----------------------------------------------------*/

	// Menu manager
	TWeakObjectPtr<class AFlareMenuManager>         MenuManager;
	
	//  UI
	bool                                            CanEdit;
	TSharedPtr<STextBlock>                          ObjectName;
	TSharedPtr<STextBlock>                          ObjectClassName;
	TSharedPtr<STextBlock>                          ObjectDescription;
	TSharedPtr<STextBlock>                          ObjectProductionBreakdown;
	TSharedPtr<SFlareSpacecraftInfo>                ObjectActionMenu;
	TSharedPtr<SFlareList>                          ShipList;
	TSharedPtr<SFlareList>                          OwnedList;
	
	TSharedPtr<SVerticalBox>                        ShipCustomizationBox;
	TSharedPtr<SHorizontalBox>                      WeaponButtonBox;
	TSharedPtr<SHorizontalBox>                      RenameBox;
	TSharedPtr<SEditableText>                       ShipName;

	TSharedPtr<SBox>									WhiteListSelectionBox;
	TSharedPtr<SFlareDropList<UFlareCompanyWhiteList*>> WhiteListDropBox;
	TArray<UFlareCompanyWhiteList*>						WhiteListOptions;
	UFlareCompanyWhiteList*								CurrentlySelectedWhiteList;

	// Spacecraft data
	UFlareSimulatedSpacecraft*                      TargetSpacecraft;
	FFlareSpacecraftDescription*					TargetDescription;
	FFlareSpacecraftSave*                           TargetSpacecraftData;
	FFlareSpacecraftComponentDescription*           RCSDescription;
	FFlareSpacecraftComponentDescription*           EngineDescription;
	TArray<FFlareSpacecraftComponentDescription*>   WeaponDescriptions;
	int32                                           CurrentWeaponGroupIndex;
	
	// List of parts being shown right now 
	TArray< FFlareSpacecraftComponentDescription* >               PartListData;
	TArray< TSharedPtr<FInterfaceContainer> >                     PartListDataShared;
	TSharedPtr< SListView< TSharedPtr<FInterfaceContainer> > >    PartList;

	// Parts list UI
	TSharedPtr<STextBlock>                          UpgradeTitle;
	TSharedPtr<STextBlock>                          CantUpgradeReason;
	TSharedPtr<SFlareListItem>                      PreviousSelection;
	TSharedPtr<SVerticalBox>                        ShipPartCustomizationBox;
	TSharedPtr<STextBlock>                          ShipPartPickerTitle;
	TSharedPtr<SFlareConfirmationBox>               BuyConfirmation;
	TSharedPtr<SHorizontalBox>                      PartCharacteristicBox;

	// Part list data
	int32                                           CurrentPartIndex;
	int32                                           CurrentEquippedPartIndex;
	int32                                           ShipPartIndex;
	FFlareSpacecraftComponentDescription*           CurrentEquippedPartDescription;

	// List
	TSharedPtr<SVerticalBox>                        ComplexList;
	TSharedPtr<SVerticalBox>                        ShipyardList;
	TSharedPtr<SVerticalBox>                        FactoryList;
	TSharedPtr<SVerticalBox>                        UpgradeBox;
	TSharedPtr<SFlareButton>                        AllowExternalOrdersButton;
	TSharedPtr<SFlareButton>                        AllowAutoConstructionButton;
	TSharedPtr<SFlareButton>						ExternalOrdersConfigurationButton;
	TSharedPtr<SImage>                              ComplexLayout;

	// Complex data
	FName                                           SelectedComplexStation;
	FName                                           SelectedComplexConnector;

	
public:

	/*----------------------------------------------------
		Getter
	----------------------------------------------------*/

	UFlareSimulatedSpacecraft* GetTargetSpacecraft()
	{
		return TargetSpacecraft;
	}
};
