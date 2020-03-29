
#include "FlareList.h"
#include "../../Flare.h"

#include "../../Game/FlareGame.h"
#include "../../Game/FlareGameTools.h"
#include "../../Player/FlareMenuManager.h"
#include "../../Player/FlarePlayerController.h"
#include "../../UI/Menus/FlareFleetMenu.h"

#include "../../Spacecrafts/Subsystems/FlareSimulatedSpacecraftWeaponsSystem.h"

#define LOCTEXT_NAMESPACE "FlareList"


/*----------------------------------------------------
	Construct
----------------------------------------------------*/

void SFlareList::Construct(const FArguments& InArgs)
{
	// Data
	MenuManager = InArgs._MenuManager;
	UseCompactDisplay = InArgs._UseCompactDisplay;
	const FFlareStyleCatalog& Theme = FFlareStyleSet::GetDefaultTheme();
	AFlarePlayerController* PC = MenuManager->GetPC();
	OnItemSelected = InArgs._OnItemSelected;
	HasShips = false;
	HasFleets = false;
	// Build structure
	ChildSlot
	.VAlign(VAlign_Fill)
	.HAlign(HAlign_Left)
	[
		SNew(SBox)
		.WidthOverride(Theme.ContentWidth)
		[
			SNew(SVerticalBox)

			// Filters
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Fill)
			.Padding(Theme.TitlePadding)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				[
					SAssignNew(Title, STextBlock)
					.Text(InArgs._Title)
					.TextStyle(&FFlareStyleSet::GetDefaultTheme().SubTitleFont)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SAssignNew(ShowStationsButton, SFlareButton)
					.Text(LOCTEXT("ShowStations", "Stations"))
					.HelpText(LOCTEXT("ShowStationsInfo", "Show stations in the list"))
					.OnClicked(this, &SFlareList::OnToggleShowFlags)
					.Visibility(this, &SFlareList::GetShipFiltersVisibility)
					.Small(true)
					.Transparent(true)
					.Toggle(true)
					.Width(2)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SAssignNew(ShowMilitaryButton, SFlareButton)
					.Text(LOCTEXT("ShowMilitary", "Military"))
					.HelpText(LOCTEXT("ShowMilitaryInfo", "Show military ships in the list"))
					.OnClicked(this, &SFlareList::OnToggleShowFlags)
					.Visibility(this, &SFlareList::GetShipFiltersVisibility)
					.Small(true)
					.Transparent(true)
					.Toggle(true)
					.Width(2)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SAssignNew(ShowFreightersButton, SFlareButton)
					.Text(LOCTEXT("ShowFreighters", "Freighters"))
					.HelpText(LOCTEXT("ShowFreightersInfo", "Show freighters in the list"))
					.OnClicked(this, &SFlareList::OnToggleShowFlags)
					.Visibility(this, &SFlareList::GetShipFiltersVisibility)
					.Small(true)
					.Transparent(true)
					.Toggle(true)
					.Width(2)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SAssignNew(GroupFleetsButton, SFlareButton)
					.Text(LOCTEXT("GroupFleets", "Fleets"))
					.HelpText(LOCTEXT("GroupFleetsInfo", "Group vessels by fleet"))
					.OnClicked(this, &SFlareList::OnToggleShowFlags)
					.Visibility(this, &SFlareList::GetFleetFilterVisibility)
					.Small(true)
					.Transparent(true)
					.Toggle(true)
					.Width(2)

				]
			]
	
			// Box
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Fill)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(UFlareGameTools::AddLeadingSpace(LOCTEXT("NothingSpaced", "No objects.\n\n"), 3))
					.TextStyle(&FFlareStyleSet::GetDefaultTheme().TextFont)
					.Visibility(this, &SFlareList::GetNoObjectsVisibility)
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SAssignNew(WidgetList, SListView< TSharedPtr<FInterfaceContainer> >)
					.ListItemsSource(&FilteredObjectList)
					.SelectionMode(ESelectionMode::Single)
					.OnGenerateRow(this, &SFlareList::GenerateTargetInfo)
					.OnSelectionChanged(this, &SFlareList::OnTargetSelected)
				]
			]
		]
	];

	// Set filters
	ShowStationsButton->SetActive(true);
	ShowMilitaryButton->SetActive(true);
	ShowFreightersButton->SetActive(true);
	GroupFleetsButton->SetActive(false);
}


