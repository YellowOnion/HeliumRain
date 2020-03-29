
#include "FlareSpacecraftOrderOverlay.h"
#include "../Components/FlareSpacecraftOrderOverlayInfo.h"
#include "../../Flare.h"

#include "FlareFactoryInfo.h"
#include "../Menus/FlareShipMenu.h"

#include "../../Player/FlareMenuManager.h"
#include "../../Player/FlarePlayerController.h"
#include "../../Economy/FlareFactory.h"
#include "../../Game/FlareGame.h"
#include "../../Game/FlareGameTools.h"
#include "../../Game/FlareSkirmishManager.h"

#include "SBackgroundBlur.h"

#define LOCTEXT_NAMESPACE "FlareSpacecraftOrderOverlay"


/*----------------------------------------------------
	Construct
----------------------------------------------------*/

void SFlareSpacecraftOrderOverlay::Construct(const FArguments& InArgs)
{
	// Data
	SpacecraftList.Empty();
	MenuManager = InArgs._MenuManager;
	const FFlareStyleCatalog& Theme = FFlareStyleSet::GetDefaultTheme();
	TargetComplex = NULL;
	TargetShipyard = NULL;
	TargetSector = NULL;
	TargetSkirmish = NULL;

	// Create the layout
	ChildSlot
	.VAlign(VAlign_Center)
	.HAlign(HAlign_Fill)
	[
		SNew(SBox)
		[
			SNew(SBackgroundBlur)
			.BlurRadius(Theme.BlurRadius)
			.BlurStrength(Theme.BlurStrength)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.Padding(FMargin(0))
			[
				SNew(SBorder)
				.HAlign(HAlign_Center)
				.Padding(Theme.ContentPadding)
				.BorderImage(&Theme.BackgroundBrush)
				[
					SNew(SVerticalBox)

					// Title
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(Theme.ContentPadding)
					[
						SNew(STextBlock)
						.Text(this, &SFlareSpacecraftOrderOverlay::GetWindowTitle)
						.TextStyle(&Theme.TitleFont)
						.Justification(ETextJustify::Center)
					]
	
					// List
					+ SVerticalBox::Slot()
					[
						SNew(SScrollBox)
						.Style(&Theme.ScrollBoxStyle)
						.ScrollBarStyle(&Theme.ScrollBarStyle)

						+ SScrollBox::Slot()
						.Padding(Theme.ContentPadding)
						[
							SAssignNew(SpacecraftSelector, SListView<TSharedPtr<FInterfaceContainer>>)
							.ListItemsSource(&SpacecraftList)
							.SelectionMode(ESelectionMode::Single)
							.OnGenerateRow(this, &SFlareSpacecraftOrderOverlay::OnGenerateSpacecraftLine)
							.OnSelectionChanged(this, &SFlareSpacecraftOrderOverlay::OnSpacecraftSelectionChanged)
						]
					]

					// Help text
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(Theme.ContentPadding)
					.HAlign(HAlign_Right)
					[
						SNew(STextBlock)
						.TextStyle(&Theme.TextFont)
						.Visibility(!OrderIsConfig ? EVisibility::Visible : EVisibility::Collapsed)
						.Text(this, &SFlareSpacecraftOrderOverlay::GetWalletText)
					]
	
					// Buttons
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(Theme.ContentPadding)
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Top)
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(Theme.ContentPadding)
						[
							SAssignNew(ConfirmText, STextBlock)
							.TextStyle(&Theme.TextFont)
							.WrapTextAt(Theme.ContentWidth)
						]

						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Top)
						[
							SAssignNew(ConfirmButon, SFlareButton)
							.Text(LOCTEXT("Confirm", "Confirm"))
							.HelpText(LOCTEXT("ConfirmInfo", "Confirm the choice and start production"))
//							.Text(GetConfirmText())
//							.HelpText(GetConfirmHelpText())
							.Icon(FFlareStyleSet::GetIcon("OK"))
							.OnClicked(this, &SFlareSpacecraftOrderOverlay::OnConfirmed)
							.Visibility(this, &SFlareSpacecraftOrderOverlay::GetConfirmVisibility)
						]

						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Top)
						[
							SNew(SFlareButton)
							.Text(LOCTEXT("Cancel", "Cancel"))
							.HelpText(LOCTEXT("CancelInfo", "Go back without saving changes"))
							.Icon(FFlareStyleSet::GetIcon("Delete"))
							.OnClicked(this, &SFlareSpacecraftOrderOverlay::OnClose)
						]
					]
				]
			]
		]
	];

	SetVisibility(EVisibility::Collapsed);
}


