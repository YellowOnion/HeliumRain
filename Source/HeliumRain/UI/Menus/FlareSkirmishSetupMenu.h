#pragma once

#include "../../Flare.h"
#include "../Components/FlareButton.h"
#include "../Components/FlareRoundButton.h"
#include "../Components/FlareDropList.h"
#include "../Components/FlareListItem.h"

#include "SBackgroundBlur.h"
#include "Engine.h"

struct FFlareCompanyDescription;
struct FFlareSectorCelestialBodyDescription;
class AFlareMenuManager;


class SFlareSkirmishSetupMenu : public SCompoundWidget
{
	/*----------------------------------------------------
		Slate arguments
	----------------------------------------------------*/

	SLATE_BEGIN_ARGS(SFlareSkirmishSetupMenu){}

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
	void Enter();

	/** Exit this menu */
	void Exit();


protected:
	
	/*----------------------------------------------------
		Content callbacks
	----------------------------------------------------*/

	FText GetAltitudeValue() const;

	FText GetAsteroidValue() const;

	FText GetDebrisValue() const;
	FText GetQuantityValue(TSharedPtr<FFlareSkirmishSpacecraftOrder> Order) const;

	FText GetPlayerFleetTitle() const;

	FText GetTotalShipCount() const;

	FText GetTotalShipReserveCount() const;

	FText GetEnemyFleetTitle() const;

	TSharedRef<ITableRow> OnGenerateSpacecraftLine(TSharedPtr<FFlareSkirmishSpacecraftOrder> Item, const TSharedRef<STableViewBase>& OwnerTable);	

	TSharedRef<SWidget> OnGenerateCompanyComboLine(FFlareCompanyDescription Item);
	FText OnGetCurrentCompanyComboLine() const;
	
	TSharedRef<SWidget> OnGeneratePlanetComboLine(FFlareSectorCelestialBodyDescription Item);
	FText OnGetCurrentPlanetComboLine() const;

	bool IsStartDisabled() const;

	FText GetAddToPlayerFleetText() const;

	FText GetAddToEnemyFleetText() const;

	FText GetCopyHelpText() const;
	FText GetReserveHelpText() const;

	bool IsCopyButtonDisabled(TSharedPtr<FFlareSkirmishSpacecraftOrder> Order) const;

	bool IsAddToPlayerFleetDisabled() const;

	bool IsAddToEnemyFleetDisabled() const;

	FText GetStartHelpText() const;


	/*----------------------------------------------------
		Callbacks
	----------------------------------------------------*/

	/** Asteroid density */
	void OnAsteroidSliderChanged(float Value, bool ForPlayer);

	/** Debris density */
	void OnDebrisSliderChanged(float Value, bool ForPlayer);
	
	/** Clear the fleet */
	void OnClearFleet(bool ForPlayer);

	/** Sort the player fleet */
	void OnSortPlayerFleet();

	/** Copy enemy fleet setup**/
	void OnMirrorFleet(bool ForPlayer);

	/** Do an automatic fleet for the enemy */
	void OnAutoCreateEnemyFleet();

	/** Start the process of ordering a new ship */
	void OnOrderShip(bool ForPlayer);

	/** Order a new ship */
	void OnOrderShipConfirmed(FFlareSpacecraftDescription* Spacecraft);

	/** Upgrade spacecraft */
	void OnUpgradeSpacecraft(TSharedPtr<FFlareSkirmishSpacecraftOrder> Order);

	/** Remove spacecraft */
	void OnRemoveSpacecraft(TSharedPtr<FFlareSkirmishSpacecraftOrder> Order);

	/** Duplicate spacecraft */
	void OnDuplicateSpacecraft(TSharedPtr<FFlareSkirmishSpacecraftOrder> Order, bool SpecifyPlayer = false, bool ForPlayer = false, bool UpdateList = true);

	void OnChangeReserve(TSharedPtr<FFlareSkirmishSpacecraftOrder> Order);
	void OnSetReserve(TSharedPtr<FFlareSkirmishSpacecraftOrder> Order);

	/** Change quantity slider */
	void OnQuantitySliderChanged(float Value, TSharedPtr<FFlareSkirmishSpacecraftOrder> Order);

	/** Attempt to set quantity */
	void QuantitySet(int32 Value, TSharedPtr<FFlareSkirmishSpacecraftOrder> Order, bool SetBar = false);


	// Upgrade callbacks
	void OnUpgradeEngine(TSharedPtr<FFlareSkirmishSpacecraftOrder> Order, FName Upgrade);
	void OnUpgradeRCS(TSharedPtr<FFlareSkirmishSpacecraftOrder> Order, FName Upgrade);
	void OnUpgradeWeapon(TSharedPtr<FFlareSkirmishSpacecraftOrder> Order, int32 GroupIndex, FName Upgrade);

	/** Close upgrade panel */
	void OnCloseUpgradePanel();

	/** Start engagement */
	void OnStartSkirmish();

	/** Quit the menu */
	void OnMainMenu();

	void UpdateShips();


	/*----------------------------------------------------
		Helpers
	----------------------------------------------------*/

	bool CanStartPlaying(FText& Reason) const;

	void SetOrderDefaults(TSharedPtr<FFlareSkirmishSpacecraftOrder> Order);


protected:

	/*----------------------------------------------------
		Protected data
	----------------------------------------------------*/

	int32 PlayerShips = 0;
	int32 EnemyShips = 0;

	int32 PlayerShipsReserved = 0;
	int32 EnemyShipsReserved = 0;

	// Data
	TWeakObjectPtr<class AFlareMenuManager>                  MenuManager;

	// Settings widgets
	TSharedPtr<SFlareDropList<FFlareCompanyDescription>>               CompanySelector;
	TSharedPtr<SFlareDropList<FFlareSectorCelestialBodyDescription>>   PlanetSelector;
	TSharedPtr<SSlider>                                                AltitudeSlider;
	TSharedPtr<SSlider>                                                AsteroidSlider;
	TSharedPtr<SSlider>                                                DebrisSlider;
	TSharedPtr<SFlareButton>                                           IcyButton;
	TSharedPtr<SFlareButton>                                           MetalDebrisButton;
	// Lists
	TSharedPtr<SListView<TSharedPtr<FFlareSkirmishSpacecraftOrder>>>   PlayerSpacecraftList;
	TSharedPtr<SListView<TSharedPtr<FFlareSkirmishSpacecraftOrder>>>   EnemySpacecraftList;

	// List data
	UPROPERTY()
	TArray<TSharedPtr<FFlareSkirmishSpacecraftOrder>>        PlayerSpacecraftListData;
	UPROPERTY()
	TArray<TSharedPtr<FFlareSkirmishSpacecraftOrder>>        EnemySpacecraftListData;

	// Upgrade widgets
	TSharedPtr<SBackgroundBlur>                              UpgradeBox;
	TSharedPtr<SVerticalBox>                                 OrbitalEngineBox;
	TSharedPtr<SVerticalBox>                                 RCSBox;
	TSharedPtr<SHorizontalBox>                               WeaponBox;

	// State
	bool                                                     IsOrderingForPlayer;

};