/*----------------------------------------------------
	Interaction
----------------------------------------------------*/

void SFlareList::AddFleet(UFlareFleet* Fleet)
{
	HasFleets = true;
	ObjectList.AddUnique(FInterfaceContainer::New(Fleet));

	ShowStationsButton->SetText(LOCTEXT("ShowMilitary", "Military"));
	ShowStationsButton->SetHelpText(LOCTEXT("ShowMilitaryInfo", "Show fleets with military firepower in the list"));

	ShowMilitaryButton->SetText(LOCTEXT("ShowCargo", "Cargo"));
	ShowMilitaryButton->SetHelpText(LOCTEXT("ShowCargoInfo", "Show fleets with cargo in the list"));

	ShowFreightersButton->SetText(LOCTEXT("ShowAutoTrader", "Autotrader"));
	ShowFreightersButton->SetHelpText(LOCTEXT("ShowFreightersInfo", "Show fleets that are set to auto trader in the list"));

	GroupFleetsButton->SetText(LOCTEXT("TogleSector", "Sector"));
	GroupFleetsButton->SetHelpText(LOCTEXT("ToggleSectorInfo", "Show fleets in the same sector as selected fleet in this list"));
}

void SFlareList::AddShip(UFlareSimulatedSpacecraft* Ship)
{
	ShowStationsButton->SetText(LOCTEXT("ShowStations", "Stations"));
	ShowStationsButton->SetHelpText(LOCTEXT("ShowStationsInfo", "Show stations in the list"));

	ShowMilitaryButton->SetText(LOCTEXT("ShowMilitary", "Military"));
	ShowMilitaryButton->SetHelpText(LOCTEXT("ShowMilitaryInfo", "Show military ships in the list"));

	ShowFreightersButton->SetText(LOCTEXT("ShowFreighters", "Freighters"));
	ShowFreightersButton->SetHelpText(LOCTEXT("ShowFreightersInfo", "Show freighters in the list"));

	GroupFleetsButton->SetText(LOCTEXT("GroupFleets", "Fleets"));
	GroupFleetsButton->SetHelpText(LOCTEXT("GroupFleetsInfo", "Group vessels by fleet"));

	HasShips = true;
	ObjectList.AddUnique(FInterfaceContainer::New(Ship));
}