/*----------------------------------------------------
	Interaction
----------------------------------------------------*/

static bool SortByCombatValue(const TSharedPtr<FInterfaceContainer>& Ship1, const TSharedPtr<FInterfaceContainer>& Ship2)
{
	FCHECK(Ship1->SpacecraftDescriptionPtr);
	FCHECK(Ship2->SpacecraftDescriptionPtr);

	if (Ship1->SpacecraftDescriptionPtr->CombatPoints < Ship2->SpacecraftDescriptionPtr->CombatPoints)
	{
		return true;
	}
	else
	{
		return false;
	}
}

void SFlareSpacecraftOrderOverlay::Open(UFlareSimulatedSpacecraft* Complex, FName ConnectorName, FOrderDelegate ConfirmationCallback)
{
	SetVisibility(EVisibility::Visible);
	TargetSector = Complex->GetCurrentSector();
	TargetComplex = Complex;
	OnConfirmedCB = ConfirmationCallback;
	OrderIsConfig = false;
	IsComplexSlotSpecial = (TargetComplex != NULL) && UFlareSimulatedSpacecraft::IsSpecialComplexSlot(ConnectorName);

	// Init station list
	SpacecraftList.Empty();
	if (TargetSector)
	{
		UFlareSpacecraftCatalog* SpacecraftCatalog = MenuManager->GetGame()->GetSpacecraftCatalog();

		for (int SpacecraftIndex = 0; SpacecraftIndex < SpacecraftCatalog->StationCatalog.Num(); SpacecraftIndex++)
		{
			// Candidate have to be not a substation and available in tech
			FFlareSpacecraftDescription* Description = &SpacecraftCatalog->StationCatalog[SpacecraftIndex]->Data;
			if (!Description->IsSubstation && MenuManager->GetPC()->GetCompany()->IsTechnologyUnlockedStation(Description))
			{
				UFlareSpacecraftCatalogEntry* Entry = SpacecraftCatalog->StationCatalog[SpacecraftIndex];
				SpacecraftList.AddUnique(FInterfaceContainer::New(&Entry->Data));
			}
		}
	}

	SpacecraftSelector->RequestListRefresh();
	ConfirmText->SetText(FText());
}

void SFlareSpacecraftOrderOverlay::Open(UFlareSimulatedSpacecraft* Shipyard, bool IsHeavy, bool IsConfig)
{
	SetVisibility(EVisibility::Visible);
	TargetShipyard = Shipyard;
	OrderIsConfig = IsConfig;

	// Init ship list
	SpacecraftList.Empty();
	if (TargetShipyard && TargetShipyard->IsShipyard())
	{
		SpaceCraftData = TargetShipyard->GetData();
		UFlareSpacecraftCatalogEntry* SelectedEntry = NULL;
		UFlareSpacecraftCatalog* SpacecraftCatalog = MenuManager->GetGame()->GetSpacecraftCatalog();

		for (int SpacecraftIndex = 0; SpacecraftIndex < SpacecraftCatalog->ShipCatalog.Num(); SpacecraftIndex++)
		{
			UFlareSpacecraftCatalogEntry* Entry = SpacecraftCatalog->ShipCatalog[SpacecraftIndex];
			FFlareSpacecraftDescription* Description = &Entry->Data;

			if (!Description->IsSubstation)
			{
				// Filter by spacecraft size and add
				if(IsConfig)
				{
					SpacecraftList.AddUnique(FInterfaceContainer::New(&Entry->Data));
				}
				else
				{
					bool LargeSpacecraft = Description->Size >= EFlarePartSize::L;
					if (IsHeavy == LargeSpacecraft)
					{
						SpacecraftList.AddUnique(FInterfaceContainer::New(&Entry->Data));
					}
				}
			}
		}
	}
	SpacecraftSelector->RequestListRefresh();
	ConfirmText->SetText(FText());
}

