#pragma once

#include "../../Flare.h"
#include "Widgets/SCompoundWidget.h"

class UFlareCompany;
class AFlarePlayerController;

class SFlareCompanyHelpInfo : public SCompoundWidget
{
	/*----------------------------------------------------
		Slate arguments
	----------------------------------------------------*/

	SLATE_BEGIN_ARGS(SFlareCompanyHelpInfo)
		: _CompanyDescription(NULL)
	{}

	SLATE_ARGUMENT(FFlareCompanyDescription*, CompanyDescription)
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

protected:

	/*----------------------------------------------------
		Callbacks
	----------------------------------------------------*/

	/** Get the name of the company */
	FText GetCompanyName() const;
	
	/** Get the company description text */
	FText GetCompanyDescription() const;

	/** Get the company AI personality text */
	FText GetCompanyPersonality() const;

	void SetCompanyBudgetPersonality();
	void SetCompanyOtherPersonality();
	void SetCompanyResourcesPersonality();
	
	FText GetCompanyBudgetPersonality() const;
	FText GetCompanyOtherPersonality() const;
	FText GetCompanyResourcesPersonality() const;

	/** Get the company emblem */
	const FSlateBrush* GetCompanyEmblem() const;

	void SetupEmblem();

	void SetupAIPersonality();

protected:

	/*----------------------------------------------------
		Protected data
	----------------------------------------------------*/

	//	UPROPERTY()
	//	TArray<UMaterialInstanceDynamic*>					  ForcedEmblemReference;

	// Game data
	TWeakObjectPtr<class AFlareMenuManager>				  MenuManager;

	UPROPERTY()
	FFlareCompanyDescription*                             CompanyDescription;

	UPROPERTY()
	UMaterialInstanceDynamic*							  CompanyEmblem;
	UPROPERTY()
	FSlateBrush							                  CompanyEmblemBrush;

	UPROPERTY()
	UFlareAIBehavior*								      Behavior;

	FText												  CompanyOtherPersonality;
	FText												  CompanyBudgetPersonality;
	FText												  CompanyResourcesPersonality;
};
