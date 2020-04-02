#pragma once

#include "../../Flare.h"
#include "../Components/FlareButton.h"
#include "../Components/FlareList.h"
#include "../Components/FlareListItem.h"
#include "../FlareUITypes.h"

class UFlareFactory;
class UFlareSimulatedSector;
class UFlareSkirmishManager;
class AFlareMenuManager;

class SFlareShipyardOverlay : public SCompoundWidget
{
	/*----------------------------------------------------
		Slate arguments
	----------------------------------------------------*/

	SLATE_BEGIN_ARGS(SFlareShipyardOverlay)
	{}

	SLATE_ARGUMENT(AFlareMenuManager*, MenuManager)

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

	/** Show the overlay */
	void Open();

	/** Is this open */
	bool IsOpen() const;

	/** Close the overlay */
	void Close();

	/** Refresh Shipyard List */
	void RefreshShipyardList();

	/*----------------------------------------------------
		Content callbacks
	----------------------------------------------------*/

	/** Get the company's wallet info */
	FText GetWalletText() const;

	/*----------------------------------------------------
		Action callbacks
	----------------------------------------------------*/

	/** Don't do anything and close */
	void OnClose();

protected:

	/*----------------------------------------------------
		Protected data
	----------------------------------------------------*/

	// State data
	UPROPERTY()
	TWeakObjectPtr<class AFlareMenuManager>                   MenuManager;

	TArray<UFlareSimulatedSpacecraft*>					      Shipyards;
	TSharedPtr<SFlareList>									  ShipyardList;

	// Slate data
	TSharedPtr<SFlareButton>                                  ConfirmButon;

};
