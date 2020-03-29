#pragma once

#include "../../Flare.h"
#include "../FlareUITypes.h"
#include "../Components/FlareListItem.h"

class AFlareGame;
class AFlareMenuManager;
class FInterfaceContainer;
class UFlareCompany;

class SFlareLeaderboardMenu : public SCompoundWidget
{
	/*----------------------------------------------------
		Slate arguments
	----------------------------------------------------*/

	SLATE_BEGIN_ARGS(SFlareLeaderboardMenu){}

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

	/** The selection was changed */
	void OnCompanySelectionChanged(TSharedPtr<FInterfaceContainer> Item, ESelectInfo::Type SelectInfo);
	void SetCompany(UFlareCompany* Company);

	/** Real-time tick */
	void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

protected:
	

	/*----------------------------------------------------
		Callbacks
	----------------------------------------------------*/

	/** Target item generator */
	TSharedRef<ITableRow> GenerateCompanyInfo(TSharedPtr<FInterfaceContainer> Item, const TSharedRef<STableViewBase>& OwnerTable);

	/** Get the window title */
	FText GetWindowTitle() const;

protected:

	/*----------------------------------------------------
		Protected data
	----------------------------------------------------*/

	// Game data
	AFlareGame*                                Game;
	TWeakObjectPtr<class AFlareMenuManager>    MenuManager;

	// Slate data
	TArray< TSharedPtr<FInterfaceContainer> >                   CompanyListData;
	TSharedPtr< SListView< TSharedPtr<FInterfaceContainer> > >  CompanyList;

	TSharedPtr<SFlareListItem>									PreviousSelection;
	TSharedPtr<FInterfaceContainer>								SelectedItem;
	
	UFlareCompany*										        SelectedCompany;

public:
	/** Get the currently selected company */
	virtual UFlareCompany* GetSelectedCompany() const;
};
	