void SFlareList::RefreshList()
{
	struct FSortBySize
	{
		FORCEINLINE bool operator()(const TSharedPtr<FInterfaceContainer> PtrA, const TSharedPtr<FInterfaceContainer> PtrB) const
		{
			FCHECK(PtrA.IsValid());
			FCHECK(PtrB.IsValid());

			// Fleets
			if (PtrA->FleetPtr)
			{
				if (PtrB->FleetPtr)
				{
					UFlareFleet* PlayerFleet = PtrA->FleetPtr->GetGame()->GetPC()->GetPlayerFleet();

					if (PtrA->FleetPtr == PlayerFleet)
					{
						return true;
					}
					else if (PtrB->FleetPtr == PlayerFleet)
					{
						return false;
					}
					else
					{
						int32 ValueA = PtrA->FleetPtr->GetCombatPoints(true);
						int32 ValueB = PtrB->FleetPtr->GetCombatPoints(true);

						if (ValueA != ValueB)
						{
							return ValueA > ValueB;
						}
						else
						{
							return PtrA->FleetPtr->GetFleetName().ToString().Compare(PtrB->FleetPtr->GetFleetName().ToString()) < 0;
						}
					}
				}
				else
				{
					return true;
				}
			}
			else if (PtrB->FleetPtr)
			{
				return false;
			}

			// Stations
			else
			{
				UFlareSimulatedSpacecraft* A = PtrA->SpacecraftPtr;
				UFlareSimulatedSpacecraft* B = PtrB->SpacecraftPtr;

				if (A->IsPlayerShip() != B->IsPlayerShip())
				{
					return A->IsPlayerShip();
				}
				else if (A->IsStation() && B->IsStation())
				{
					if (A->GetDescription()->IsSubstation && !B->GetDescription()->IsSubstation)
					{
						return true;
					}
					else if (!A->GetDescription()->IsSubstation && B->GetDescription()->IsSubstation)
					{
						return false;
					}
					else if (A->GetDescription()->GetCapacity() != B->GetDescription()->GetCapacity())
					{
						return A->GetDescription()->GetCapacity() > B->GetDescription()->GetCapacity();
					}
					else
					{
						return A->GetDescription()->Mass > B->GetDescription()->Mass;
					}
				}
				else if (A->IsStation() && !B->IsStation())
				{
					return true;
				}
				else if (!A->IsStation() && B->IsStation())
				{
					return false;
				}

				// Ships
				else if (A->GetSize() > B->GetSize())
				{
					return true;
				}
				else if (A->GetSize() < B->GetSize())
				{
					return false;
				}
				else if (A->IsMilitary())
				{
					if (!B->IsMilitary())
					{
						return true;
					}
					else
					{
						return A->GetWeaponsSystem()->GetWeaponGroupCount() > B->GetWeaponsSystem()->GetWeaponGroupCount();
					}
				}
				else
				{
					return false;
				}
			}
			return false;
		}
	};


	// Apply filters
	FilteredObjectList.Empty();
	TArray<UFlareFleet*> FilteredFleets;
	for (auto Object : ObjectList)
	{
		// Ships have three filters
		if (Object->SpacecraftPtr)
		{
			bool IsStation = Object->SpacecraftPtr->IsStation();
			bool IsMilitary = Object->SpacecraftPtr->IsMilitary();

			if ((IsStation && ShowStationsButton->IsActive())
			 || (IsMilitary && ShowMilitaryButton->IsActive())
			 || (!IsStation && !IsMilitary && ShowFreightersButton->IsActive()))
			{
				UFlareFleet* ObjectFleet = Object->SpacecraftPtr->GetCurrentFleet();

				// Create a new fleet pointer if we're grouping by fleets
				if (GroupFleetsButton->IsActive() && !IsStation)
				{
					if (FilteredFleets.Find(ObjectFleet) == INDEX_NONE)
					{
						FilteredFleets.AddUnique(ObjectFleet);
						FilteredObjectList.AddUnique(FInterfaceContainer::New(ObjectFleet));
					}
				}
				else
				{
					FilteredObjectList.Add(Object);
				}
			}
		}

		// Fleets have no filters
		else if(Object->FleetPtr)
		{
			UFlareFleet* ObjectFleet = Object->FleetPtr;
			UFlareFleet* FleetToEdit = nullptr;

			if (ObjectFleet && ObjectFleet->GetShips().Num())
			{
//					|| (FleetToEdit&&FilterSector && FleetToEdit->GetCurrentSector() == Fleet->GetCurrentSector())

//
				if (MenuManager->GetCurrentMenu() == EFlareMenu::MENU_Fleet)
				{
					FleetToEdit = MenuManager->GetFleetMenu()->GetFleetEditing();
//						TSharedPtr<SFlareFleetMenu> GetFleetMenu()
//						GetFleetEditing
				}

				//todo: lot of redundancy here, gotta clean this trash up!
				if (GroupFleetsButton->IsActive() && FleetToEdit)
				{
					if(FleetToEdit->GetCurrentSector() == ObjectFleet->GetCurrentSector())
					{
						if ((ObjectFleet->GetCombatPoints(false) > 0 && ShowStationsButton->IsActive())
							|| (ObjectFleet->IsAutoTrading() && ShowFreightersButton->IsActive()))
						{
							FilteredObjectList.Add(Object);
						}
						else if (ShowMilitaryButton->IsActive())
						{
							//todo: the fleet itself should know its cargo capacity
							bool FoundFreighter = false;
							for (UFlareSimulatedSpacecraft* Ship : ObjectFleet->GetShips())
							{
								if (Ship->GetActiveCargoBay()->GetCapacity() > 0)
								{
									FoundFreighter = true;
									break;
								}
							}
							if (FoundFreighter)
							{
								FilteredObjectList.Add(Object);
							}
						}
					}
				}

				else if ((ObjectFleet->GetCombatPoints(false) > 0 && ShowStationsButton->IsActive())
					|| (ObjectFleet->IsAutoTrading() && ShowFreightersButton->IsActive()))
				{
					FilteredObjectList.Add(Object);
				}
				else if (ShowMilitaryButton->IsActive())
				{
					//todo: the fleet itself should know its cargo capacity
					bool FoundFreighter = false;
					for (UFlareSimulatedSpacecraft* Ship : ObjectFleet->GetShips())
					{
						if (Ship->GetActiveCargoBay()->GetCapacity() > 0)
						{
							FoundFreighter = true;
							break;
						}
					}
					if (FoundFreighter)
					{
						FilteredObjectList.Add(Object);
					}
				}
			}

/*
	bool FilterMilitary = 
	bool FilterCargo = ShowCargoButton->IsActive();
	bool FilterAutoTrade = ;
	bool FilterSector = SectorToggle->IsActive();

		for (int32 FleetIndex = 0; FleetIndex < FleetCount; FleetIndex++)
		{
			UFlareFleet* Fleet = MenuManager->GetPC()->GetCompany()->GetCompanyFleets()[FleetIndex];

			if (Fleet && Fleet->GetShips().Num() && FleetToEdit != Fleet)
			{
				if((Fleet->GetCombatPoints(false) > 0 && FilterMilitary)
				|| (FleetToEdit&&FilterSector && FleetToEdit->GetCurrentSector() == Fleet->GetCurrentSector())
				|| (Fleet->IsAutoTrading() && FilterAutoTrade))
				{
					List->AddFleet(Fleet);
				}

				//TODO make fleets aware of total cargo capacity of ships within as a simple var
				else if (FilterCargo)
				{
					bool FoundFreighter = false;
					for (UFlareSimulatedSpacecraft* Ship : Fleet->GetShips())
					{
						if (Ship->GetActiveCargoBay()->GetCapacity() > 0)
						{
							FoundFreighter = true;
							break;
						}
					}
					if (FoundFreighter)
					{
						List->AddFleet(Fleet);
					}
				}
*/


		}
	}

	// Sort and update
	FilteredObjectList.Sort(FSortBySize());
	WidgetList->RequestListRefresh();
	SlatePrepass(FSlateApplicationBase::Get().GetApplicationScale());

	ClearSelection();
}