void SFlareSpacecraftOrderOverlay::Open(UFlareSimulatedSector* Sector, FOrderDelegate ConfirmationCallback)
{
	SetVisibility(EVisibility::Visible);
	TargetSector = Sector;
	OnConfirmedCB = ConfirmationCallback;
	OrderIsConfig = false;

	// Init station list
	SpacecraftList.Empty();
	if (TargetSector)
	{
		UFlareSpacecraftCatalog* SpacecraftCatalog = MenuManager->GetGame()->GetSpacecraftCatalog();

		for (int SpacecraftIndex = 0; SpacecraftIndex < SpacecraftCatalog->StationCatalog.Num(); SpacecraftIndex++)
		{
			FFlareSpacecraftDescription* Description = &SpacecraftCatalog->StationCatalog[SpacecraftIndex]->Data;
			if (!Description->IsSubstation && MenuManager->GetPC()->GetCompany()->IsTechnologyUnlockedStation(Description))
			{
				UFlareSpacecraftCatalogEntry* Entry = SpacecraftCatalog->StationCatalog[SpacecraftIndex];
				SpacecraftList.AddUnique(FInterfaceContainer::New(&Entry->Data));
			}
		}
	}

	SpacecraftSelector->RequestListRefresh();
	ConfirmText->SetText(FText());
}

void SFlareSpacecraftOrderOverlay::Open(UFlareSkirmishManager* Skirmish, bool ForPlayer, FOrderDelegate ConfirmationCallback)
{
	SetVisibility(EVisibility::Visible);
	TargetSkirmish = Skirmish;
	OnConfirmedCB = ConfirmationCallback;
	OrderForPlayer = ForPlayer;
	OrderIsConfig = false;

	// Init ship list
	SpacecraftList.Empty();
	if (TargetSkirmish)
	{
		UFlareSpacecraftCatalog* SpacecraftCatalog = MenuManager->GetGame()->GetSpacecraftCatalog();

		for (int SpacecraftIndex = 0; SpacecraftIndex < SpacecraftCatalog->ShipCatalog.Num(); SpacecraftIndex++)
		{
			FFlareSpacecraftDescription* Description = &SpacecraftCatalog->ShipCatalog[SpacecraftIndex]->Data;
			if (!Description->IsSubstation && Description->IsMilitary())
			{
				UFlareSpacecraftCatalogEntry* Entry = SpacecraftCatalog->ShipCatalog[SpacecraftIndex];
				SpacecraftList.AddUnique(FInterfaceContainer::New(&Entry->Data));
			}
		}
	}

	SpacecraftList.Sort(SortByCombatValue);

	SpacecraftSelector->RequestListRefresh();
	ConfirmText->SetText(FText());
}

bool SFlareSpacecraftOrderOverlay::IsOpen() const
{
	return (GetVisibility() == EVisibility::Visible);
}

void SFlareSpacecraftOrderOverlay::Close()
{
	SetVisibility(EVisibility::Collapsed);

	SpacecraftList.Empty();
	SpacecraftSelector->RequestListRefresh();
	SpacecraftSelector->ClearSelection();

	ConfirmText->SetText(FText());
	TargetComplex = NULL;
	TargetShipyard = NULL;
	TargetSector = NULL;
	TargetSkirmish = NULL;
	IsComplexSlotSpecial = false;
	OrderIsConfig = false;
}

