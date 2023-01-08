#pragma once

#include "../../Flare.h"
#include "../Components/FlareList.h"

class UFlareCompany;

class SFlareAutomatedFleetsInfo : public SCompoundWidget
{
	/*----------------------------------------------------
		Slate arguments
	----------------------------------------------------*/

	SLATE_BEGIN_ARGS(SFlareAutomatedFleetsInfo){}

	SLATE_ARGUMENT(TWeakObjectPtr<class AFlareMenuManager>, MenuManager)
	
	SLATE_END_ARGS()


public:

	/*----------------------------------------------------
		Interaction
	----------------------------------------------------*/

	/** Create the widget */
	void Construct(const FArguments& InArgs);
	
	/** Generate the automated fleet list */
	void Update();

	/** Clear the automated fleet list */
	void Clear();

	/** Info text */
	FText GetFleetDetailText(UFlareFleet* Fleet) const;

	/** Inspect fleet */
	void OnInspectFleetClicked(UFlareFleet* Fleet);


protected:

	/*----------------------------------------------------
		Protected data
	----------------------------------------------------*/

	// Menu manager
	TWeakObjectPtr<class AFlareMenuManager>  MenuManager;
	
	// Menu data
	TSharedPtr<SVerticalBox>                 AutomatedFleetList;
};