void SFlareList::ClearSelection()
{
	WidgetList->ClearSelection();

	// De-select previous widget
	if (PreviousWidget.IsValid())
	{
		if (PreviousWidget->GetContainer()->GetContent()->GetTypeAsString() == "SFlareSpacecraftInfo")
		{
			StaticCastSharedRef<SFlareSpacecraftInfo>(PreviousWidget->GetContainer()->GetContent())->SetMinimized(true);
		}
		else if (PreviousWidget->GetContainer()->GetContent()->GetTypeAsString() == "SFlareFleetInfo")
		{
			StaticCastSharedRef<SFlareFleetInfo>(PreviousWidget->GetContainer()->GetContent())->SetMinimized(true);
		}

		PreviousWidget->SetSelected(false);
	}
}

void SFlareList::SetTitle(FText NewTitle)
{
	Title->SetText(NewTitle);
}

void SFlareList::SetUseCompactDisplay(bool Status)
{
	UseCompactDisplay = Status;
}

void SFlareList::Reset()
{
	HasShips = false;
	HasFleets = false;

	ObjectList.Empty();
	FilteredObjectList.Empty();

	WidgetList->ClearSelection();
	WidgetList->RequestListRefresh();

	SelectedObject.Reset();
}


/*----------------------------------------------------
	Callbacks
----------------------------------------------------*/

EVisibility SFlareList::GetNoObjectsVisibility() const
{
	return (FilteredObjectList.Num() > 0 ? EVisibility::Collapsed : EVisibility::Visible);
}

EVisibility SFlareList::GetShipFiltersVisibility() const
{
	return ((HasShips||HasFleets) ? EVisibility::Visible : EVisibility::Hidden);
}

EVisibility SFlareList::GetFleetFiltersVisibility() const
{
	return (HasFleets ? EVisibility::Visible : EVisibility::Hidden);
}




EVisibility SFlareList::GetFleetFilterVisibility() const
{
	if (MenuManager->GetCurrentMenu() != EFlareMenu::MENU_Fleet)
	{
		return (HasShips ? EVisibility::Visible : EVisibility::Hidden);
	}
	else
	{
		UFlareFleet* FleetToEdit = MenuManager->GetFleetMenu()->GetFleetEditing();
		return ((HasFleets&&FleetToEdit) ? EVisibility::Visible : EVisibility::Hidden);
	}
}