void SFlareSpacecraftOrderOverlay::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (SpacecraftSelector->GetNumItemsSelected() > 0)
	{
		FFlareSpacecraftDescription* Desc = SpacecraftSelector->GetSelectedItems()[0]->SpacecraftDescriptionPtr;
		UFlareCompany* PlayerCompany = MenuManager->GetPC()->GetCompany();
		ConfirmText->SetText(FText());
		ConfirmButon->SetText(LOCTEXT("Confirm", "Confirm"));

		bool CanBuild = false;

		if (Desc)
		{
			// Shipyard mode
			if (TargetShipyard)
			{
				if (OrderIsConfig)
				{
					CanBuild = true;
					SpaceCraftData = TargetShipyard->GetData();
					if (SpaceCraftData.ShipyardOrderExternalConfig.Find(Desc->Identifier) != INDEX_NONE)
					{
						ConfirmText->SetText(FText::Format(LOCTEXT("ConfigEnabledInfo", "Disable remote company access to {0}?"),
							Desc->Name));
						ConfirmButon->SetText(LOCTEXT("ConfigEnabled", "Disable"));
					}
					else
					{
						ConfirmText->SetText(FText::Format(LOCTEXT("ConfigDisabledInfo", "Enable remote company access to {0}?"),
						Desc->Name));
						ConfirmButon->SetText(LOCTEXT("ConfigDisabled", "Enable"));
					}
				}
				else
				{
				uint32 ShipPrice;

				// Get price
				if (TargetShipyard->GetCompany() != PlayerCompany)
				{
					ShipPrice = UFlareGameTools::ComputeSpacecraftPrice(Desc->Identifier, TargetShipyard->GetCurrentSector(), true);
				}
				else
				{
					ShipPrice = Desc->CycleCost.ProductionCost;
				}
				CanBuild = (PlayerCompany->GetMoney() >= ShipPrice);

				// Show reason
				if (!CanBuild)
				{
					ConfirmText->SetText(FText::Format(LOCTEXT("CannotBuildShip", "Not enough credits ({0} / {1})"),
						FText::AsNumber(UFlareGameTools::DisplayMoney(PlayerCompany->GetMoney())),
						FText::AsNumber(UFlareGameTools::DisplayMoney(ShipPrice))));
				}
			}
			}

			// Skirmish mode
			else if (TargetSkirmish)
			{
				// No rules
				CanBuild = true;
			}

			// Sector mode (stations, complex stations)
			else
			{
				TArray<FText> Reasons;
				CanBuild = TargetSector->CanBuildStation(Desc, PlayerCompany, Reasons, false, (TargetComplex != NULL), IsComplexSlotSpecial);

				// Show reason
				if (!CanBuild)
				{
					FString CantBuildReasons;
					for (int32 Index = 0; Index < Reasons.Num(); Index++)
					{
						if (Index)
						{
							CantBuildReasons += FString("\n");
						}
						CantBuildReasons += Reasons[Index].ToString();
					}
					ConfirmText->SetText(FText::FromString(CantBuildReasons));
				}
			}
		}
		ConfirmButon->SetDisabled(!CanBuild);
	}
}


/*----------------------------------------------------
	Content callbacks
----------------------------------------------------*/

FText SFlareSpacecraftOrderOverlay::GetWindowTitle() const
{
	if (TargetComplex)
	{
		return LOCTEXT("BuildComplexStationTitle", "Add complex element");
	}
	else if (TargetShipyard)
	{
		if (OrderIsConfig)
		{
			return LOCTEXT("SpacecraftConfigTitle", "Configure external orders");
		}
		else
		{
			return LOCTEXT("SpacecraftOrderTitle", "Order spacecraft");
		}
	}

	else if (TargetSkirmish)
	{
		return LOCTEXT("AddSkirmishTitle", "Add spacecraft");
	}
	else if (TargetSector)
	{
		return LOCTEXT("BuildStationTitle", "Build station");
	}
	else
	{
		return FText();
	}
}

FText SFlareSpacecraftOrderOverlay::GetWalletText() const
{
	if (TargetSkirmish)
	{
		// No rules
		return FText();
	}
	else if (MenuManager->GetPC())
	{
		FCHECK(MenuManager->GetPC()->GetCompany());

		return FText::Format(LOCTEXT("CompanyCurrentWallet", "You have {0} credits available."),
			FText::AsNumber(MenuManager->GetPC()->GetCompany()->GetMoney() / 100));
	}

	return FText();
}

