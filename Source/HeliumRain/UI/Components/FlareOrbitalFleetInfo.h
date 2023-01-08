#pragma once

#include "../../Flare.h"
#include "../Components/FlareList.h"

class UFlareCompany;
class UFlareSimulatedSpacecraft;

class SFlareOrbitalFleetInfo : public SCompoundWidget
{
	/*----------------------------------------------------
		Slate arguments
	----------------------------------------------------*/

	SLATE_BEGIN_ARGS(SFlareOrbitalFleetInfo){}

	SLATE_ARGUMENT(TWeakObjectPtr<class AFlareMenuManager>, MenuManager)
	
	SLATE_END_ARGS()


public:

	/*----------------------------------------------------
		Interaction
	----------------------------------------------------*/

	/** Create the widget */
	void Construct(const FArguments& InArgs);

	/** Clear the fleet list */
	void Clear();

	/** Update the fleet list */
	void Update();

	UFlareFleet* GetSelectedFleet();

protected:
	/** A fleet has been selected */
	void OnFleetSelected(TSharedPtr<FInterfaceContainer> SpacecraftContainer);
	/** A fleet has been unselected */
	void OnFleetUnSelected(TSharedPtr<FInterfaceContainer> SpacecraftContainer);

	/*----------------------------------------------------
		Protected data
	----------------------------------------------------*/

	// Menu manager
	TWeakObjectPtr<class AFlareMenuManager>  MenuManager;
	
	// Menu data
	TSharedPtr<SFlareList>                          FleetList;
	UFlareFleet*                                    SelectedFleet;
};