TSharedRef<ITableRow> SFlareList::GenerateTargetInfo(TSharedPtr<FInterfaceContainer> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	TSharedPtr<SFlareListItem> ListItem;
	int32 Width = 15;
	int32 Height = 1;

	// Ship
	if (Item->SpacecraftPtr)
	{
		TSharedPtr<SFlareSpacecraftInfo> Temp;

		SAssignNew(ListItem, SFlareListItem, OwnerTable)
		.Width(Width)
		.Height(Height)
		.Content()
		[
			SAssignNew(Temp, SFlareSpacecraftInfo)
			.Player(MenuManager->GetPC())
			.OwnerWidget(this)
			.Minimized(true)
			.OnRemoved(this, &SFlareList::OnShipRemoved)
		];

		Temp->SetSpacecraft(Item->SpacecraftPtr);
		Temp->Show();
	}

	// Fleet
	else if (Item->FleetPtr)
	{
		TSharedPtr<SFlareFleetInfo> Temp;

		SAssignNew(ListItem, SFlareListItem, OwnerTable)
		.Width(Width)
		.Height(Height)
		.Content()
		[
			SAssignNew(Temp, SFlareFleetInfo)
			.Player(MenuManager->GetPC())
			.OwnerWidget(this)
			.Minimized(true)
		];

		Temp->SetFleet(Item->FleetPtr);
		Temp->Show();
	}

	// Invalid item
	else
	{
		SAssignNew(ListItem, SFlareListItem, OwnerTable)
		.Content()
		[
			SNew(STextBlock).Text(LOCTEXT("Invalid", "Invalid item"))
		];
	}

	return ListItem.ToSharedRef();
}

void SFlareList::OnTargetSelected(TSharedPtr<FInterfaceContainer> Item, ESelectInfo::Type SelectInfo)
{
	FLOG("SFlareList::OnTargetSelected");

	SelectedObject = Item;
	TSharedPtr<SFlareListItem> NewWidget = StaticCastSharedPtr<SFlareListItem>(WidgetList->WidgetFromItem(Item));
	bool UseExpandedDisplay = Item.IsValid() && (Item->SpacecraftPtr || Item->FleetPtr) && !UseCompactDisplay;

	// De-select previous item
	if (PreviousWidget.IsValid())
	{
		if (UseExpandedDisplay)
		{
			if (PreviousWidget->GetContainer()->GetContent()->GetTypeAsString() == "SFlareSpacecraftInfo")
			{
				StaticCastSharedRef<SFlareSpacecraftInfo>(PreviousWidget->GetContainer()->GetContent())->SetMinimized(true);
			}
			else if (PreviousWidget->GetContainer()->GetContent()->GetTypeAsString() == "SFlareFleetInfo")
			{
				StaticCastSharedRef<SFlareFleetInfo>(PreviousWidget->GetContainer()->GetContent())->SetMinimized(true);
			}
		}

		PreviousWidget->SetSelected(false);
	}

	// Select new item
	if (NewWidget.IsValid())
	{
		if (OnItemSelected.IsBound())
		{
			OnItemSelected.Execute(Item); 
		}

		if (UseExpandedDisplay)
		{
			if (NewWidget->GetContainer()->GetContent()->GetTypeAsString() == "SFlareSpacecraftInfo")
			{
				StaticCastSharedRef<SFlareSpacecraftInfo>(NewWidget->GetContainer()->GetContent())->SetMinimized(false);
			}
			else if (NewWidget->GetContainer()->GetContent()->GetTypeAsString() == "SFlareFleetInfo")
			{
				StaticCastSharedRef<SFlareFleetInfo>(NewWidget->GetContainer()->GetContent())->SetMinimized(false);
			}
		}

		NewWidget->SetSelected(true);
		PreviousWidget = NewWidget;
	}
}

void SFlareList::OnToggleShowFlags()
{
	RefreshList();
}

void SFlareList::OnShipRemoved(UFlareSimulatedSpacecraft* Ship)
{
	for (auto Spacecraft : ObjectList)
	{
		if (Spacecraft->SpacecraftPtr == Ship)
		{
			ObjectList.Remove(Spacecraft);
			break;
		}
	}

	PreviousWidget.Reset();
	RefreshList();
}

#undef LOCTEXT_NAMESPACE