TSharedRef<ITableRow> SFlareSpacecraftOrderOverlay::OnGenerateSpacecraftLine(TSharedPtr<FInterfaceContainer> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	// Setup
	const FFlareStyleCatalog& Theme = FFlareStyleSet::GetDefaultTheme();
	FFlareSpacecraftDescription* Desc = Item->SpacecraftDescriptionPtr;

	uint32 Width = 0;
	if (OrderIsConfig)
	{
		Width = 24;
	}
	else
	{
		Width = 32;
	}
	// Structure

	return SNew(SFlareListItem, OwnerTable)
	.Width(Width)
	.Height(2)
	.Content()
	[
		SNew(SFlareSpaceCraftOverlayInfo)
		.Desc(Desc)
		.TargetShipyard(TargetShipyard)
		.TargetSkirmish(TargetSkirmish)
		.OrderIsConfig(OrderIsConfig)
		.TargetSector(TargetSector)
		.MenuManager(MenuManager)
	];

	/*
	return SNew(SFlareListItem, OwnerTable)
	.Width(Width)
	.Height(2)
	.Content()
	[
		SNew(SHorizontalBox)

		// Picture
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(Theme.ContentPadding)
		.VAlign(VAlign_Top)
		[
			SNew(SImage)
			.Image(&Desc->MeshPreviewBrush)
		]

		// Main infos
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(Theme.ContentPadding)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(Theme.SmallContentPadding)
			[
				SNew(STextBlock)
				.Text(Desc->Name)
				.TextStyle(&Theme.NameFont)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(Theme.SmallContentPadding)
			[
				SNew(SBox)
				.WidthOverride(Theme.ContentWidth)
				[
					SNew(STextBlock)
					.Text(Desc->Description)
					.TextStyle(&Theme.TextFont)
					.WrapTextAt(Theme.ContentWidth)
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(Theme.SmallContentPadding)
			[
				SNew(STextBlock)
				.Text(SpacecraftInfoText)
				.TextStyle(&Theme.TextFont)
			]
		]

		// Costs
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(Theme.ContentPadding)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ProductionCost", "Production cost & duration"))
				.Visibility(!OrderIsConfig ? EVisibility::Visible : EVisibility::Collapsed)
				.TextStyle(&Theme.NameFont)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
//				SAssignNew(TextBlock, STextBlock)
				SNew(STextBlock)
				.Text(ProductionCost)
				.WrapTextAt(0.65 * Theme.ContentWidth)
				.TextStyle(&Theme.TextFont)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::Format(LOCTEXT("ProductionTimeFormat", "\u2022 {0} days"), FText::AsNumber(ProductionTime)))
				.Visibility((!TargetSkirmish && !OrderIsConfig && ProductionTime > 0) ? EVisibility::Visible : EVisibility::Collapsed)
				.WrapTextAt(0.65 * Theme.ContentWidth)
				.TextStyle(&Theme.TextFont)
			]
		]
	];
*/
}

void SFlareSpacecraftOrderOverlay::OnSpacecraftSelectionChanged(TSharedPtr<FInterfaceContainer> Item, ESelectInfo::Type SelectInfo)
{
	FLOG("SFlareSpacecraftOrderOverlay::OnSpacecraftSelectionChanged");
	TSharedPtr<SFlareListItem> ItemWidget = StaticCastSharedPtr<SFlareListItem>(SpacecraftSelector->WidgetFromItem(Item));
	SelectedItem = Item;

	// Update selection
	if (PreviousSelection.IsValid())
	{
		PreviousSelection->SetSelected(false);
	}

	if (ItemWidget.IsValid())
	{
		ItemWidget->SetSelected(true);
		PreviousSelection = ItemWidget;
	}
}

EVisibility SFlareSpacecraftOrderOverlay::GetConfirmVisibility() const
{
	if (SpacecraftSelector->GetNumItemsSelected() > 0)
	{
		return EVisibility::Visible;
	}
	return EVisibility::Hidden;
}

/*----------------------------------------------------
	Action callbacks
----------------------------------------------------*/

void SFlareSpacecraftOrderOverlay::OnConfirmed()
{
	// Apply
	if (SpacecraftSelector->GetNumItemsSelected() > 0)
	{
		FFlareSpacecraftDescription* Desc = SpacecraftSelector->GetSelectedItems()[0]->SpacecraftDescriptionPtr;
		if (Desc)
		{
			FLOGV("SFlareSpacecraftOrderOverlay::OnConfirmed : picked '%s'", *Desc->Identifier.ToString());

			// Factory
			if (TargetShipyard)
			{
				if (OrderIsConfig)
				{
					FName Identifier = Desc->Identifier;
					TargetShipyard->AddRemoveExternalOrderArray(Desc->Identifier);
				}
				else
				{
					TargetShipyard->ShipyardOrderShip(MenuManager->GetPC()->GetCompany(), Desc->Identifier);
				}

				if (MenuManager->GetCurrentMenu() == EFlareMenu::MENU_Station || MenuManager->GetCurrentMenu() == EFlareMenu::MENU_Ship)
				{
					MenuManager->GetShipMenu()->UpdateShipyard();
				}
			}

			// Sector, complex, skirmish...
			else
			{
				OnConfirmedCB.ExecuteIfBound(Desc);
			}
		}

		MenuManager->GetPC()->ClientPlaySound(MenuManager->GetPC()->GetSoundManager()->InfoSound);
	}

	if (!OrderIsConfig)
	{
		Close();
	}
}

void SFlareSpacecraftOrderOverlay::OnClose()
{
	MenuManager->GetPC()->ClientPlaySound(MenuManager->GetPC()->GetSoundManager()->NegativeClickSound);
	Close();
}


#undef LOCTEXT_NAMESPACE
