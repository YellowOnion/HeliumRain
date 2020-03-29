#pragma once

#include "../../Flare.h"
#include "../FlareUITypes.h"

struct FFlareSpacecraftDescription;
//struct FFlareSpacecraftSave;

class UFlareSimulatedSpacecraft;
class UFlareSkirmishManager;
class UFlareSimulatedSector;
class AFlareMenuManager;

class SFlareSpaceCraftOverlayInfo : public SCompoundWidget
{
	/*----------------------------------------------------
		Slate arguments
	----------------------------------------------------*/

	SLATE_BEGIN_ARGS(SFlareSpaceCraftOverlayInfo)
	{}

	SLATE_ARGUMENT(FFlareSpacecraftDescription*, Desc)
	SLATE_ARGUMENT(UFlareSimulatedSpacecraft*, TargetShipyard)
	SLATE_ARGUMENT(UFlareSkirmishManager*, TargetSkirmish)
	SLATE_ARGUMENT(bool, OrderIsConfig)

	SLATE_ARGUMENT(UFlareSimulatedSector*, TargetSector)
	SLATE_ARGUMENT(TWeakObjectPtr<class AFlareMenuManager>, MenuManager)
	SLATE_END_ARGS()

public:

	/*----------------------------------------------------
		Public methods
	----------------------------------------------------*/

	/** Create the widget */
	void Construct(const FArguments& InArgs);

	/*----------------------------------------------------
		Interaction
	----------------------------------------------------*/

	/** Set the company to display */

protected:

	/*----------------------------------------------------
		Callbacks
	----------------------------------------------------*/

	/** Get the name of the company */
	FText GetProductionText() const;
	FText GetProductionTime() const;
	FText GetSpacecraftInfo() const;
	
	void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	EVisibility GetProductionTimeVisibility() const;


protected:

	/*----------------------------------------------------
		Protected data
	----------------------------------------------------*/

	// Game data
	UPROPERTY()
	TWeakObjectPtr<class AFlareMenuManager>	 MenuManager;
	FFlareSpacecraftDescription*			 Desc;
	UFlareSimulatedSpacecraft*				 TargetShipyard;
	UFlareSkirmishManager*					 TargetSkirmish;
	UFlareSimulatedSector*                   TargetSector;
	FFlareSpacecraftSave					 SpaceCraftData;
	bool									 OrderIsConfig;

	//HUD STUFFS
	TSharedPtr<STextBlock>                   TextBlock;

	int										 ProductionTime = 0;
};
