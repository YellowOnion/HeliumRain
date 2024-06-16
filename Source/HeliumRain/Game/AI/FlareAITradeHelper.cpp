#include "FlareAITradeHelper.h"

#include "../FlareGame.h"
#include "../FlareCompany.h"
#include "../FlareSectorHelper.h"
#include "../FlareScenarioTools.h"

#include "../../Data/FlareResourceCatalog.h"

#include "../../Economy/FlareFactory.h"
#include "../../Economy/FlareCargoBay.h"

#include "../../Player/FlarePlayerController.h"
#include "../../Player/FlareMenuManager.h"

#include "../../Spacecrafts/FlareSimulatedSpacecraft.h"
#include "../../Spacecrafts/Subsystems/FlareSimulatedSpacecraftDamageSystem.h"

#include "FlareAIBehavior.h"
#include <functional>


DECLARE_CYCLE_STAT(TEXT("AITradeHelper FindBestDealForShipFromSector"), STAT_AITradeHelper_FindBestDealForShipFromSector, STATGROUP_Flare);
DECLARE_CYCLE_STAT(TEXT("AITradeHelper FindBestDealForShip"), STAT_AITradeHelper_FindBestDealForShip, STATGROUP_Flare);
DECLARE_CYCLE_STAT(TEXT("AITradeHelper FindBestDealForShip Sectors"), STAT_AITradeHelper_FindBestDealForShip_Sectors, STATGROUP_Flare);
DECLARE_CYCLE_STAT(TEXT("AITradeHelper FindBestDealForShip Loop"), STAT_AITradeHelper_FindBestDealForShip_Loop, STATGROUP_Flare);
DECLARE_CYCLE_STAT(TEXT("AITradeHelper ApplyDeal"), STAT_AITradeHelper_ApplyDeal, STATGROUP_Flare);



#define DEBUG_AI_TRADING 0
#define DEBUG_AI_TRADING_FIND_DEAL 1
#define DEBUG_AI_TRADING_COMPANY "PLY"
#define DEBUG_AI_TRADING_SECTOR_FILTER 1
//#define DEBUG_AI_TRADING_SECTOR "miners-home"
#define DEBUG_AI_TRADING_SECTOR "first-light"
#define DEBUG_AI_TRADING_SECTOR_B_FILTER 1
#define DEBUG_AI_TRADING_SECTOR_B "first-light"
//#define DEBUG_AI_TRADING_SECTOR_B "miners-home"
//#define DEBUG_AI_TRADING_SECTOR_B "nights-home"
#define DEBUG_AI_TRADING_RESOURCES_FILTER 1
//#define DEBUG_AI_TRADING_RESOURCES "fleet-supply"
#define DEBUG_AI_TRADING_RESOURCES "Fuel"
//#define DEBUG_AI_TRADING_RESOURCES "Feo"
//#define DEBUG_AI_TRADING_RESOURCES "Food"

#define LOCTEXT_NAMESPACE "AITradeHelper"

void AITradeHelper::CompanyAutoTrade(UFlareCompany* Company)
{
	Company->GetAI()->GetBehavior()->Load(Company);
	TMap<UFlareSimulatedSector*, SectorVariation> WorldResourceVariation;
	WorldResourceVariation.Reserve(Company->GetVisitedSectors().Num());
	for (int32 SectorIndex = 0; SectorIndex < Company->GetVisitedSectors().Num(); SectorIndex++)
	{
		UFlareSimulatedSector* Sector = Company->GetVisitedSectors()[SectorIndex];
		SectorVariation Variation = AITradeHelper::ComputeSectorResourceVariation(Company, Sector, false);

		WorldResourceVariation.Add(Sector, Variation);
		//DumpSectorResourceVariation(Sector, &Variation);
	}

	for(UFlareFleet* Fleet : Company->GetCompanyFleets())
	{
		if(Fleet->IsAutoTrading())
		{
			Fleet->GetData()->AutoTradeStatsDays++;
			FleetAutoTrade(Fleet, WorldResourceVariation);
		}
	}
}

UFlareSimulatedSpacecraft* AITradeHelper::FindBestMasterShip(int32& UsableShipCount,UFlareFleet* Fleet, TArray<UFlareSimulatedSpacecraft*>& ExcludeList)
{
	UFlareSimulatedSpacecraft* MasterShip = nullptr;
	int32 MasterShipCapacity = 0;
	int32 MasterShipQuantity = 0;
	UsableShipCount = 0;

	for(UFlareSimulatedSpacecraft* Ship : Fleet->GetShips())
	{
		if(Ship->IsMilitary())
		{
			continue;
		}

		if(Ship->GetDamageSystem()->IsUncontrollable())
		{
			continue;
		}

		UsableShipCount++;

		if(ExcludeList.Contains(Ship))
		{
			continue;
		}

		int32 ShipQuantity = Ship->GetActiveCargoBay()->GetUsedCargoSpace();
		int32 ShipCapacity = Ship->GetActiveCargoBay()->GetUsedCargoSpace();

		auto SelectMasterShip = [&]()
		{
			MasterShip = Ship;
			MasterShipCapacity = ShipCapacity;
			MasterShipQuantity = ShipQuantity;
		};

		if(MasterShip == nullptr)
		{
			SelectMasterShip();
		}
		else if(ShipQuantity > MasterShipQuantity)
		{
			SelectMasterShip();
		}
		else if(ShipQuantity == MasterShipQuantity && ShipCapacity > MasterShipCapacity)
		{
			SelectMasterShip();
		}
	}
	return MasterShip;
}

void AITradeHelper::FleetAutoTrade(UFlareFleet* Fleet, TMap<UFlareSimulatedSector*, SectorVariation>& WorldResourceVariation)
{
	if(Fleet->IsTrading() || Fleet->IsTraveling())
	{
		// Wait next day
		return;
	}

	bool IsFleetStranded = !Fleet->CanTravel();
	bool CheckedRepairing = false;
	bool IsRepairing = false;
	if (IsFleetStranded)
	{
		if (!CheckedRepairing)
		{
			CheckedRepairing = true;
			IsRepairing = Fleet->IsRepairing();
		}

		if (!IsRepairing)
		{
			AFlarePlayerController* PC = Fleet->GetGame()->GetPC();
			FFlareMenuParameterData Data;
			Data.Fleet = Fleet;
			PC->Notify(LOCTEXT("AutoTradeStranded", "Stranded fleet"),
			FText::Format(LOCTEXT("StrandedAutoTradeFormat", "Your fleet {0} is stranded and has limited trading capabilities."), Fleet->GetFleetName()),
			FName("auto-trade-stranded"),
			EFlareNotification::NT_Economy,
			false,
			EFlareMenu::MENU_Fleet,
			Data);
		}
	}

	TArray<UFlareSimulatedSpacecraft*> ExcludeList;
	UFlareSimulatedSpacecraft* MasterShip = nullptr;
	SectorDeal MasterShipBestDeal;

	do
	{
		int32 UsableShipCount;
		MasterShip = FindBestMasterShip(UsableShipCount, Fleet, ExcludeList);

		if(UsableShipCount == 0)
		{
			if (!CheckedRepairing)
			{
				CheckedRepairing = true;
				IsRepairing = Fleet->IsRepairing();
			}

			if (!IsRepairing)
			{
				AFlarePlayerController* PC = Fleet->GetGame()->GetPC();
				FFlareMenuParameterData Data;
				Data.Fleet = Fleet;
				PC->Notify(LOCTEXT("NoAutoTradeCapability", "No auto-trading capability"),
					FText::Format(LOCTEXT("NoAutoTradeCapabilityFormat", "Your fleet {0} doesn't have any trading ship."), Fleet->GetFleetName()),
					FName("no-auto-trade-capability"),
					EFlareNotification::NT_Economy,
					false,
					EFlareMenu::MENU_Fleet,
					Data);
			}
			return;
		}
		
		if(MasterShip == nullptr)
		{
			// No deal possible
			return;
		}


		if(IsFleetStranded)
		{
			// Search local deal only
			MasterShipBestDeal = FindBestDealForShip(MasterShip, WorldResourceVariation, Fleet->GetCurrentSector(), Fleet->GetCurrentSector());
		}
		else
		{
			MasterShipBestDeal = FindBestDealForShip(MasterShip, WorldResourceVariation, nullptr, nullptr);
		}
		ExcludeList.Add(MasterShip);
	}
	while(MasterShipBestDeal.Resource == nullptr);


	ApplyDeal(MasterShip, MasterShipBestDeal, &WorldResourceVariation, true, false);

	if(Fleet->IsTraveling())
	{
		return;
	}

	// Find deal for others ships
	for(UFlareSimulatedSpacecraft* Ship : Fleet->GetShips())
	{
		if(Ship == MasterShip)
		{
			continue;
		}

		if(Ship->IsMilitary())
		{
			continue;
		}

		if(Ship->GetDamageSystem()->IsUncontrollable())
		{
			continue;
		}

		SectorDeal BestDeal = FindBestDealForShip(Ship, WorldResourceVariation, MasterShipBestDeal.SectorA, MasterShipBestDeal.SectorB);

		if(BestDeal.Resource != nullptr)
		{
			ApplyDeal(Ship, BestDeal, &WorldResourceVariation, false,  false);
		}
	}

}

TArray<UFlareSimulatedSpacecraft*> AITradeHelper::FindIdleCargos(UFlareCompany* Company)
{
	TArray<UFlareSimulatedSpacecraft*> IdleCargos;

	for (int32 SectorIndex = 0; SectorIndex < Company->GetKnownSectors().Num(); SectorIndex++)
	{
		UFlareSimulatedSector* Sector = Company->GetKnownSectors()[SectorIndex];

		for (int32 ShipIndex = 0 ; ShipIndex < Sector->GetSectorShips().Num(); ShipIndex++)
		{
			UFlareSimulatedSpacecraft* Ship = Sector->GetSectorShips()[ShipIndex];
			if (Ship->GetCompany() != Company || Ship->GetDamageSystem()->IsStranded() || Ship->IsTrading() || (Ship->GetCurrentFleet() && Ship->GetCurrentFleet()->IsTraveling()) || Ship->GetCurrentTradeRoute() != NULL || Ship->GetActiveCargoBay()->GetCapacity() == 0)
			{
				continue;
			}

			IdleCargos.Add(Ship);
		}
	}

	return IdleCargos;
}

SectorDeal AITradeHelper::FindBestDealForShipFromSector(UFlareSimulatedSpacecraft* Ship, UFlareSimulatedSector* SectorA, SectorDeal* DealToBeat, TMap<UFlareSimulatedSector*, SectorVariation> const& WorldResourceVariation, UFlareSimulatedSector* SectorBRestiction)
{
	SCOPE_CYCLE_COUNTER(STAT_AITradeHelper_FindBestDealForShipFromSector);

	UFlareCompany* Company = Ship->GetCompany();
	AFlareGame* Game = Ship->GetGame();
	UFlareAIBehavior* Behavior = Company->GetAI()->GetBehavior();


	SectorDeal BestDeal;
	BestDeal.Resource = NULL;
	BestDeal.BuyQuantity = 0;
	BestDeal.Score = DealToBeat->Score;
	BestDeal.Resource = NULL;
	BestDeal.SectorA = NULL;
	BestDeal.SectorB = NULL;
	BestDeal.SellStation = NULL;
	BestDeal.BuyStation = NULL;

	if (SectorA->GetSectorBattleState(Company).HasDanger)
	{
		return BestDeal;
	}

	for (int32 SectorBIndex = 0; SectorBIndex < Company->GetVisitedSectors().Num(); SectorBIndex++)
	{
		UFlareSimulatedSector* SectorB = Company->GetVisitedSectors()[SectorBIndex];

		BestDeal = FindBestDealForShipToSector(Ship, SectorA, SectorB, &BestDeal, WorldResourceVariation, SectorBRestiction);
	}

	return BestDeal;
}


SectorDeal AITradeHelper::FindBestDealForShipToSector(UFlareSimulatedSpacecraft* Ship, UFlareSimulatedSector* SectorA, UFlareSimulatedSector* SectorB, SectorDeal* DealToBeat, TMap<UFlareSimulatedSector*, SectorVariation> const& WorldResourceVariation, UFlareSimulatedSector* SectorBRestiction)
{
	UFlareCompany* Company = Ship->GetCompany();
	AFlareGame* Game = Ship->GetGame();
	UFlareAIBehavior* Behavior = Company->GetAI()->GetBehavior();

	SectorDeal BestDeal = *DealToBeat;

	if(SectorBRestiction != nullptr && SectorBRestiction != SectorB && SectorA != SectorB)
	{
		return BestDeal;
	}

	int64 TravelTimeToA;
	int64 TravelTimeToB;

	if (SectorB->GetSectorBattleState(Company).HasDanger)
	{
		return BestDeal;
	}

	if (Ship->GetCurrentSector() == SectorA)
	{
		TravelTimeToA = 0;
	}
	else
	{
		TravelTimeToA = UFlareTravel::ComputeTravelDuration(Game->GetGameWorld(), Ship->GetCurrentSector(), SectorA, Company);
	}

	if (SectorA == SectorB)
	{
		// Stay in sector option
		TravelTimeToB = 0;
	}
	else
	{
		// Travel time

		TravelTimeToB = UFlareTravel::ComputeTravelDuration(Game->GetGameWorld(), SectorA, SectorB, Company);

	}
	int64 TravelTime = TravelTimeToA + TravelTimeToB;


#if DEBUG_AI_TRADING && DEBUG_AI_TRADING_FIND_DEAL
	if (Company->GetShortName() == DEBUG_AI_TRADING_COMPANY
			&& (!DEBUG_AI_TRADING_SECTOR_B_FILTER || SectorB->GetIdentifier() == DEBUG_AI_TRADING_SECTOR_B))
	{
		FLOGV("Travel %s -> %s -> %s : %lld days", *Ship->GetCurrentSector()->GetSectorName().ToString(),
		*SectorA->GetSectorName().ToString(), *SectorB->GetSectorName().ToString(), TravelTime);
	}
#endif

	SectorVariation const* SectorVariationA = &(WorldResourceVariation[SectorA]);
	SectorVariation const* SectorVariationB = &(WorldResourceVariation[SectorB]);

	for (int32 ResourceIndex = 0; ResourceIndex < Game->GetResourceCatalog()->Resources.Num(); ResourceIndex++)
	{
		FFlareResourceDescription* Resource = &Game->GetResourceCatalog()->Resources[ResourceIndex]->Data;
		struct ResourceVariation const* VariationA = &SectorVariationA->ResourceVariations[Resource];
		struct ResourceVariation const* VariationB = &SectorVariationB->ResourceVariations[Resource];

		//FLOGV("- Check for %s", *Resource->Name.ToString());

#if DEBUG_AI_TRADING && DEBUG_AI_TRADING_FIND_DEAL
	if (Company->GetShortName() == DEBUG_AI_TRADING_COMPANY
			&& (!DEBUG_AI_TRADING_SECTOR_B_FILTER || SectorB->GetIdentifier() == DEBUG_AI_TRADING_SECTOR_B)
			&& (!DEBUG_AI_TRADING_RESOURCES_FILTER || Resource->Identifier == DEBUG_AI_TRADING_RESOURCES))
	{
		FLOGV("- Check for %s", *Resource->Name.ToString());
		FLOGV("!VariationA->OwnedFlow %d",VariationA->OwnedFlow);
		FLOGV("!VariationA->FactoryFlow %d",VariationA->FactoryFlow);
		FLOGV("!VariationA->OwnedStock %d",VariationA->OwnedStock);
		FLOGV("!VariationA->FactoryStock %d",VariationA->FactoryStock);
		FLOGV("!VariationA->StorageStock %d",VariationA->StorageStock);
		FLOGV("!VariationA->OwnedCapacity %d",VariationA->OwnedCapacity);
		FLOGV("!VariationA->FactoryCapacity %d",VariationA->FactoryCapacity);
		FLOGV("!VariationA->StorageCapacity %d",VariationA->StorageCapacity);
		FLOGV("!VariationA->MaintenanceCapacity %d",VariationA->MaintenanceCapacity);
		FLOGV("!VariationA->MinCapacity %d",VariationA->MinCapacity);
		FLOGV("!VariationA->IncomingResources %d",VariationA->IncomingResources);
		FLOGV("!VariationA->ProviderCount %d",VariationA->ProviderCount);
		FLOGV("!VariationB->OwnedFlow %d",VariationB->OwnedFlow);
		FLOGV("!VariationB->FactoryFlow %d",VariationB->FactoryFlow);
		FLOGV("!VariationB->OwnedStock %d",VariationB->OwnedStock);
		FLOGV("!VariationB->FactoryStock %d",VariationB->FactoryStock);
		FLOGV("!VariationB->StorageStock %d",VariationB->StorageStock);
		FLOGV("!VariationB->OwnedCapacity %d",VariationB->OwnedCapacity);
		FLOGV("!VariationB->FactoryCapacity %d",VariationB->FactoryCapacity);
		FLOGV("!VariationB->StorageCapacity %d", VariationB->StorageCapacity);
		FLOGV("!VariationB->MaintenanceCapacity %d", VariationB->MaintenanceCapacity);
		FLOGV("!VariationB->MinCapacity %d", VariationB->MinCapacity);
		FLOGV("!VariationB->IncomingResources %d", VariationB->IncomingResources);
		FLOGV("  - VariationA %p", VariationA);
		FLOGV("  - VariationB %p", VariationB);
	}

	/*if (Resource->Identifier != "fuel")
	{
		return BestDeal;
	}*/
#endif


		if (!VariationA->OwnedFlow &&
			!VariationA->FactoryFlow &&
			!VariationA->OwnedStock &&
			!VariationA->FactoryStock &&
			!VariationA->StorageStock &&
			!VariationA->OwnedCapacity &&
			!VariationA->FactoryCapacity &&
			!VariationA->StorageCapacity &&
			!VariationA->MaintenanceCapacity &&
			!VariationA->MinCapacity &&
			!VariationB->OwnedFlow &&
			!VariationB->FactoryFlow &&
			!VariationB->OwnedStock &&
			!VariationB->FactoryStock &&
			!VariationB->StorageStock &&
			!VariationB->OwnedCapacity &&
			!VariationB->FactoryCapacity &&
			!VariationB->StorageCapacity &&
			!VariationB->MaintenanceCapacity &&
			!VariationB->MinCapacity)
		{
			continue;
		}


		int32 InitialQuantity = Ship->GetActiveCargoBay()->GetResourceQuantity(Resource, Ship->GetCompany());
		int32 FreeSpace = Ship->GetActiveCargoBay()->GetFreeSpaceForResource(Resource, Ship->GetCompany());

		int32 StockInAAfterTravel =
			VariationA->OwnedStock
			+ VariationA->FactoryStock
			+ VariationA->StorageStock
			- (VariationA->OwnedFlow * TravelTimeToA)
			- (VariationA->FactoryFlow * TravelTimeToA);
#if DEBUG_AI_TRADING && DEBUG_AI_TRADING_FIND_DEAL
		if (Company->GetShortName() == DEBUG_AI_TRADING_COMPANY
				&& (!DEBUG_AI_TRADING_SECTOR_B_FILTER || SectorB->GetIdentifier() == DEBUG_AI_TRADING_SECTOR_B)
				&& (!DEBUG_AI_TRADING_RESOURCES_FILTER || Resource->Identifier == DEBUG_AI_TRADING_RESOURCES))
		{
			FLOGV("InitialQuantity %d", InitialQuantity);
			FLOGV("StockInAAfterTravel %d", StockInAAfterTravel);
		}
#endif

		if (StockInAAfterTravel <= 0 && InitialQuantity == 0)
		{
			continue;
		}

		int32 CanBuyQuantity = FMath::Min(FreeSpace, StockInAAfterTravel);
		CanBuyQuantity = FMath::Max(0, CanBuyQuantity);

		// Affordable quantity
		CanBuyQuantity = FMath::Min(CanBuyQuantity, (int32)(Company->GetMoney() / SectorA->GetResourcePrice(Resource, EFlareResourcePriceContext::FactoryInput)));

		int32 TimeToGetB = TravelTime + (CanBuyQuantity > 0 ? 1 : 0); // If full, will not buy so no trade time in A

		int32 LocalCapacity = VariationB->OwnedCapacity
				+ VariationB->FactoryCapacity
				+ VariationB->StorageCapacity
				+ VariationB->MaintenanceCapacity;

		if (VariationB->MinCapacity > 0)
		{
			// The nerf system make big capacity malus in whole sector if a big station is near full
			// If there is an empty small station in the sector, this station will not get any resource
			// as the sector will be avoided by trade ships
			LocalCapacity = FMath::Max(LocalCapacity, VariationB->MinCapacity);
		}

		int32 CapacityInBAfterTravel =
			LocalCapacity
			+ VariationB->OwnedFlow * TimeToGetB
			+ VariationB->FactoryFlow * TimeToGetB;
		if (TimeToGetB > 0)
		{
			CapacityInBAfterTravel -= VariationB->IncomingResources;
		}

		int32 SellQuantity = FMath::Min(CapacityInBAfterTravel, CanBuyQuantity + InitialQuantity);
		int32  BuyQuantity = FMath::Max(0, SellQuantity - InitialQuantity);

		if(SectorA != SectorB && SectorBRestiction != nullptr && BuyQuantity == 0)
		{
			// Will have to wait the departure so do a better local deal
			continue;
		}

		// Use price details

		int32 MoneyGain = 0;
		int32 QuantityToSell = SellQuantity;

		int32 OwnedCapacity = FMath::Max(0, (int32)(VariationB->OwnedCapacity + VariationB->OwnedFlow * TravelTime));
		int32 MaintenanceCapacity = VariationB->MaintenanceCapacity;
		int32 FactoryCapacity = FMath::Max(0, (int32)(VariationB->FactoryCapacity + VariationB->FactoryFlow * TravelTime));
		int32 StorageCapacity = VariationB->StorageCapacity;

		if(OwnedCapacity + MaintenanceCapacity + FactoryCapacity + StorageCapacity < VariationB->MinCapacity)
		{
			FactoryCapacity += VariationB->MinCapacity;
		}

		int32 OwnedSellQuantity = FMath::Min(OwnedCapacity, QuantityToSell);
		MoneyGain += OwnedSellQuantity * SectorB->GetResourcePrice(Resource, EFlareResourcePriceContext::Default) * 2;
		QuantityToSell -= OwnedSellQuantity;

		int32 MaintenanceSellQuantity = FMath::Min(MaintenanceCapacity, QuantityToSell);
		MoneyGain += MaintenanceSellQuantity * SectorB->GetResourcePrice(Resource, EFlareResourcePriceContext::MaintenanceConsumption);
		QuantityToSell -= MaintenanceSellQuantity;

		int32 FactorySellQuantity = FMath::Min(FactoryCapacity, QuantityToSell);
		MoneyGain += FactorySellQuantity * SectorB->GetResourcePrice(Resource, EFlareResourcePriceContext::FactoryInput);
		QuantityToSell -= FactorySellQuantity;

		int32 StorageSellQuantity = FMath::Min(StorageCapacity, QuantityToSell);

		// If has some resource, consider that storage make gain nothing to prefer any solution except sell to a hub
		if(InitialQuantity > 0 && StorageSellQuantity > 0)
		{
			MoneyGain += 1;
		}
		else
		{
			MoneyGain += StorageSellQuantity * SectorB->GetResourcePrice(Resource, EFlareResourcePriceContext::HubInput);
		}

		QuantityToSell -= StorageSellQuantity;

		int32 MoneySpend = 0;


		if(BuyQuantity / (VariationA->ProviderCount > 0 ? VariationA->ProviderCount : 1) < InitialQuantity / 8)
		{
#if DEBUG_AI_TRADING && DEBUG_AI_TRADING_FIND_DEAL
			if (Company->GetShortName() == DEBUG_AI_TRADING_COMPANY
					&& (!DEBUG_AI_TRADING_SECTOR_B_FILTER || SectorB->GetIdentifier() == DEBUG_AI_TRADING_SECTOR_B)
					&& (!DEBUG_AI_TRADING_RESOURCES_FILTER || Resource->Identifier == DEBUG_AI_TRADING_RESOURCES))
			{
				FLOGV(" Too small QuantityToBuy=%d. Use 0 (ProviderCount=%d)", BuyQuantity, VariationA->ProviderCount);
			}
#endif
			BuyQuantity = 0;
		}

		int32 QuantityToBuy = BuyQuantity;

		int32 OwnedStock = FMath::Max(0, (int32)(VariationA->OwnedStock - VariationA->OwnedFlow * TravelTimeToA));
		int32 FactoryStock = FMath::Max(0, (int32)(VariationA->FactoryStock - VariationA->FactoryFlow * TravelTimeToA));
		int32 StorageStock = VariationA->StorageStock;


		int32 OwnedBuyQuantity = FMath::Min(OwnedStock, QuantityToBuy);
		MoneySpend += OwnedBuyQuantity * SectorA->GetResourcePrice(Resource, EFlareResourcePriceContext::Default) * 0.5;
		QuantityToBuy -= OwnedBuyQuantity;

		int32 FactoryBuyQuantity = FMath::Min(FactoryStock, QuantityToBuy);
		MoneySpend += FactoryBuyQuantity * SectorA->GetResourcePrice(Resource, EFlareResourcePriceContext::FactoryOutput);
		QuantityToBuy -= FactoryBuyQuantity;

		int32 StorageBuyQuantity = FMath::Min(StorageStock, QuantityToBuy);
		MoneySpend += StorageBuyQuantity * SectorA->GetResourcePrice(Resource, EFlareResourcePriceContext::HubOutput);
		QuantityToBuy -= StorageBuyQuantity;

		// TODO per station computation
		// TODO prefer own transport

		int32 MoneyBalance = MoneyGain - MoneySpend;

		int32 TransactionDuration = (TimeToGetB + 1); // 1 day to sell
		float MoneyBalanceParDay = (float)MoneyBalance / (float) TransactionDuration;

		bool Temporisation = false;
		if (BuyQuantity == 0 && Ship->GetCurrentSector() != SectorA)
		{
			// If can't buy in A and A is not local, it's just a temporisation route. Better to do nothing.
			// Accepting to be idle help to avoid building ships
			Temporisation = true;
		}

		MoneyBalanceParDay *= Behavior->GetResourceAffility(Resource);

		float Score = MoneyBalanceParDay
				* Behavior->GetResourceAffility(Resource)
				* (Behavior->GetSectorAffility(SectorA) + Behavior->GetSectorAffility(SectorB));

		if (VariationB->HighPriority > 0)
		{
			Score *= VariationB->HighPriority;
		}

#if DEBUG_AI_TRADING && DEBUG_AI_TRADING_FIND_DEAL
		if (Company->GetShortName() == DEBUG_AI_TRADING_COMPANY
				&& (!DEBUG_AI_TRADING_SECTOR_B_FILTER || SectorB->GetIdentifier() == DEBUG_AI_TRADING_SECTOR_B)
				&& (!DEBUG_AI_TRADING_RESOURCES_FILTER || Resource->Identifier == DEBUG_AI_TRADING_RESOURCES))
		{
			FLOGV(" -> IncomingCapacity=%d", SectorVariationA->IncomingCapacity);
			FLOGV(" -> IncomingResources=%d", VariationA->IncomingResources);
			FLOGV(" -> InitialQuantity=%d", InitialQuantity);
			FLOGV(" -> FreeSpace=%d", FreeSpace);
			FLOGV(" -> StockInAAfterTravel=%d", StockInAAfterTravel);
			FLOGV(" -> BuyQuantity=%d", BuyQuantity);
			FLOGV(" -> CapacityInBAfterTravel=%d", CapacityInBAfterTravel);
			FLOGV(" -> SellQuantity=%d", SellQuantity);
			FLOGV(" -> MoneyGain=%f", MoneyGain/100.f);
			FLOGV(" -> MoneySpend=%f", MoneySpend/100.f);
			FLOGV("   -> OwnedBuyQuantity=%d", OwnedBuyQuantity);
			FLOGV("   -> FactoryBuyQuantity=%d", FactoryBuyQuantity);
			FLOGV("   -> StorageBuyQuantity=%d", StorageBuyQuantity);
			FLOGV(" -> MoneyBalance=%f", MoneyBalance/100.f);
			FLOGV(" -> MoneyBalanceParDay=%f", MoneyBalanceParDay/100.f);
			FLOGV(" -> Resource affility=%f", Behavior->GetResourceAffility(Resource));
			FLOGV(" -> SectorA affility=%f", Behavior->GetSectorAffility(SectorA));
			FLOGV(" -> SectorB affility=%f", Behavior->GetSectorAffility(SectorB));
			FLOGV(" -> Temporisation=%d", Temporisation);
			FLOGV(" -> HighPriority=%d", VariationB->HighPriority);
			FLOGV(" -> TransactionDuration=%d", TransactionDuration);
			FLOGV(" -> Score=%f", Score);
		}
#endif

		if (Score > BestDeal.Score && !Temporisation)
		{
			BestDeal.Score = Score;
			BestDeal.SectorA = SectorA;
			BestDeal.SectorB = SectorB;
			BestDeal.Resource = Resource;
			BestDeal.BuyQuantity = BuyQuantity;

#if DEBUG_AI_TRADING && DEBUG_AI_TRADING_FIND_DEAL
			if (Company->GetShortName() == DEBUG_AI_TRADING_COMPANY)
			{


				FLOGV("New Best Resource %s", *Resource->Name.ToString())
				FLOGV("Travel %s -> %s -> %s : %lld days", *Ship->GetCurrentSector()->GetSectorName().ToString(),
					*SectorA->GetSectorName().ToString(), *SectorB->GetSectorName().ToString(), TravelTime);

				FLOGV(" -> IncomingCapacity=%d", SectorVariationA->IncomingCapacity);
				FLOGV(" -> IncomingResources=%d", VariationA->IncomingResources);
				FLOGV(" -> InitialQuantity=%d", InitialQuantity);
				FLOGV(" -> FreeSpace=%d", FreeSpace);
				FLOGV(" -> StockInAAfterTravel=%d", StockInAAfterTravel);
				FLOGV(" -> BuyQuantity=%d", BuyQuantity);
				FLOGV(" -> CapacityInBAfterTravel=%d", CapacityInBAfterTravel);
				FLOGV(" -> SellQuantity=%u", SellQuantity);
				FLOGV(" -> MoneyGain=%f", MoneyGain/100.f);
				FLOGV(" -> MoneySpend=%f", MoneySpend/100.f);
				FLOGV("   -> OwnedBuyQuantity=%d", OwnedBuyQuantity);
				FLOGV("   -> FactoryBuyQuantity=%d", FactoryBuyQuantity);
				FLOGV("   -> StorageBuyQuantity=%d", StorageBuyQuantity);
				FLOGV(" -> MoneyBalance=%f", MoneyBalance/100.f);
				FLOGV(" -> MoneyBalanceParDay=%f", MoneyBalanceParDay/100.f);
				FLOGV(" -> Resource affility=%f", Behavior->GetResourceAffility(Resource));
				FLOGV(" -> SectorA affility=%f", Behavior->GetSectorAffility(SectorA));
				FLOGV(" -> SectorB affility=%f", Behavior->GetSectorAffility(SectorB));
				FLOGV(" -> HighPriority=%d", VariationB->HighPriority);
				FLOGV(" -> TransactionDuration=%d", TransactionDuration);
				FLOGV(" -> Score=%f", Score);
			}
#endif
		}
	}

	return BestDeal;
}

SectorDeal AITradeHelper::FindBestDealForShip(UFlareSimulatedSpacecraft* Ship, TMap<UFlareSimulatedSector*, SectorVariation>& WorldResourceVariation, UFlareSimulatedSector* SectorARestiction, UFlareSimulatedSector* SectorBRestiction)
{
	SCOPE_CYCLE_COUNTER(STAT_AITradeHelper_FindBestDealForShip);

	UFlareCompany* Company = Ship->GetCompany();

	SectorDeal BestDeal;
	BestDeal.BuyQuantity = 0;
	BestDeal.Score = 0;
	BestDeal.Resource = NULL;
	BestDeal.SectorA = NULL;
	BestDeal.SectorB = NULL;
	BestDeal.SellStation = NULL;
	BestDeal.BuyStation = NULL;

	// Sell only option
	for (int32 SectorAIndex = 0; SectorAIndex < Company->GetVisitedSectors().Num(); SectorAIndex++)
	{
		SCOPE_CYCLE_COUNTER(STAT_AITradeHelper_FindBestDealForShip_Sectors);
		UFlareSimulatedSector* SectorA = Company->GetVisitedSectors()[SectorAIndex];

		if(SectorARestiction != nullptr && SectorARestiction != SectorA)
		{
			continue;
		}

		SectorDeal SectorBestDeal;
		SectorBestDeal.Resource = NULL;
		SectorBestDeal.BuyQuantity = 0;
		SectorBestDeal.Score = 0;
		SectorBestDeal.Resource = NULL;
		SectorBestDeal.SectorA = NULL;
		SectorBestDeal.SectorB = NULL;
		SectorBestDeal.SellStation = NULL;
		SectorBestDeal.BuyStation = NULL;

		while (true)
		{
			SCOPE_CYCLE_COUNTER(STAT_AITradeHelper_FindBestDealForShip_Loop);

			SectorBestDeal = AITradeHelper::FindBestDealForShipFromSector(Ship, SectorA, &BestDeal, WorldResourceVariation, SectorBRestiction);
			if (!SectorBestDeal.Resource)
			{
				// No best deal found
				break;
			}

			SectorVariation* SectorVariationA = &WorldResourceVariation[SectorA];
			if (Ship->GetCurrentSector() != SectorA && SectorVariationA->IncomingCapacity > 0 && SectorBestDeal.BuyQuantity > 0)
			{
				//FLOGV("UFlareCompanyAI::UpdateTrading : IncomingCapacity to %s = %d", *SectorA->GetSectorName().ToString(), SectorVariationA->IncomingCapacity);
				int32 UsedIncomingCapacity = FMath::Min(SectorBestDeal.BuyQuantity, SectorVariationA->IncomingCapacity);

				SectorVariationA->IncomingCapacity -= UsedIncomingCapacity;
				struct ResourceVariation* VariationA = &SectorVariationA->ResourceVariations[SectorBestDeal.Resource];
				VariationA->OwnedStock -= UsedIncomingCapacity;
			}
			else
			{
				break;
			}
		}

		if (SectorBestDeal.Resource)
		{
			BestDeal = SectorBestDeal;
		}
	}

	return BestDeal;
}

void AITradeHelper::ApplyDeal(UFlareSimulatedSpacecraft* Ship, SectorDeal const&Deal, TMap<UFlareSimulatedSector*, SectorVariation>* WorldResourceVariation, bool AllowTravel, bool AllowUseNoTradeForMe)
{
	SCOPE_CYCLE_COUNTER(STAT_AITradeHelper_ApplyDeal);

	AFlareGame* Game = Ship->GetGame();


#if DEBUG_AI_TRADING
	if (Ship->GetCompany()->GetShortName() == DEBUG_AI_TRADING_COMPANY)
	{
		FLOGV("UFlareCompanyAI::UpdateTrading : Best balance for %s (%s) : %f score",
			*Ship->GetImmatriculation().ToString(), *Ship->GetCurrentSector()->GetSectorName().ToString(), Deal.Score / 100);
		FLOGV("UFlareCompanyAI::UpdateTrading -> Transfer %s from %s to %s",
			*Deal.Resource->Name.ToString(), *Deal.SectorA->GetSectorName().ToString(), *Deal.SectorB->GetSectorName().ToString());
	}
#endif
	if (Ship->GetCurrentSector() == Deal.SectorA)
	{
		// Already in A, buy resources and go to B
		if (Deal.BuyQuantity == 0)
		{
			if (Ship->GetCurrentSector() != Deal.SectorB)
			{
				// Already buy resources,go to B
				if(AllowTravel)
				{
					Game->GetGameWorld()->StartTravel(Ship->GetCurrentFleet(), Deal.SectorB);
#if DEBUG_AI_TRADING
					if (Ship->GetCompany()->GetShortName() == DEBUG_AI_TRADING_COMPANY)
					{
						FLOGV("UFlareCompanyAI::UpdateTrading -> Travel to %s to sell", *Deal.SectorB->GetSectorName().ToString());
					}
#endif
				 }
				else
				{
#if DEBUG_AI_TRADING
					if (Ship->GetCompany()->GetShortName() == DEBUG_AI_TRADING_COMPANY)
					{
						FLOGV("UFlareCompanyAI::UpdateTrading -> Travel to %s to sell not allowed", *Deal.SectorB->GetSectorName().ToString());
					}
#endif
				}
			}
		}
		else
		{

			SectorHelper::FlareTradeRequest Request;
			Request.Resource = Deal.Resource;
			Request.Operation = EFlareTradeRouteOperation::Load;
			Request.Client = Ship;
			Request.CargoLimit = 0.f;
			Request.AllowStorage = true;
			Request.IsDonation = false;
			Request.AllowUseNoTradeForMe = AllowUseNoTradeForMe;
			Request.LoadUnloadPriority = 10.f;
			Request.BuySellPriority = 1.f;

			if(Deal.Resource == Game->GetScenarioTools()->FleetSupply)
			{
				Request.MaxQuantity = FMath::Min(Deal.BuyQuantity, Ship->GetActiveCargoBay()->GetFreeSpaceForResource(Deal.Resource, Ship->GetCompany()));
			}
			else
			{
				Request.MaxQuantity = Ship->GetActiveCargoBay()->GetFreeSpaceForResource(Deal.Resource, Ship->GetCompany());
			}

			UFlareSimulatedSpacecraft* StationCandidate = Deal.BuyStation != nullptr ? Deal.BuyStation : SectorHelper::FindTradeStation(Request);

			int32 BroughtResource = 0;

			if (StationCandidate)
			{
				if (!StationCandidate->IsUnderConstruction() && StationCandidate->IsComplex())
				{
					if(StationCandidate->GetActiveCargoBay()->WantBuy(Deal.Resource, Ship->GetCompany()) && StationCandidate->GetActiveCargoBay()->WantSell(Deal.Resource, Ship->GetCompany()))
					{
						int32 TotalCapacity = StationCandidate->GetActiveCargoBay()->GetTotalCapacityForResource(Deal.Resource, Ship->GetCompany());
						int32 Stock = StationCandidate->GetActiveCargoBay()->GetResourceQuantity(Deal.Resource, Ship->GetCompany());
						Stock = FMath::Max(0, Stock - TotalCapacity / 2);
						Request.MaxQuantity = FMath::Min(Stock, Request.MaxQuantity);
					}
				}

				int64 TransactionPrice;
				BroughtResource = SectorHelper::Trade(StationCandidate, Ship, Deal.Resource, Request.MaxQuantity, &TransactionPrice);
				Ship->GetCurrentFleet()->GetData()->AutoTradeStatsLoadResources += BroughtResource;
				Ship->GetCurrentFleet()->GetData()->AutoTradeStatsMoneyBuy += TransactionPrice;

#if DEBUG_AI_TRADING
				if (Ship->GetCompany()->GetShortName() == DEBUG_AI_TRADING_COMPANY)
				{
					FLOGV("UFlareCompanyAI::UpdateTrading -> Buy %d / %d to %s", BroughtResource, Deal.BuyQuantity, *StationCandidate->GetImmatriculation().ToString());
				}
#endif
#if DEBUG_AI_TRADING_STATS
				if(Ship->GetCurrentFleet()->IsAutoTrading())
				{
					FLOGV("Auto trading %s buy %d / %d %s to %s for %lld", *Ship->GetImmatriculation().ToString(), BroughtResource, Request.MaxQuantity, *Deal.Resource->Name.ToString(), *StationCandidate->GetImmatriculation().ToString(), TransactionPrice);
					FLOGV("AutoTradeStatsDays=%d LoadResources=%d UnloadResources=%d MoneyBuy=%lld MoneySell=%lld",
						  Ship->GetCurrentFleet()->GetData()->AutoTradeStatsDays,
						  Ship->GetCurrentFleet()->GetData()->AutoTradeStatsLoadResources,
						  Ship->GetCurrentFleet()->GetData()->AutoTradeStatsUnloadResources,
						  Ship->GetCurrentFleet()->GetData()->AutoTradeStatsMoneyBuy,
						  Ship->GetCurrentFleet()->GetData()->AutoTradeStatsMoneySell);
				}
#endif

			}

			// TODO reduce computed sector stock

			if(WorldResourceVariation != nullptr)
			{
				if (BroughtResource > 0)
				{
					// Virtualy decrease the stock for other ships in sector A
					SectorVariation* SectorVariationA = &(*WorldResourceVariation)[Deal.SectorA];
					struct ResourceVariation* VariationA = &SectorVariationA->ResourceVariations[Deal.Resource];
					VariationA->OwnedStock -= BroughtResource;


					// Virtualy say some capacity arrive in sector B
					SectorVariation* SectorVariationB = &(*WorldResourceVariation)[Deal.SectorB];
					SectorVariationB->IncomingCapacity += BroughtResource;

					// Virtualy decrease the capacity for other ships in sector B
					struct ResourceVariation* VariationB = &SectorVariationB->ResourceVariations[Deal.Resource];
					VariationB->OwnedCapacity -= BroughtResource;
				}
				else if (BroughtResource == 0)
				{
					// Failed to buy the promised resources, remove the deal from the list
					SectorVariation* SectorVariationA = &(*WorldResourceVariation)[Deal.SectorA];
					struct ResourceVariation* VariationA = &SectorVariationA->ResourceVariations[Deal.Resource];
					VariationA->FactoryStock = 0;
					VariationA->OwnedStock = 0;
					VariationA->StorageStock = 0;
					if (VariationA->OwnedFlow > 0)
						VariationA->OwnedFlow = 0;
					if (VariationA->FactoryFlow > 0)
						VariationA->FactoryFlow = 0;
	#if DEBUG_AI_TRADING
					if (Ship->GetCompany()->GetShortName() == DEBUG_AI_TRADING_COMPANY)
					{
						FLOG("UFlareCompanyAI::UpdateTrading -> Buy failed, remove the deal from the list");
					}
	#endif
				}
			}
		}
	}
	else
	{
		if (Deal.SectorA != Ship->GetCurrentSector())
		{
			if(AllowTravel)
			{
				Game->GetGameWorld()->StartTravel(Ship->GetCurrentFleet(), Deal.SectorA);
#if DEBUG_AI_TRADING
				if (Ship->GetCompany()->GetShortName() == DEBUG_AI_TRADING_COMPANY)
				{
					FLOGV("UFlareCompanyAI::UpdateTrading -> Travel to %s to buy", *Deal.SectorA->GetSectorName().ToString());
				}
#endif
			}
			else
			{
#if DEBUG_AI_TRADING
				if (Ship->GetCompany()->GetShortName() == DEBUG_AI_TRADING_COMPANY)
				{
					FLOGV("UFlareCompanyAI::UpdateTrading -> Travel to %s to buy not allowed", *Deal.SectorA->GetSectorName().ToString());
				}
#endif
			}
		}

		if(WorldResourceVariation != nullptr)
		{
			// Reserve the deal by virtualy decrease the stock for other ships
			SectorVariation* SectorVariationA = &(*WorldResourceVariation)[Deal.SectorA];
			struct ResourceVariation* VariationA = &SectorVariationA->ResourceVariations[Deal.Resource];
			VariationA->OwnedStock -= Deal.BuyQuantity;
			// Virtualy say some capacity arrive in sector B
			SectorVariation* SectorVariationB = &(*WorldResourceVariation)[Deal.SectorB];
			SectorVariationB->IncomingCapacity += Deal.BuyQuantity;

			// Virtualy decrease the capacity for other ships in sector B
			struct ResourceVariation* VariationB = &SectorVariationB->ResourceVariations[Deal.Resource];
			VariationB->OwnedCapacity -= Deal.BuyQuantity;
		}
	}

	if (Ship->GetCurrentSector() == Deal.SectorB && !Ship->IsTrading())
	{
		// Try to sell
		// Try to unload or sell
		SectorHelper::FlareTradeRequest Request;
		Request.Resource = Deal.Resource;
		Request.Operation = EFlareTradeRouteOperation::Unload;
		Request.Client = Ship;
		Request.CargoLimit = 1.f;
		Request.MaxQuantity = Ship->GetActiveCargoBay()->GetResourceQuantity(Deal.Resource, Ship->GetCompany());
		Request.AllowStorage = false;
		Request.AllowUseNoTradeForMe = AllowUseNoTradeForMe;
		Request.LoadUnloadPriority = 10.f;
		Request.BuySellPriority = 1.f;

#if DEBUG_AI_TRADING
		if (Ship->GetCompany()->GetShortName() == DEBUG_AI_TRADING_COMPANY)
		{
			FLOGV("UFlareCompanyAI::UpdateTrading -> FindTradeStation for max %d",  Request.MaxQuantity);
		}
#endif

		UFlareSimulatedSpacecraft* StationCandidate = Deal.SellStation != nullptr ? Deal.SellStation : SectorHelper::FindTradeStation(Request);

		if (StationCandidate)
		{
			if (!StationCandidate->IsUnderConstruction() && StationCandidate->IsComplex())
			{
				if(StationCandidate->GetActiveCargoBay()->WantBuy(Deal.Resource, Ship->GetCompany()) && StationCandidate->GetActiveCargoBay()->WantSell(Deal.Resource, Ship->GetCompany()))
				{
					int32 TotalCapacity = StationCandidate->GetActiveCargoBay()->GetTotalCapacityForResource(Deal.Resource, Ship->GetCompany());
					int32 Capacity = StationCandidate->GetActiveCargoBay()->GetFreeSpaceForResource(Deal.Resource, Ship->GetCompany());
					Capacity = FMath::Max(0, Capacity - TotalCapacity / 2);
					Request.MaxQuantity = FMath::Min(Capacity, Request.MaxQuantity);
				}
			}

			int64 TransactionPrice;
			int32 SellQuantity = SectorHelper::Trade(Ship, StationCandidate, Deal.Resource, Request.MaxQuantity, &TransactionPrice);
			Ship->GetCurrentFleet()->GetData()->AutoTradeStatsUnloadResources += SellQuantity;
			Ship->GetCurrentFleet()->GetData()->AutoTradeStatsMoneySell += TransactionPrice;
#if DEBUG_AI_TRADING
			if (Ship->GetCompany()->GetShortName() == DEBUG_AI_TRADING_COMPANY)
			{
				FLOGV("UFlareCompanyAI::UpdateTrading -> Sell %d / %d to %s", SellQuantity, Request.MaxQuantity, *StationCandidate->GetImmatriculation().ToString());
			}
#endif
#if DEBUG_AI_TRADING_STATS
			if(Ship->GetCurrentFleet()->IsAutoTrading())
			{
				FLOGV("Auto trading %s sell %d / %d %s to %s for %lld", *Ship->GetImmatriculation().ToString(), SellQuantity, Request.MaxQuantity, *Deal.Resource->Name.ToString(), *StationCandidate->GetImmatriculation().ToString(), TransactionPrice);
				FLOGV("AutoTradeStatsDays=%d LoadResources=%d UnloadResources=%d MoneyBuy=%lld MoneySell=%lld",
					  Ship->GetCurrentFleet()->GetData()->AutoTradeStatsDays,
					  Ship->GetCurrentFleet()->GetData()->AutoTradeStatsLoadResources,
					  Ship->GetCurrentFleet()->GetData()->AutoTradeStatsUnloadResources,
					  Ship->GetCurrentFleet()->GetData()->AutoTradeStatsMoneyBuy,
					  Ship->GetCurrentFleet()->GetData()->AutoTradeStatsMoneySell);
			}
#endif

		}
	}
}

SectorVariation AITradeHelper::ComputeSectorResourceVariation(UFlareCompany* Company, UFlareSimulatedSector* Sector, bool AllowUseNoTradeForMe, bool FactorIncoming)
{
	AFlareGame* Game = Company->GetGame();
	UFlareAIBehavior* Behavior = Company->GetAI()->GetBehavior();
	UFlareCompany* ClientCompany = AllowUseNoTradeForMe ? Company : nullptr;

#if DEBUG_AI_TRADING
				if (Company->GetShortName() == DEBUG_AI_TRADING_COMPANY
						&& (!DEBUG_AI_TRADING_SECTOR_FILTER || Sector->GetIdentifier() == DEBUG_AI_TRADING_SECTOR))
				{
					FLOGV("ComputeSectorResourceVariation for sector %s)"
						  , *Sector->GetSectorName().ToString());
				}
#endif

	SectorVariation SectorVariation;
	SectorVariation.ResourceVariations.Reserve(Game->GetResourceCatalog()->Resources.Num());
	for (int32 ResourceIndex = 0; ResourceIndex < Game->GetResourceCatalog()->Resources.Num(); ResourceIndex++)
	{
		FFlareResourceDescription* Resource = &Game->GetResourceCatalog()->Resources[ResourceIndex]->Data;
		struct ResourceVariation ResourceVariation;
		ResourceVariation.OwnedFlow = 0;
		ResourceVariation.FactoryFlow = 0;
		ResourceVariation.OwnedStock = 0;
		ResourceVariation.FactoryStock = 0;
		ResourceVariation.StorageStock = 0;
		ResourceVariation.ProviderCount = 0;
		ResourceVariation.OwnedCapacity = 0;
		ResourceVariation.FactoryCapacity = 0;
		ResourceVariation.StorageCapacity = 0;
		ResourceVariation.MaintenanceCapacity = 0;
		ResourceVariation.IncomingResources = 0;
		ResourceVariation.MinCapacity = 0;
		ResourceVariation.ConsumerMaxStock = 0;
		ResourceVariation.MaintenanceMaxStock = 0;
		ResourceVariation.HighPriority = 0;

		SectorVariation.ResourceVariations.Add(Resource, ResourceVariation);
	}

	int32 OwnedCustomerStation = 0;
	int32 NotOwnedCustomerStation = 0;

	for (int32 StationIndex = 0 ; StationIndex < Sector->GetSectorStations().Num(); StationIndex++)
	{
		UFlareSimulatedSpacecraft* Station = Sector->GetSectorStations()[StationIndex];

#if DEBUG_AI_TRADING
				if (Company->GetShortName() == DEBUG_AI_TRADING_COMPANY
						&& (!DEBUG_AI_TRADING_SECTOR_FILTER || Sector->GetIdentifier() == DEBUG_AI_TRADING_SECTOR))
				{
					FLOGV("Station %s)"
						  , *Station->GetImmatriculation().ToString());
				}
#endif


		if (Station->IsHostile(Company))
		{
			continue;
		}

		bool IsConstruction = Station->IsUnderConstruction();
		bool IsShipyard = Station->IsShipyard();
		UFlareScenarioTools* ST = Game->GetScenarioTools();

		for (int32 ResourceIndex = 0; ResourceIndex < Game->GetResourceCatalog()->Resources.Num(); ResourceIndex++)
		{
			FFlareResourceDescription* Resource = &Game->GetResourceCatalog()->Resources[ResourceIndex]->Data;
			struct ResourceVariation* Variation = &SectorVariation.ResourceVariations[Resource];

			int32 Stock = 0;
			int32 Capacity = 0;

			UFlareCargoBay* CargoBay = Station->GetActiveCargoBay();

			if(CargoBay->WantBuy(Resource, ClientCompany))
			{
				Capacity = CargoBay->GetFreeSpaceForResource(Resource, ClientCompany);
			}

			if(CargoBay->WantSell(Resource, ClientCompany))
			{

				if (Company != Station->GetCompany() && Station->HasCapability(EFlareSpacecraftCapability::Maintenance) && Resource == Sector->GetGame()->GetScenarioTools()->FleetSupply)
				{
					Stock = 0;
				}
				else
				{
					Stock = CargoBay->GetResourceQuantity(Resource, ClientCompany, 1);
				}

				if(Stock > 0)
				{
					Variation->ProviderCount++;
				}
#if DEBUG_AI_TRADING
				if (Company->GetShortName() == DEBUG_AI_TRADING_COMPANY
						&& (!DEBUG_AI_TRADING_SECTOR_FILTER || Sector->GetIdentifier() == DEBUG_AI_TRADING_SECTOR)
						&& (!DEBUG_AI_TRADING_RESOURCES_FILTER || Resource->Identifier == DEBUG_AI_TRADING_RESOURCES))
				{
					FLOGV("Stock of %s in %s, (sector %s): %d"
						  , *Resource->Name.ToString()
						  , *Station->GetImmatriculation().ToString()
						  , *Sector->GetSectorName().ToString()
						  , Stock);
					FLOGV("Behavior->TradingBuy %f", Behavior->TradingBuy);
				}
#endif
			}

			if (!IsConstruction)
			{
				if (Station->HasCapability(EFlareSpacecraftCapability::Storage))
				{
					// Don't buy to hub
					Capacity = 0;
				}
				if (Station->IsComplex())
				{
					if (Station->GetActiveCargoBay()->WantBuy(Resource, ClientCompany) && Station->GetActiveCargoBay()->WantSell(Resource, ClientCompany))
					{
						int32 TotalCapacity = CargoBay->GetTotalCapacityForResource(Resource, ClientCompany);
							Capacity = FMath::Max(0, Capacity - TotalCapacity / 2);
							Stock = FMath::Max(0, Stock - TotalCapacity / 2);
					}
				}
			}

			if (Company == Station->GetCompany())
			{
				Variation->OwnedStock += Stock;
				Variation->OwnedCapacity += Capacity;

			}
			else
			{
				Variation->FactoryStock += Stock * Behavior->TradingBuy;

				int32 CanBuyQuantity =  FMath::Max(0, (int32) (Station->GetCompany()->GetMoney() / Sector->GetResourcePrice(Resource, EFlareResourcePriceContext::FactoryInput)));
				Capacity = FMath::Min(Capacity, CanBuyQuantity);
				Variation->FactoryCapacity += Capacity * Behavior->TradingSell;
			}
		}

		int32 InitialSlotCapacity = Station->GetActiveCargoBay()->GetSlotCapacity();

		for (int32 FactoryIndex = 0; FactoryIndex < Station->GetFactories().Num(); FactoryIndex++)
		{
			UFlareFactory* Factory = Station->GetFactories()[FactoryIndex];

#if DEBUG_AI_TRADING
				if (Company->GetShortName() == DEBUG_AI_TRADING_COMPANY
						&& (!DEBUG_AI_TRADING_SECTOR_FILTER || Sector->GetIdentifier() == DEBUG_AI_TRADING_SECTOR))
				{
					FLOGV("Factory %s)"
						  , *Factory->GetDescription()->Name.ToString());
				}
#endif

			if ((!Factory->IsActive() || !Factory->IsNeedProduction()))
			{
				// No resources needed
				continue;
			}

			// Input flow
			for (int32 ResourceIndex = 0; ResourceIndex < Factory->GetInputResourcesCount(); ResourceIndex++)
			{
				FFlareResourceDescription* Resource = Factory->GetInputResource(ResourceIndex);
				struct ResourceVariation* Variation = &SectorVariation.ResourceVariations[Resource];

				int64 ProductionDuration = Factory->GetProductionDuration();
				if (ProductionDuration == 0)
				{
					ProductionDuration = 10;
				}

				if (!Company->IsPlayerCompany())
				{
					if (IsConstruction)
					{
						Variation->HighPriority += 1000000;
					}
					else if(IsShipyard && Company != ST->Pirates)
					{
						Variation->HighPriority += 1500000;
					}
				}

				int32 Flow = FMath::CeilToInt(float(Factory->GetInputResourceQuantity(ResourceIndex)) / float(ProductionDuration));
				int32 CanBuyQuantity =  FMath::Max(0, (int32) (Station->GetCompany()->GetMoney() / Sector->GetResourcePrice(Resource, EFlareResourcePriceContext::FactoryInput)));

				if (Flow == 0)
				{
					continue;
				}

				if (Factory->IsProducing())
				{
					if (Company == Station->GetCompany())
					{
						Variation->OwnedFlow += Flow;
					}
					else
					{
						Flow = FMath::Min(Flow, CanBuyQuantity);
						Variation->FactoryFlow += Flow;
					}
				}
			}

			// Ouput flow
			for (int32 ResourceIndex = 0; ResourceIndex < Factory->GetOutputResourcesCount(); ResourceIndex++)
			{
				FFlareResourceDescription* Resource = Factory->GetOutputResource(ResourceIndex);
				struct ResourceVariation* Variation = &SectorVariation.ResourceVariations[Resource];

				int64 ProductionDuration = Factory->GetProductionDuration();
				if (ProductionDuration == 0)
				{
					ProductionDuration = 10;
				}



				int32 Flow = FMath::CeilToInt(float(Factory->GetOutputResourceQuantity(ResourceIndex)) / float(ProductionDuration));


				if (Flow == 0)
				{
					continue;
				}

				if (Factory->IsProducing())
				{
					if (Company == Station->GetCompany())
					{
						Variation->OwnedFlow -= Flow;
					}
					else
					{
						Variation->FactoryFlow -= Flow;
					}
				}
			}
		}

		// Customer flow
		if (!IsConstruction)
		{
			if (Station->HasCapability(EFlareSpacecraftCapability::Consumer))
			{
				if (Company == Station->GetCompany())
				{
					OwnedCustomerStation++;
				}
				else
				{
					NotOwnedCustomerStation++;
				}

				for (int32 ResourceIndex = 0; ResourceIndex < Game->GetResourceCatalog()->ConsumerResources.Num(); ResourceIndex++)
				{
					FFlareResourceDescription* Resource = &Game->GetResourceCatalog()->ConsumerResources[ResourceIndex]->Data;
					struct ResourceVariation* Variation = &SectorVariation.ResourceVariations[Resource];

					Variation->ConsumerMaxStock += Station->GetActiveCargoBay()->GetSlotCapacity();
				}
			}

			// Maintenance
			if (Station->HasCapability(EFlareSpacecraftCapability::Maintenance))
			{
				for (int32 ResourceIndex = 0; ResourceIndex < Game->GetResourceCatalog()->MaintenanceResources.Num(); ResourceIndex++)
				{
					FFlareResourceDescription* Resource = &Game->GetResourceCatalog()->MaintenanceResources[ResourceIndex]->Data;
					struct ResourceVariation* Variation = &SectorVariation.ResourceVariations[Resource];


					Variation->MaintenanceMaxStock += Station->GetActiveCargoBay()->GetSlotCapacity();
				}
			}

			// Storage
			if (Station->HasCapability(EFlareSpacecraftCapability::Storage))
			{
				for (int32 ResourceIndex = 0; ResourceIndex < Game->GetResourceCatalog()->Resources.Num(); ResourceIndex++)
				{
					FFlareResourceDescription* Resource = &Game->GetResourceCatalog()->Resources[ResourceIndex]->Data;
					struct ResourceVariation* Variation = &SectorVariation.ResourceVariations[Resource];

					int32 ResourceQuantity = Station->GetActiveCargoBay()->GetResourceQuantity(Resource, ClientCompany);
					int32 MaxCapacity = Station->GetActiveCargoBay()->GetFreeSpaceForResource(Resource, ClientCompany);

					int32 CanBuyQuantity = (int32)(Station->GetCompany()->GetMoney() / Sector->GetResourcePrice(Resource, EFlareResourcePriceContext::HubInput));
					int32 Capacity = FMath::Min(MaxCapacity, (InitialSlotCapacity - ResourceQuantity));
					int32 SlotCapacity = InitialSlotCapacity;


					float BaseSlotCapacity = SlotCapacity / Station->GetLevel();

					if (ResourceQuantity < SlotCapacity)
					{

						if (Company == Station->GetCompany())
						{
							//Variation->OwnedCapacity += Capacity;
						}
						else
						{
							Capacity = FMath::Min(Capacity, CanBuyQuantity);
							//Variation->StorageCapacity += Capacity * Behavior->TradingSell;
						}
					}
					Variation->MaintenanceMaxStock += Station->GetActiveCargoBay()->GetSlotCapacity();
				}
			}
		}
	}

	if (OwnedCustomerStation || NotOwnedCustomerStation)
	{
		float OwnedCustomerRatio = (float) OwnedCustomerStation / (float) (OwnedCustomerStation + NotOwnedCustomerStation);
		float NotOwnedCustomerRatio = (float) NotOwnedCustomerStation / (float) (OwnedCustomerStation + NotOwnedCustomerStation);

		for (int32 ResourceIndex = 0; ResourceIndex < Game->GetResourceCatalog()->ConsumerResources.Num(); ResourceIndex++)
		{
			FFlareResourceDescription* Resource = &Game->GetResourceCatalog()->ConsumerResources[ResourceIndex]->Data;
			struct ResourceVariation* Variation = &SectorVariation.ResourceVariations[Resource];


			int32 Consumption = Sector->GetPeople()->GetRessourceConsumption(Resource, false);

			Variation->OwnedFlow = OwnedCustomerRatio * Consumption;
			Variation->FactoryFlow = NotOwnedCustomerRatio * Consumption * Behavior->TradingSell;
		}
	}

	// Compute incoming capacity and resources
	SectorVariation.IncomingCapacity = 0;
	if (FactorIncoming)
	{
		for (int32 TravelIndex = 0; TravelIndex < Game->GetGameWorld()->GetTravels().Num(); TravelIndex++)
		{
			UFlareTravel* Travel = Game->GetGameWorld()->GetTravels()[TravelIndex];
			if (Travel->GetDestinationSector() != Sector)
			{
				continue;
			}

			int64 RemainingTravelDuration = FMath::Max((int64)1, Travel->GetRemainingTravelDuration());

			UFlareFleet* IncomingFleet = Travel->GetFleet();

			for (int32 ShipIndex = 0; ShipIndex < IncomingFleet->GetShips().Num(); ShipIndex++)
			{
				UFlareSimulatedSpacecraft* Ship = IncomingFleet->GetShips()[ShipIndex];

				if (Ship->GetActiveCargoBay()->GetSlotCapacity() == 0 && Ship->GetDamageSystem()->IsStranded())
				{
					continue;
				}

				if (Ship->GetCompany()->GetMoney() > 0)
				{
					SectorVariation.IncomingCapacity += Ship->GetActiveCargoBay()->GetCapacity() / RemainingTravelDuration;
				}


				TArray<FFlareCargo>& CargoBaySlots = Ship->GetActiveCargoBay()->GetSlots();
				for (int32 CargoIndex = 0; CargoIndex < CargoBaySlots.Num(); CargoIndex++)
				{
					FFlareCargo& Cargo = CargoBaySlots[CargoIndex];

					if (!Cargo.Resource)
					{
						continue;
					}
					struct ResourceVariation* Variation = &SectorVariation.ResourceVariations[Cargo.Resource];

					Variation->IncomingResources += Cargo.Quantity / (RemainingTravelDuration * 0.5);
				}
			}
		}
	}

	// Add damage fleet and repair to maintenance capacity
	for (int32 ResourceIndex = 0; ResourceIndex < Game->GetResourceCatalog()->MaintenanceResources.Num(); ResourceIndex++)
	{
		FFlareResourceDescription* Resource = &Game->GetResourceCatalog()->MaintenanceResources[ResourceIndex]->Data;
		struct ResourceVariation* Variation = &SectorVariation.ResourceVariations[Resource];

		for (int CompanyIndex = 0; CompanyIndex < Game->GetGameWorld()->GetCompanies().Num(); CompanyIndex++)
		{
			UFlareCompany* OtherCompany = Game->GetGameWorld()->GetCompanies()[CompanyIndex];

			if (OtherCompany->GetWarState(Company) == EFlareHostility::Hostile)
			{
				continue;
			}

			if(Company->IsPlayerCompany() && OtherCompany->IsPlayerCompany())
			{
				// Player don't always want to repair
				continue;
			}

			int32 NeededFS;
			int32 TotalNeededFS;
			int64 MaxDuration;
			int32 NeededFSSum = 0;

			SectorHelper::GetRefillFleetSupplyNeeds(Sector, OtherCompany, NeededFS, TotalNeededFS, MaxDuration, true);
			NeededFSSum += TotalNeededFS;

			SectorHelper::GetRepairFleetSupplyNeeds(Sector, OtherCompany, NeededFS, TotalNeededFS, MaxDuration, true);
			NeededFSSum += TotalNeededFS;

			if(OtherCompany == Company)
			{
				Variation->MaintenanceCapacity += NeededFSSum;
			}
			else
			{
				FFlareResourceDescription* FleetSupply = Sector->GetGame()->GetScenarioTools()->FleetSupply;
				int32 CanBuyQuantity =  (int32) (OtherCompany->GetMoney() / Sector->GetResourcePrice(FleetSupply, EFlareResourcePriceContext::FactoryInput));
				CanBuyQuantity = FMath::Max(CanBuyQuantity, 0);

				Variation->MaintenanceCapacity += FMath::Min(CanBuyQuantity, NeededFSSum);
			}
		}

		// Remove available FS
		int32 AvailableFS;
		int32 OwnedFS;
		int32 AffordableFS;
		// Use own company to have a pessimitic estimation
		SectorHelper::GetAvailableFleetSupplyCount(Sector, Company, OwnedFS, AvailableFS, AffordableFS);
		Variation->MaintenanceCapacity -= AvailableFS;
	}

	return SectorVariation;
}

#define TRADING_MIN_NEED_QUANTITY 50

inline static int32 NeedComparatorComparator(const AITradeNeed& n1, const AITradeNeed& n2)
{
	if(n1.HighPriority == n2.HighPriority)
	{
		return n1.Ratio > n2.Ratio;
	}
	else
		return n1.HighPriority > n2.HighPriority;
}

// New trading
void AITradeHelper::GenerateTradingNeeds(AITradeNeeds& Needs, AITradeNeeds& MaintenanceNeeds, AITradeNeeds& StorageNeeds, UFlareWorld* World)
{
	// Trading needs
	UFlareScenarioTools* ST = World->GetGame()->GetScenarioTools();

	int32 GameDifficulty = -1;
	GameDifficulty = World->GetGame()->GetPC()->GetPlayerData()->DifficultyId;
	
	int32 PlayerPriority = 0;
	bool PlayerallowedpriorityConstruction = true;
	if (GameDifficulty >= 1) // Hard
	{
		PlayerallowedpriorityConstruction = false;
	}
	if (GameDifficulty >= 2)  // Very Hard
	{
		PlayerPriority = -1;
	}

	for(UFlareCompany* Company : World->GetCompanies())
	{
		for(UFlareSimulatedSpacecraft* Station : Company->GetCompanyStations())
		{
			bool IsConstruction = Station->IsUnderConstruction();
			bool IsShipyard = Station->IsShipyard();

			for(UFlareResourceCatalogEntry* ResourceEntry : World->GetGame()->GetResourceCatalog()->Resources)
			{
				FFlareResourceDescription* Resource = &ResourceEntry->Data;

				if(Station->GetActiveCargoBay()->WantBuy(Resource, nullptr))
				{
					int32 Quantity = Station->GetActiveCargoBay()->GetFreeSpaceForResource(Resource, nullptr);

					if (!IsConstruction && Station->IsComplex())
					{
						if(Station->GetActiveCargoBay()->WantBuy(Resource, Company) && Station->GetActiveCargoBay()->WantSell(Resource, Company))
						{
							int32 TotalCapacity = Station->GetActiveCargoBay()->GetTotalCapacityForResource(Resource, Company);
							Quantity = FMath::Max(0, Quantity - TotalCapacity / 2);
						}
					}

					if(Quantity > TRADING_MIN_NEED_QUANTITY || (IsConstruction && Quantity > 0) || (IsShipyard && Quantity > 0))
					{
						AITradeNeed Need;
						Need.Resource = Resource;
						Need.Quantity = Quantity;
						Need.TotalCapacity = Station->GetActiveCargoBay()->GetTotalCapacityForResource(Resource, nullptr);
						Need.Company = Station->GetCompany();
						Need.Sector = Station->GetCurrentSector();
						Need.Station = Station;
						Need.SourceFunctionIndex = 0;
						Need.Maintenance = false;
						Need.Consume(0); // Generate ratio

						if (Company == World->GetGame()->GetPC()->GetCompany())
						{
							if (PlayerallowedpriorityConstruction && (IsConstruction || IsShipyard))
							{
								if (IsConstruction)
								{
									Need.HighPriority = 1;
								}
								else if (IsShipyard && Station->IsAllowExternalOrder())
								{
									Need.HighPriority = 2;
								}
								else
								{
									Need.HighPriority = PlayerPriority;
								}
							}
							else
							{
								Need.HighPriority = PlayerPriority;
							}
						}
						else
						{
							if (IsConstruction)
							{
								Need.HighPriority = 2;
							}
							else
							{
								if (IsShipyard)
								{
									if (Company != ST->Pirates)
									{
										Need.HighPriority = 3;
									}
									else
									{
										Need.HighPriority = 1;
									}
								}
								else
								{
									Need.HighPriority = 0;
								}
							}
						}

						if (Company != World->GetGame()->GetPC()->GetCompany() && !IsConstruction && !IsShipyard)
						{
							for (int32 FactoryIndex = 0; FactoryIndex < Station->GetFactories().Num(); FactoryIndex++)
							{
								UFlareFactory* Factory = Station->GetFactories()[FactoryIndex];
								uint32 InputQuantityCycles = Factory->GetInputResourceQuantityCycles(Resource);
								if (InputQuantityCycles >= 0 && InputQuantityCycles < 7)
								{
									Need.HighPriority++;
									break;
								}
							}
						}

						if(Station->HasCapability(EFlareSpacecraftCapability::Storage) && !IsConstruction)
						{
							StorageNeeds.List.Add(Need);
						}
						else
						{
							Needs.List.Add(Need);
						}
					}
				}
			}
		}
	}

	// Maintenance needs
	for(UFlareSimulatedSector* Sector : World->GetSectors())
	{
		for(UFlareCompany* Company : World->GetCompanies())
		{
			int32 CurrentNeededFleetSupply = 0;
			int32 RepairTotalNeededFleetSupply = 0;
			int32 RefillTotalNeededFleetSupply = 0;
			int64 MaxDuration = 0;

			SectorHelper::GetRepairFleetSupplyNeeds(Sector, Company, CurrentNeededFleetSupply, RepairTotalNeededFleetSupply, MaxDuration, true);
			SectorHelper::GetRefillFleetSupplyNeeds(Sector, Company, CurrentNeededFleetSupply, RefillTotalNeededFleetSupply, MaxDuration, true);

			int32 TotalNeededFleetSupply = RepairTotalNeededFleetSupply + RefillTotalNeededFleetSupply;

			if(TotalNeededFleetSupply > 0)
			{
				AITradeNeed Need;
				Need.Resource = World->GetGame()->GetScenarioTools()->FleetSupply;
				Need.Quantity = TotalNeededFleetSupply;
				Need.TotalCapacity = TotalNeededFleetSupply * (MaxDuration +1);
				Need.Company = Company;
				Need.Sector = Sector;
				Need.Station = nullptr;
				Need.SourceFunctionIndex = 0;
				Need.Maintenance = true;
				Need.HighPriority = true;
				Need.Consume(0); // Generate ratio
				MaintenanceNeeds.List.Add(Need);
			}

		}
	}
#if DEBUG_NEW_AI_TRADING
	Needs.List.Sort(&NeedComparatorComparator);
#endif
}

void AITradeHelper::GenerateTradingSources(AITradeSources& Sources, AITradeSources& MaintenanceSources, UFlareWorld* World)
{
	for(UFlareCompany* Company : World->GetCompanies())
	{
		for(UFlareSimulatedSpacecraft* Station : Company->GetCompanyStations())
		{
			bool Construction = Station->IsUnderConstruction();
			bool Complex = Station->IsComplex();

			for(UFlareResourceCatalogEntry* ResourceEntry : World->GetGame()->GetResourceCatalog()->Resources)
			{
				FFlareResourceDescription* Resource = &ResourceEntry->Data;
				bool MaintenanceSource = false;

				if(Station->GetActiveCargoBay()->WantSell(Resource, nullptr))
				{
					if(Station->GetActiveCargoBay()->WantBuy(Resource, nullptr) && !Station->HasCapability(EFlareSpacecraftCapability::Storage))
					{
//						if(Resource == World->GetGame()->GetScenarioTools()->FleetSupply)
						if(Resource->IsMaintenanceResource)
						{
							MaintenanceSource = true;
						}
						else
						{
							FLOGV("DEBUG : %d", Station->IsComplex());
							check(Station->IsComplex() == true);
							// Trade to not use as source
							//continue;
						}
					}

					int32 Quantity = Station->GetActiveCargoBay()->GetResourceQuantity(Resource, nullptr);

					if (!Construction && Complex)
					{
						if(Station->GetActiveCargoBay()->WantBuy(Resource, Company) && Station->GetActiveCargoBay()->WantSell(Resource, Company))
						{
							int32 TotalCapacity = Station->GetActiveCargoBay()->GetTotalCapacityForResource(Resource, Company);
							Quantity = FMath::Max(0, Quantity - TotalCapacity / 2);
						}
					}

					if(Quantity > 0)
					{
						AITradeSource Source;
						Source.Resource = Resource;
						Source.Quantity = Quantity;
						Source.Company = Station->GetCompany();
						Source.Sector = Station->GetCurrentSector();
						Source.Station = Station;
						Source.Ship = nullptr;
						if(MaintenanceSource)
						{
							MaintenanceSources.Add(Source);
						}
						else
						{
							Sources.Add(Source);
						}
					}
				}
			}
		}
	}

	for(UFlareCompany* Company : World->GetCompanies())
	{
		if(Company->IsPlayerCompany())
		{
			continue;
		}

		for(UFlareSimulatedSpacecraft* Ship : Company->GetCompanyShips())
		{
			if (Ship->IsMilitary())
			{
				if (Company->GetWarCount(Company) > 0)
				{
					continue;
				}
				else if (Ship->GetActiveCargoBay()->GetCapacity() == 0)
				{
					continue;
				}
			}

			if (Ship->GetDescription()->IsDroneCarrier || Ship->GetDescription()->IsDroneShip)
			{
				continue;
			}

			if(Ship->GetDamageSystem()->IsUncontrollable() || Ship->IsTrading())
			{
				continue;
			}

			for(UFlareResourceCatalogEntry* ResourceEntry : World->GetGame()->GetResourceCatalog()->Resources)
			{
				FFlareResourceDescription* Resource = &ResourceEntry->Data;
				int32 Quantity = Ship->GetActiveCargoBay()->GetResourceQuantity(Resource, nullptr);
				if(Quantity > 0)
				{
					bool Traveling = Ship->GetCurrentFleet()->IsTraveling();
					AITradeSource Source;
					Source.Resource = Resource;
					Source.Quantity = Quantity;
					Source.Company = Ship->GetCompany();
					Source.Sector = Traveling ? Ship->GetCurrentFleet()->GetCurrentTravel()->GetDestinationSector() : Ship->GetCurrentSector();
					Source.Station = nullptr;
					Source.Ship = Ship;
					Source.Stranded = Ship->GetDamageSystem()->IsStranded();
					Source.Traveling = Traveling;
					Sources.Add(Source);
				}
			}
		}
	}
	Sources.GenerateCache();
	MaintenanceSources.GenerateCache();
}

void AITradeHelper::GenerateIdleShips(AITradeIdleShips& Ships, UFlareWorld* World)
{
	for(UFlareCompany* Company : World->GetCompanies())
	{
		if(Company->IsPlayerCompany())
		{
			continue;
		}

		for(UFlareSimulatedSpacecraft* Ship : Company->GetCompanyShips())
		{
			if (Ship->IsMilitary())
			{
				if (Company->GetWarCount(Company) > 0)
				{
					continue;
				}
				else if (Ship->GetActiveCargoBay()->GetCapacity() == 0)
				{
					continue;
				}
			}

			if (Ship->GetDescription()->IsDroneCarrier || Ship->GetDescription()->IsDroneShip)
			{
				continue;
			}

			if(Ship->GetDamageSystem()->IsUncontrollable() || Ship->IsTrading())
			{
				continue;
			}

			if(Ship->GetActiveCargoBay()->GetUsedCargoSpace() > 0)
			{
				continue;
			}

			bool Traveling = Ship->GetCurrentFleet()->IsTraveling();

			AIIdleShip IdleShip;
			IdleShip.Company = Ship->GetCompany();
			IdleShip.Capacity = Ship->GetActiveCargoBay()->GetCapacity();
			IdleShip.Ship = Ship;
			IdleShip.Sector = Traveling ? Ship->GetCurrentFleet()->GetCurrentTravel()->GetDestinationSector() : Ship->GetCurrentSector();
			IdleShip.Stranded = Ship->GetDamageSystem()->IsStranded();
			IdleShip.Traveling = Traveling;
			Ships.Add(IdleShip);
		}
	}

	Ships.GenerateCache();


}





#define SourceFunctionCount 18
#define IdleShipFunctionCount 12

void AITradeHelper::ComputeGlobalTrading(UFlareWorld* World, AITradeNeeds& Needs, AITradeSources& Sources, AITradeSources& MaintenanceSources, AITradeIdleShips& IdleShips, AICompaniesMoney& CompaniesMoney)
{
	while(Needs.List.Num() > 0)
	{
		Needs.List.Sort(&NeedComparatorComparator);

		TArray<AITradeNeed> KeepList;

		for(AITradeNeed& Need : Needs.List)
		{
			bool Keep = ProcessNeed(Need, Sources, MaintenanceSources, IdleShips, CompaniesMoney);

			if(Keep)
			{
				KeepList.Add(Need);
			}
		}

		Needs.List = KeepList;
	}
}


bool AITradeHelper::ProcessNeed(AITradeNeed& Need, AITradeSources& Sources, AITradeSources& MaintenanceSources, AITradeIdleShips& IdleShips, AICompaniesMoney& CompaniesMoney)
{
	bool MaintenanceSource = false;
//	AITradeSource* Source = FindBestSource(Sources, Need.Resource, Need.Sector, Need.Company, Need.Quantity, Need.SourceFunctionIndex, CompaniesMoney);
	AITradeSource* Source = FindBestSource(Sources, Need, CompaniesMoney);

	if(Source == nullptr && Need.Maintenance)
	{
//		Source = FindBestSource(MaintenanceSources, Need.Resource, Need.Sector, Need.Company, Need.Quantity, Need.SourceFunctionIndex, CompaniesMoney);
		Source = FindBestSource(MaintenanceSources, Need, CompaniesMoney);
		MaintenanceSource = true;
	}

	if(Source == nullptr)
	{
		//FLOG("No best source");
		// No possible source, don't keep
		Need.SourceFunctionIndex++;

		if(Need.SourceFunctionIndex < SourceFunctionCount)
		{
			return true;
		}
#if DEBUG_NEW_AI_TRADING
		FLOGV("No source found for need : %s %s in %s: %d/%d %s",
			  *Need.Company->GetCompanyName().ToString(),
			  *Need.Station->GetImmatriculation().ToString(),
			  *Need.Sector->GetSectorName().ToString(),
			  Need.Quantity,
			  Need.TotalCapacity,
			  *Need.Resource->Name.ToString()
			  );
#endif

		return false;
	}

	//FLOGV("Best source %p Ship=%p Station=%p", Source, Source->Ship, Source->Station);

	int32 UsedQuantity = 0;

	if(Source->Ship != nullptr)
	{
		//FLOG("Consume source Ship");

		// Source in ship, consume source and process transfer
		if(MaintenanceSource)
		{
			MaintenanceSources.ConsumeSource(Source);
		}
		else
		{
			Sources.ConsumeSource(Source);
		}

		UsedQuantity = FMath::Min(Need.Quantity, Source->Quantity);

		if(Source->Ship->GetCompany() != Need.Company)
		{
			int32 ResourcePrice = Need.Maintenance ? Need.Sector->GetResourcePrice (Need.Resource, EFlareResourcePriceContext::MaintenanceConsumption) : Need.Sector->GetTransfertResourcePrice(Source->Ship, Need.Station, Need.Resource);
			CompaniesMoney.ConsumeMoney(Need.Company, UsedQuantity * ResourcePrice);
		}

#if DEBUG_NEW_AI_TRADING
		FLOGV("Need : %s %s in %s: %d/%d %s",
			  *Need.Company->GetCompanyName().ToString(),
		      Need.Station != nullptr ? *Need.Station->GetImmatriculation().ToString() : *FString("sector maintenance"),
			  *Need.Sector->GetSectorName().ToString(),
			  Need.Quantity,
			  Need.TotalCapacity,
			  *Need.Resource->Name.ToString()
			  );
		FLOGV(" use source %s %s in %s: %d %s (stranded: %d, travelling: %d)",
			  *Source->Company->GetCompanyName().ToString(),
			  *Source->Ship->GetImmatriculation().ToString(),
			  *Source->Sector->GetSectorName().ToString(),
			  Source->Quantity,
			  *Source->Resource->Name.ToString(),
			  Source->Stranded,
			  Source->Traveling
			  );
#endif

		SectorDeal Deal;
		Deal.BuyQuantity = 0;
		Deal.Resource = Source->Resource;
		Deal.SectorA = Need.Sector;
		Deal.SectorB = Need.Sector;
		Deal.BuyStation = nullptr;
		Deal.SellStation = Need.Station;

		ApplyDeal(Source->Ship, Deal, nullptr, true, false);
	}
	else
	{
		AIIdleShip* IdleShip = FindBestShip(IdleShips, *Source, Need);
		//FindBestShip(IdleShips, Source->Sector, Source->Company, Need.Company, Need.Quantity);

//FLOGV("Station IdleShip=%p", IdleShip);
		if(IdleShip == nullptr)
		{
#if DEBUG_NEW_AI_TRADING
			FLOGV("No idle ship found for need : %s %s in %s: %d/%d %s",
				  *Need.Company->GetCompanyName().ToString(),
			      Need.Station != nullptr ? *Need.Station->GetImmatriculation().ToString() : *FString("sector maintenance"),
				  *Need.Sector->GetSectorName().ToString(),
				  Need.Quantity,
				  Need.TotalCapacity,
				  *Need.Resource->Name.ToString()
				  );
 #endif
			// No availble ship, don't keep
			return false;
		}

		UFlareSimulatedSpacecraft* Ship = IdleShip->Ship;

		if(Ship->GetCompany() != Need.Company)
		{
			if(!CompaniesMoney.HasMoney(Need.Company))
			{
#if DEBUG_NEW_AI_TRADING
				FLOGV("No affordable idle ship found for need : %s %s in %s: %d/%d %s",
					  *Need.Company->GetCompanyName().ToString(),
				      Need.Station != nullptr ? *Need.Station->GetImmatriculation().ToString() : *FString("sector maintenance"),
					  *Need.Sector->GetSectorName().ToString(),
					  Need.Quantity,
					  Need.TotalCapacity,
					  *Need.Resource->Name.ToString()
					  );
#endif
				// Not enough money
				return false;
			}
		}


		UsedQuantity = FMath::Min(Need.Quantity, Source->Quantity);
		UsedQuantity = FMath::Min(UsedQuantity, Ship->GetActiveCargoBay()->GetFreeSpaceForResource(Need.Resource, Ship->GetCompany()));

		if(Ship->GetCompany() != Need.Company)
		{
			int32 ResourcePrice = Need.Maintenance ? Need.Sector->GetResourcePrice (Need.Resource, EFlareResourcePriceContext::MaintenanceConsumption) : Need.Sector->GetTransfertResourcePrice(Source->Ship, Need.Station, Need.Resource);
			CompaniesMoney.ConsumeMoney(Need.Company, UsedQuantity * ResourcePrice);
		}

		SectorDeal Deal;
		Deal.Resource = Source->Resource;
		Deal.SectorA = Source->Sector;
		Deal.SectorB = Need.Sector;
		Deal.BuyStation = Source->Station;
		Deal.SellStation = Need.Station;
		Deal.BuyQuantity = UsedQuantity;

#if DEBUG_NEW_AI_TRADING
		FLOGV("Need : %s %s in %s: %d/%d %s",
			  *Need.Company->GetCompanyName().ToString(),
		      Need.Station != nullptr ? *Need.Station->GetImmatriculation().ToString() : *FString("sector maintenance"),
			  *Need.Sector->GetSectorName().ToString(),
			  Need.Quantity,
			  Need.TotalCapacity,
			  *Need.Resource->Name.ToString()
			  );

			FLOGV(" use source %s station %s in %s: %d %s",
				  *Source->Company->GetCompanyName().ToString(),
				  *Source->Station->GetImmatriculation().ToString(),
				  *Source->Sector->GetSectorName().ToString(),
				  Source->Quantity,
				  *Source->Resource->Name.ToString()
				  );
		FLOGV(" with ship : %s %s in %s: %d free space (stranded: %d, travelling: %d)",
				*IdleShip->Company->GetCompanyName().ToString(),
				*IdleShip->Ship->GetImmatriculation().ToString(),
				*IdleShip->Sector->GetSectorName().ToString(),
				IdleShip->Capacity,
				IdleShip->Stranded,
				IdleShip->Traveling
				  );
#endif

		if(!Need.Maintenance || IdleShip->Sector != Need.Sector)
		{
			ApplyDeal(Ship, Deal, nullptr, true, false);
		}


		if(MaintenanceSource)
		{
			MaintenanceSources.ConsumeSource(Source, UsedQuantity);
		}
		else
		{
			Sources.ConsumeSource(Source, UsedQuantity);
		}
		IdleShips.ConsumeShip(IdleShip);
	}

	Need.Consume(UsedQuantity);

	if(Need.Quantity <= 0)
	{
		// Need fulfill
		return false;
	}
	else
	{
		return true;
	}
}

AITradeSource* AITradeHelper::FindBestSource(AITradeSources& Sources, AITradeNeed& Need, AICompaniesMoney& CompaniesMoney)
//AITradeSource* AITradeHelper::FindBestSource(AITradeSources& Sources, FFlareResourceDescription* Resource, UFlareSimulatedSector* Sector, UFlareCompany* Company, int32 NeededQuantity, size_t FunctionIndex, AICompaniesMoney& CompaniesMoney)
{
	static std::function<AITradeSource* (AITradeSourcesByResource&, UFlareSimulatedSector*, UFlareCompany*, int32, UFlareSimulatedSpacecraft*, FFlareResourceDescription*, AICompaniesMoney&)> Functions[SourceFunctionCount];
	static bool FunctionsInit = false;

	//FLOGV("FindBestSource nbSource=%d FunctionIndex=%d", Sources.SourceCount, FunctionIndex);

	if(!FunctionsInit)
	{
		FunctionsInit = true;

		// Priority 1 : cargo source
		//   Priority 2 : owned source
		//     Sub Priority 1 : local cargo
		//     Sub Priority 2 : incoming cargo
		//     Sub Priority 3 : cargo in same moon
		//     Sub Priority 4 : cargo in world
		//   Priority 2 : not owned source


		// Priority 1 : station source
		//   Priority 2 : owned source
		//     Sub Priority 5 : local station
		//     Sub Priority 6 : station in same moon
		//     Sub Priority 7 : station in world



		// Owned local cargo
		int32 InitFunctionIndex = 0;
		Functions[InitFunctionIndex++] = [](AITradeSourcesByResource& iSourcesByResource, UFlareSimulatedSector* iSector, UFlareCompany* iCompany, int32 iNeededQuantity, UFlareSimulatedSpacecraft* iNeedStation, FFlareResourceDescription* iNeedResource, AICompaniesMoney& iCompaniesMoney)
		{
			AITradeSourcesByResourceLocation& SourcesByResourceSector = iSourcesByResource.GetSourcesPerSector(iSector);

			TArray<AITradeSource *>& SourcesByResourceSectorCompany = SourcesByResourceSector.GetSourcePerCompany(iCompany);

			AITradeSource* BestSource = nullptr;
			FText Unused;
			for(AITradeSource* Source : SourcesByResourceSectorCompany)
			{
				if(Source->Ship == nullptr)
				{
					// Not cargo, skip
					continue;
				}

				if(Source->Traveling)
				{
					// Not local
					continue;
				}

				if ((Source->Ship && !Source->Ship->CanTradeWhiteListTo(iNeedStation, Unused, iNeedResource)) || (iNeedStation && !iNeedStation->CanTradeWhiteListFrom(Source->Ship, Unused, iNeedResource)))
				{
					// White list restricted
					continue;
				}

				if(BestSource == nullptr)
				{
					BestSource = Source;
				}
				else
				{
					if(BestSource->Quantity < iNeededQuantity && BestSource->Quantity < Source->Quantity)
					{
						// Closer to the needed quantiy
						BestSource = Source;
					}
					else if(BestSource->Quantity > iNeededQuantity && BestSource->Quantity > Source->Quantity)
					{
						// Closer to the needed quantiy
						BestSource = Source;
					}
				}
			}

			return BestSource;
		};

		// Owned incoming cargo
		Functions[InitFunctionIndex++] = [](AITradeSourcesByResource& iSourcesByResource, UFlareSimulatedSector* iSector, UFlareCompany* iCompany, int32 iNeededQuantity, UFlareSimulatedSpacecraft* iNeedStation, FFlareResourceDescription* iNeedResource, AICompaniesMoney& iCompaniesMoney)
		{
			AITradeSourcesByResourceLocation& SourcesByResourceSector = iSourcesByResource.GetSourcesPerSector(iSector);

			TArray<AITradeSource *>& SourcesByResourceSectorCompany = SourcesByResourceSector.GetSourcePerCompany(iCompany);

			AITradeSource* BestSource = nullptr;
			FText Unused;
			for(AITradeSource* Source : SourcesByResourceSectorCompany)
			{
				if(Source->Ship == nullptr)
				{
					// Not cargo, skip
					continue;
				}

				if(Source->Stranded)
				{
					continue;
				}

				if(!Source->Traveling)
				{
					// Not traveling
					continue;
				}

				if ((Source->Ship && !Source->Ship->CanTradeWhiteListTo(iNeedStation, Unused, iNeedResource)) || (iNeedStation && !iNeedStation->CanTradeWhiteListFrom(Source->Ship, Unused, iNeedResource)))
				{
					// White list restricted
					continue;
				}

				if(BestSource == nullptr)
				{
					BestSource = Source;
				}
				else
				{
					if(BestSource->Quantity < iNeededQuantity && BestSource->Quantity < Source->Quantity)
					{
						// Closer to the needed quantiy
						BestSource = Source;
					}
					else if(BestSource->Quantity > iNeededQuantity && BestSource->Quantity > Source->Quantity)
					{
						// Closer to the needed quantiy
						BestSource = Source;
					}
				}
			}

			return BestSource;
		};

		// Owned cargo in moon
		Functions[InitFunctionIndex++] = [](AITradeSourcesByResource& iSourcesByResource, UFlareSimulatedSector* iSector, UFlareCompany* iCompany, int32 iNeededQuantity, UFlareSimulatedSpacecraft* iNeedStation, FFlareResourceDescription* iNeedResource, AICompaniesMoney& iCompaniesMoney)
		{
			AITradeSourcesByResourceLocation& SourcesByResourceSector = iSourcesByResource.GetSourcesPerMoon(iSector->GetOrbitParameters()->CelestialBodyIdentifier);

			TArray<AITradeSource *>& SourcesByResourceSectorCompany = SourcesByResourceSector.GetSourcePerCompany(iCompany);

			AITradeSource* BestSource = nullptr;
			FText Unused;
			for(AITradeSource* Source : SourcesByResourceSectorCompany)
			{
				if(Source->Ship == nullptr)
				{
					// Not cargo, skip
					continue;
				}

				if(Source->Stranded)
				{
					continue;
				}

				if(Source->Traveling)
				{
					// Not local
					continue;
				}

				if ((Source->Ship && !Source->Ship->CanTradeWhiteListTo(iNeedStation, Unused, iNeedResource)) || (iNeedStation && !iNeedStation->CanTradeWhiteListFrom(Source->Ship, Unused, iNeedResource)))
				{
					// White list restricted
					continue;
				}

				if(BestSource == nullptr)
				{
					BestSource = Source;
				}
				else
				{
					if(BestSource->Quantity < iNeededQuantity && BestSource->Quantity < Source->Quantity)
					{
						// Closer to the needed quantiy
						BestSource = Source;
					}
					else if(BestSource->Quantity > iNeededQuantity && BestSource->Quantity > Source->Quantity)
					{
						// Closer to the needed quantiy
						BestSource = Source;
					}
				}
			}

			return BestSource;
		};

		// Owned incoming in moon
		Functions[InitFunctionIndex++] = [](AITradeSourcesByResource& iSourcesByResource, UFlareSimulatedSector* iSector, UFlareCompany* iCompany, int32 iNeededQuantity, UFlareSimulatedSpacecraft* iNeedStation, FFlareResourceDescription* iNeedResource, AICompaniesMoney& iCompaniesMoney)
		{
			AITradeSourcesByResourceLocation& SourcesByResourceSector = iSourcesByResource.GetSourcesPerMoon(iSector->GetOrbitParameters()->CelestialBodyIdentifier);

			TArray<AITradeSource *>& SourcesByResourceSectorCompany = SourcesByResourceSector.GetSourcePerCompany(iCompany);

			AITradeSource* BestSource = nullptr;
			FText Unused;
			for(AITradeSource* Source : SourcesByResourceSectorCompany)
			{
				if(Source->Ship == nullptr)
				{
					// Not cargo, skip
					continue;
				}

				if(Source->Stranded)
				{
					continue;
				}

				if(!Source->Traveling)
				{
					// Not traveling
					continue;
				}

				if ((Source->Ship && !Source->Ship->CanTradeWhiteListTo(iNeedStation, Unused, iNeedResource)) || (iNeedStation && !iNeedStation->CanTradeWhiteListFrom(Source->Ship, Unused, iNeedResource)))
				{
					// White list restricted
					continue;
				}

				if(BestSource == nullptr)
				{
					BestSource = Source;
				}
				else
				{
					if(BestSource->Quantity < iNeededQuantity && BestSource->Quantity < Source->Quantity)
					{
						// Closer to the needed quantiy
						BestSource = Source;
					}
					else if(BestSource->Quantity > iNeededQuantity && BestSource->Quantity > Source->Quantity)
					{
						// Closer to the needed quantiy
						BestSource = Source;
					}
				}
			}

			return BestSource;
		};

		// Owned cargo in world
		Functions[InitFunctionIndex++] = [](AITradeSourcesByResource& iSourcesByResource, UFlareSimulatedSector* iSector, UFlareCompany* iCompany, int32 iNeededQuantity, UFlareSimulatedSpacecraft* iNeedStation, FFlareResourceDescription* iNeedResource, AICompaniesMoney& iCompaniesMoney)
		{
			TArray<AITradeSource *>& SourcesByResourceCompany = iSourcesByResource.GetSourcePerCompany(iCompany);

			AITradeSource* BestSource = nullptr;
			FText Unused;
			for(AITradeSource* Source : SourcesByResourceCompany)
			{
				if(Source->Ship == nullptr)
				{
					// Not cargo, skip
					continue;
				}

				if(Source->Stranded)
				{
					continue;
				}

				if(Source->Traveling)
				{
					// Not local
					continue;
				}

				if ((Source->Ship && !Source->Ship->CanTradeWhiteListTo(iNeedStation, Unused, iNeedResource)) || (iNeedStation && !iNeedStation->CanTradeWhiteListFrom(Source->Ship, Unused, iNeedResource)))
				{
					// White list restricted
					continue;
				}

				if(BestSource == nullptr)
				{
					BestSource = Source;
				}
				else
				{
					if(BestSource->Quantity < iNeededQuantity && BestSource->Quantity < Source->Quantity)
					{
						// Closer to the needed quantiy
						BestSource = Source;
					}
					else if(BestSource->Quantity > iNeededQuantity && BestSource->Quantity > Source->Quantity)
					{
						// Closer to the needed quantiy
						BestSource = Source;
					}
				}
			}

			return BestSource;
		};

		// Owned incoming in world
		Functions[InitFunctionIndex++] = [](AITradeSourcesByResource& iSourcesByResource, UFlareSimulatedSector* iSector, UFlareCompany* iCompany, int32 iNeededQuantity, UFlareSimulatedSpacecraft* iNeedStation, FFlareResourceDescription* iNeedResource, AICompaniesMoney& iCompaniesMoney)
		{
			TArray<AITradeSource *>& SourcesByResourceCompany = iSourcesByResource.GetSourcePerCompany(iCompany);

			AITradeSource* BestSource = nullptr;
			FText Unused;
			for(AITradeSource* Source : SourcesByResourceCompany)
			{
				if(Source->Ship == nullptr)
				{
					// Not cargo, skip
					continue;
				}

				if(Source->Stranded)
				{
					continue;
				}

				if(!Source->Traveling)
				{
					// Not traveling
					continue;
				}

				if ((Source->Ship && !Source->Ship->CanTradeWhiteListTo(iNeedStation, Unused, iNeedResource)) || (iNeedStation && !iNeedStation->CanTradeWhiteListFrom(Source->Ship, Unused, iNeedResource)))
				{
					// White list restricted
					continue;
				}

				if(BestSource == nullptr)
				{
					BestSource = Source;
				}
				else
				{
					if(BestSource->Quantity < iNeededQuantity && BestSource->Quantity < Source->Quantity)
					{
						// Closer to the needed quantiy
						BestSource = Source;
					}
					else if(BestSource->Quantity > iNeededQuantity && BestSource->Quantity > Source->Quantity)
					{
						// Closer to the needed quantiy
						BestSource = Source;
					}
				}
			}

			return BestSource;
		};



		// Other company sources
		// Local cargo
		Functions[InitFunctionIndex++] = [](AITradeSourcesByResource& iSourcesByResource, UFlareSimulatedSector* iSector, UFlareCompany* iCompany, int32 iNeededQuantity, UFlareSimulatedSpacecraft* iNeedStation, FFlareResourceDescription* iNeedResource, AICompaniesMoney& iCompaniesMoney)
		{
			AITradeSourcesByResourceLocation& SourcesByResourceSector = iSourcesByResource.GetSourcesPerSector(iSector);

			TArray<AITradeSource *>& SourcesByResourceSectorCompany = SourcesByResourceSector.GetSources();

			AITradeSource* BestSource = nullptr;

			if(!iCompaniesMoney.HasMoney(iCompany))
			{
				// No money to buy to other company
				return BestSource;
			}
			FText Unused;
			for(AITradeSource* Source : SourcesByResourceSectorCompany)
			{
				if(Source->Ship == nullptr)
				{
					// Not cargo, skip
					continue;
				}

				if(iCompany->GetWarState(Source->Company) == EFlareHostility::Hostile)
				{
					// Cannot trade itself as enemy of the source
					continue;
				}


				if(Source->Traveling)
				{
					// Not local
					continue;
				}

				if ((Source->Ship && !Source->Ship->CanTradeWhiteListTo(iNeedStation, Unused, iNeedResource)) || (iNeedStation && !iNeedStation->CanTradeWhiteListFrom(Source->Ship, Unused, iNeedResource)))
				{
					// White list restricted
					continue;
				}

				if(BestSource == nullptr)
				{
					BestSource = Source;
				}
				else
				{
					if(BestSource->Quantity < iNeededQuantity && BestSource->Quantity < Source->Quantity)
					{
						// Closer to the needed quantiy
						BestSource = Source;
					}
					else if(BestSource->Quantity > iNeededQuantity && BestSource->Quantity > Source->Quantity)
					{
						// Closer to the needed quantiy
						BestSource = Source;
					}
				}
			}

			return BestSource;
		};

		// Incoming cargo
		Functions[InitFunctionIndex++] = [](AITradeSourcesByResource& iSourcesByResource, UFlareSimulatedSector* iSector, UFlareCompany* iCompany, int32 iNeededQuantity, UFlareSimulatedSpacecraft* iNeedStation, FFlareResourceDescription* iNeedResource, AICompaniesMoney& iCompaniesMoney)
		{
			AITradeSourcesByResourceLocation& SourcesByResourceSector = iSourcesByResource.GetSourcesPerSector(iSector);

			TArray<AITradeSource *>& SourcesByResourceSectorCompany = SourcesByResourceSector.GetSources();

			AITradeSource* BestSource = nullptr;

			if(!iCompaniesMoney.HasMoney(iCompany))
			{
				// No money to buy to other company
				return BestSource;
			}
			FText Unused;
			for(AITradeSource* Source : SourcesByResourceSectorCompany)
			{
				if(Source->Ship == nullptr)
				{
					// Not cargo, skip
					continue;
				}

				if(iCompany->GetWarState(Source->Company) == EFlareHostility::Hostile)
				{
					// Cannot trade itself as enemy of the source
					continue;
				}

				if(Source->Stranded)
				{
					continue;
				}

				if(!Source->Traveling)
				{
					// Not traveling
					continue;
				}

				if ((Source->Ship && !Source->Ship->CanTradeWhiteListTo(iNeedStation, Unused, iNeedResource)) || (iNeedStation && !iNeedStation->CanTradeWhiteListFrom(Source->Ship, Unused, iNeedResource)))
				{
					// White list restricted
					continue;
				}

				if(BestSource == nullptr)
				{
					BestSource = Source;
				}
				else
				{
					if(BestSource->Quantity < iNeededQuantity && BestSource->Quantity < Source->Quantity)
					{
						// Closer to the needed quantiy
						BestSource = Source;
					}
					else if(BestSource->Quantity > iNeededQuantity && BestSource->Quantity > Source->Quantity)
					{
						// Closer to the needed quantiy
						BestSource = Source;
					}
				}
			}

			return BestSource;
		};

		// Cargo in moon
		Functions[InitFunctionIndex++] = [](AITradeSourcesByResource& iSourcesByResource, UFlareSimulatedSector* iSector, UFlareCompany* iCompany, int32 iNeededQuantity, UFlareSimulatedSpacecraft* iNeedStation, FFlareResourceDescription* iNeedResource, AICompaniesMoney& iCompaniesMoney)
		{
			AITradeSourcesByResourceLocation& SourcesByResourceSector = iSourcesByResource.GetSourcesPerMoon(iSector->GetOrbitParameters()->CelestialBodyIdentifier);

			TArray<AITradeSource *>& SourcesByResourceSectorCompany = SourcesByResourceSector.GetSources();

			AITradeSource* BestSource = nullptr;

			if(!iCompaniesMoney.HasMoney(iCompany))
			{
				// No money to buy to other company
				return BestSource;
			}
			FText Unused;
			for(AITradeSource* Source : SourcesByResourceSectorCompany)
			{
				if(Source->Ship == nullptr)
				{
					// Not cargo, skip
					continue;
				}

				if(iCompany->GetWarState(Source->Company) == EFlareHostility::Hostile)
				{
					// Cannot trade itself as enemy of the source
					continue;
				}

				if(Source->Stranded)
				{
					continue;
				}

				if(Source->Traveling)
				{
					// Not local
					continue;
				}

				if ((Source->Ship && !Source->Ship->CanTradeWhiteListTo(iNeedStation, Unused, iNeedResource)) || (iNeedStation && !iNeedStation->CanTradeWhiteListFrom(Source->Ship, Unused, iNeedResource)))
				{
					// White list restricted
					continue;
				}

				if(BestSource == nullptr)
				{
					BestSource = Source;
				}
				else
				{
					if(BestSource->Quantity < iNeededQuantity && BestSource->Quantity < Source->Quantity)
					{
						// Closer to the needed quantiy
						BestSource = Source;
					}
					else if(BestSource->Quantity > iNeededQuantity && BestSource->Quantity > Source->Quantity)
					{
						// Closer to the needed quantiy
						BestSource = Source;
					}
				}
			}

			return BestSource;
		};

		// Incoming in moon
		Functions[InitFunctionIndex++] = [](AITradeSourcesByResource& iSourcesByResource, UFlareSimulatedSector* iSector, UFlareCompany* iCompany, int32 iNeededQuantity, UFlareSimulatedSpacecraft* iNeedStation, FFlareResourceDescription* iNeedResource, AICompaniesMoney& iCompaniesMoney)
		{
			AITradeSourcesByResourceLocation& SourcesByResourceSector = iSourcesByResource.GetSourcesPerMoon(iSector->GetOrbitParameters()->CelestialBodyIdentifier);

			TArray<AITradeSource *>& SourcesByResourceSectorCompany = SourcesByResourceSector.GetSources();

			AITradeSource* BestSource = nullptr;

			if(!iCompaniesMoney.HasMoney(iCompany))
			{
				// No money to buy to other company
				return BestSource;
			}
			FText Unused;
			for(AITradeSource* Source : SourcesByResourceSectorCompany)
			{
				if(Source->Ship == nullptr)
				{
					// Not cargo, skip
					continue;
				}

				if(iCompany->GetWarState(Source->Company) == EFlareHostility::Hostile)
				{
					// Cannot trade itself as enemy of the source
					continue;
				}

				if(Source->Stranded)
				{
					continue;
				}

				if(!Source->Traveling)
				{
					// Not traveling
					continue;
				}

				if ((Source->Ship && !Source->Ship->CanTradeWhiteListTo(iNeedStation, Unused, iNeedResource)) || (iNeedStation && !iNeedStation->CanTradeWhiteListFrom(Source->Ship, Unused, iNeedResource)))
				{
					// White list restricted
					continue;
				}

				if(BestSource == nullptr)
				{
					BestSource = Source;
				}
				else
				{
					if(BestSource->Quantity < iNeededQuantity && BestSource->Quantity < Source->Quantity)
					{
						// Closer to the needed quantiy
						BestSource = Source;
					}
					else if(BestSource->Quantity > iNeededQuantity && BestSource->Quantity > Source->Quantity)
					{
						// Closer to the needed quantiy
						BestSource = Source;
					}
				}
			}

			return BestSource;
		};

		// Cargo in world
		Functions[InitFunctionIndex++] = [](AITradeSourcesByResource& iSourcesByResource, UFlareSimulatedSector* iSector, UFlareCompany* iCompany, int32 iNeededQuantity, UFlareSimulatedSpacecraft* iNeedStation, FFlareResourceDescription* iNeedResource, AICompaniesMoney& iCompaniesMoney)
		{
			TArray<AITradeSource *>& SourcesByResourceCompany = iSourcesByResource.GetSources();

			AITradeSource* BestSource = nullptr;

			if(!iCompaniesMoney.HasMoney(iCompany))
			{
				// No money to buy to other company
				return BestSource;
			}
			FText Unused;
			for(AITradeSource* Source : SourcesByResourceCompany)
			{
				if(Source->Ship == nullptr)
				{
					// Not cargo, skip
					continue;
				}

				if(iCompany->GetWarState(Source->Company) == EFlareHostility::Hostile)
				{
					// Cannot trade itself as enemy of the source
					continue;
				}

				if(Source->Stranded)
				{
					continue;
				}

				if(Source->Traveling)
				{
					// Not local
					continue;
				}

				if ((Source->Ship && !Source->Ship->CanTradeWhiteListTo(iNeedStation, Unused, iNeedResource)) || (iNeedStation && !iNeedStation->CanTradeWhiteListFrom(Source->Ship, Unused, iNeedResource)))
				{
					// White list restricted
					continue;
				}

				if(BestSource == nullptr)
				{
					BestSource = Source;
				}
				else
				{
					if(BestSource->Quantity < iNeededQuantity && BestSource->Quantity < Source->Quantity)
					{
						// Closer to the needed quantiy
						BestSource = Source;
					}
					else if(BestSource->Quantity > iNeededQuantity && BestSource->Quantity > Source->Quantity)
					{
						// Closer to the needed quantiy
						BestSource = Source;
					}
				}
			}

			return BestSource;
		};

		// Incoming in world
		Functions[InitFunctionIndex++] = [](AITradeSourcesByResource& iSourcesByResource, UFlareSimulatedSector* iSector, UFlareCompany* iCompany, int32 iNeededQuantity, UFlareSimulatedSpacecraft* iNeedStation, FFlareResourceDescription* iNeedResource, AICompaniesMoney& iCompaniesMoney)
		{
			TArray<AITradeSource *>& SourcesByResourceCompany = iSourcesByResource.GetSources();

			AITradeSource* BestSource = nullptr;

			if(!iCompaniesMoney.HasMoney(iCompany))
			{
				// No money to buy to other company
				return BestSource;
			}
			FText Unused;
			for(AITradeSource* Source : SourcesByResourceCompany)
			{
				if(Source->Ship == nullptr)
				{
					// Not cargo, skip
					continue;
				}

				if(iCompany->GetWarState(Source->Company) == EFlareHostility::Hostile)
				{
					// Cannot trade itself as enemy of the source
					continue;
				}

				if(Source->Stranded)
				{
					continue;
				}

				if(!Source->Traveling)
				{
					// Not traveling
					continue;
				}

				if ((Source->Ship && !Source->Ship->CanTradeWhiteListTo(iNeedStation, Unused, iNeedResource)) || (iNeedStation && !iNeedStation->CanTradeWhiteListFrom(Source->Ship, Unused, iNeedResource)))
				{
					// White list restricted
					continue;
				}

				if(BestSource == nullptr)
				{
					BestSource = Source;
				}
				else
				{
					if(BestSource->Quantity < iNeededQuantity && BestSource->Quantity < Source->Quantity)
					{
						// Closer to the needed quantiy
						BestSource = Source;
					}
					else if(BestSource->Quantity > iNeededQuantity && BestSource->Quantity > Source->Quantity)
					{
						// Closer to the needed quantiy
						BestSource = Source;
					}
				}
			}

			return BestSource;
		};


		// Owned local station
		Functions[InitFunctionIndex++] = [](AITradeSourcesByResource& iSourcesByResource, UFlareSimulatedSector* iSector, UFlareCompany* iCompany, int32 iNeededQuantity, UFlareSimulatedSpacecraft* iNeedStation, FFlareResourceDescription* iNeedResource, AICompaniesMoney& iCompaniesMoney)
		{
			AITradeSourcesByResourceLocation& SourcesByResourceSector = iSourcesByResource.GetSourcesPerSector(iSector);

			TArray<AITradeSource *>& SourcesByResourceSectorCompany = SourcesByResourceSector.GetSourcePerCompany(iCompany);

			AITradeSource* BestSource = nullptr;
			FText Unused;
			for(AITradeSource* Source : SourcesByResourceSectorCompany)
			{
				if(Source->Station == nullptr)
				{
					// Not station, skip
					continue;
				}

				if ((!Source->Station->CanTradeWhiteListTo(iNeedStation, Unused, iNeedResource)) || (iNeedStation && !iNeedStation->CanTradeWhiteListFrom(Source->Station, Unused, iNeedResource)))
				{
					// White list restricted
					continue;
				}

				if(BestSource == nullptr)
				{
					BestSource = Source;
				}
				else
				{
					if(BestSource->Quantity < iNeededQuantity && BestSource->Quantity < Source->Quantity)
					{
						// Closer to the needed quantiy
						BestSource = Source;
					}
					else if(BestSource->Quantity > iNeededQuantity && BestSource->Quantity > Source->Quantity)
					{
						// Closer to the needed quantiy
						BestSource = Source;
					}
				}
			}

			return BestSource;
		};

		// Owned moon station
		Functions[InitFunctionIndex++] = [](AITradeSourcesByResource& iSourcesByResource, UFlareSimulatedSector* iSector, UFlareCompany* iCompany, int32 iNeededQuantity, UFlareSimulatedSpacecraft* iNeedStation, FFlareResourceDescription* iNeedResource, AICompaniesMoney& iCompaniesMoney)
		{
			AITradeSourcesByResourceLocation& SourcesByResourceSector = iSourcesByResource.GetSourcesPerMoon(iSector->GetOrbitParameters()->CelestialBodyIdentifier);

			TArray<AITradeSource *>& SourcesByResourceSectorCompany = SourcesByResourceSector.GetSourcePerCompany(iCompany);

			AITradeSource* BestSource = nullptr;
			FText Unused;
			for(AITradeSource* Source : SourcesByResourceSectorCompany)
			{
				if(Source->Station == nullptr)
				{
					// Not station, skip
					continue;
				}

				if ((!Source->Station->CanTradeWhiteListTo(iNeedStation, Unused, iNeedResource)) || (iNeedStation && !iNeedStation->CanTradeWhiteListFrom(Source->Station, Unused, iNeedResource)))
				{
					// White list restricted
					continue;
				}

				if(BestSource == nullptr)
				{
					BestSource = Source;
				}
				else
				{
					if(BestSource->Quantity < iNeededQuantity && BestSource->Quantity < Source->Quantity)
					{
						// Closer to the needed quantiy
						BestSource = Source;
					}
					else if(BestSource->Quantity > iNeededQuantity && BestSource->Quantity > Source->Quantity)
					{
						// Closer to the needed quantiy
						BestSource = Source;
					}
				}
			}

			return BestSource;
		};

		// Owned world station
		Functions[InitFunctionIndex++] = [](AITradeSourcesByResource& iSourcesByResource, UFlareSimulatedSector* iSector, UFlareCompany* iCompany, int32 iNeededQuantity, UFlareSimulatedSpacecraft* iNeedStation, FFlareResourceDescription* iNeedResource, AICompaniesMoney& iCompaniesMoney)
		{
			TArray<AITradeSource *>& SourcesByResourceSectorCompany = iSourcesByResource.GetSourcePerCompany(iCompany);

			AITradeSource* BestSource = nullptr;
			FText Unused;
			for(AITradeSource* Source : SourcesByResourceSectorCompany)
			{
				if(Source->Station == nullptr)
				{
					// Not station, skip
					continue;
				}

				if ((!Source->Station->CanTradeWhiteListTo(iNeedStation, Unused, iNeedResource)) || (iNeedStation && !iNeedStation->CanTradeWhiteListFrom(Source->Station, Unused, iNeedResource)))
				{
					// White list restricted
					continue;
				}

				if(BestSource == nullptr)
				{
					BestSource = Source;
				}
				else
				{
					if(BestSource->Quantity < iNeededQuantity && BestSource->Quantity < Source->Quantity)
					{
						// Closer to the needed quantiy
						BestSource = Source;
					}
					else if(BestSource->Quantity > iNeededQuantity && BestSource->Quantity > Source->Quantity)
					{
						// Closer to the needed quantiy
						BestSource = Source;
					}
				}
			}

			return BestSource;
		};

		// Local station
		Functions[InitFunctionIndex++] = [](AITradeSourcesByResource& iSourcesByResource, UFlareSimulatedSector* iSector, UFlareCompany* iCompany, int32 iNeededQuantity, UFlareSimulatedSpacecraft* iNeedStation, FFlareResourceDescription* iNeedResource, AICompaniesMoney& iCompaniesMoney)
		{
			AITradeSourcesByResourceLocation& SourcesByResourceSector = iSourcesByResource.GetSourcesPerSector(iSector);

			TArray<AITradeSource *>& SourcesByResourceSectorCompany = SourcesByResourceSector.GetSources();

			AITradeSource* BestSource = nullptr;
			FText Unused;
			for(AITradeSource* Source : SourcesByResourceSectorCompany)
			{
				if(Source->Station == nullptr)
				{
					// Not station, skip
					continue;
				}

				if ((!Source->Station->CanTradeWhiteListTo(iNeedStation, Unused, iNeedResource)) || (iNeedStation && !iNeedStation->CanTradeWhiteListFrom(Source->Station, Unused, iNeedResource)))
				{
					// White list restricted
					continue;
				}

				if(BestSource == nullptr)
				{
					BestSource = Source;
				}
				else
				{
					if(BestSource->Quantity < iNeededQuantity && BestSource->Quantity < Source->Quantity)
					{
						// Closer to the needed quantiy
						BestSource = Source;
					}
					else if(BestSource->Quantity > iNeededQuantity && BestSource->Quantity > Source->Quantity)
					{
						// Closer to the needed quantiy
						BestSource = Source;
					}
				}
			}

			return BestSource;
		};

		// Moon station
		Functions[InitFunctionIndex++] = [](AITradeSourcesByResource& iSourcesByResource, UFlareSimulatedSector* iSector, UFlareCompany* iCompany, int32 iNeededQuantity, UFlareSimulatedSpacecraft* iNeedStation, FFlareResourceDescription* iNeedResource, AICompaniesMoney& iCompaniesMoney)
		{
			AITradeSourcesByResourceLocation& SourcesByResourceSector = iSourcesByResource.GetSourcesPerMoon(iSector->GetOrbitParameters()->CelestialBodyIdentifier);

			TArray<AITradeSource *>& SourcesByResourceSectorCompany = SourcesByResourceSector.GetSources();

			AITradeSource* BestSource = nullptr;
			FText Unused;
			for(AITradeSource* Source : SourcesByResourceSectorCompany)
			{
				if(Source->Station == nullptr)
				{
					// Not station, skip
					continue;
				}

				if ((!Source->Station->CanTradeWhiteListTo(iNeedStation, Unused, iNeedResource)) || (iNeedStation && !iNeedStation->CanTradeWhiteListFrom(Source->Station, Unused, iNeedResource)))
				{
					// White list restricted
					continue;
				}

				if(BestSource == nullptr)
				{
					BestSource = Source;
				}
				else
				{
					if(BestSource->Quantity < iNeededQuantity && BestSource->Quantity < Source->Quantity)
					{
						// Closer to the needed quantiy
						BestSource = Source;
					}
					else if(BestSource->Quantity > iNeededQuantity && BestSource->Quantity > Source->Quantity)
					{
						// Closer to the needed quantiy
						BestSource = Source;
					}
				}
			}

			return BestSource;
		};

		// World station
		Functions[InitFunctionIndex++] = [](AITradeSourcesByResource& iSourcesByResource, UFlareSimulatedSector* iSector, UFlareCompany* iCompany, int32 iNeededQuantity, UFlareSimulatedSpacecraft* iNeedStation, FFlareResourceDescription* iNeedResource, AICompaniesMoney& iCompaniesMoney)
		{
			TArray<AITradeSource *>& SourcesByResourceSectorCompany = iSourcesByResource.GetSources();

			AITradeSource* BestSource = nullptr;
			FText Unused;
			for(AITradeSource* Source : SourcesByResourceSectorCompany)
			{
				if(Source->Station == nullptr)
				{
					// Not station, skip
					continue;
				}

				if ((!Source->Station->CanTradeWhiteListTo(iNeedStation, Unused, iNeedResource)) || (iNeedStation && !iNeedStation->CanTradeWhiteListFrom(Source->Station, Unused, iNeedResource)))
				{
					// White list restricted
					continue;
				}

				if(BestSource == nullptr)
				{
					BestSource = Source;
				}
				else
				{
					if(BestSource->Quantity < iNeededQuantity && BestSource->Quantity < Source->Quantity)
					{
						// Closer to the needed quantiy
						BestSource = Source;
					}
					else if(BestSource->Quantity > iNeededQuantity && BestSource->Quantity > Source->Quantity)
					{
						// Closer to the needed quantiy
						BestSource = Source;
					}
				}
			}

			return BestSource;
		};

		FCHECK(SourceFunctionCount == InitFunctionIndex);
	}

	AITradeSourcesByResource& SourcesByResource = Sources.GetSourcesPerResource(Need.Resource);
	return Functions[Need.SourceFunctionIndex](SourcesByResource, Need.Sector, Need.Company, Need.Quantity, Need.Station, Need.Resource, CompaniesMoney);
}

AIIdleShip* AITradeHelper::FindBestShip(AITradeIdleShips& IdleShips, AITradeSource& Source, AITradeNeed& Need)
//FindBestShip(AITradeIdleShips& IdleShips, UFlareSimulatedSector* Sector, UFlareCompany* SourceCompany, UFlareCompany* NeedCompany, int32 NeedQuantity)
{
	static std::function<AIIdleShip* (AITradeIdleShips&, UFlareSimulatedSector*, UFlareCompany*, UFlareCompany*, UFlareSimulatedSpacecraft*, FFlareResourceDescription*, int32)> Functions[IdleShipFunctionCount];

	static bool FunctionsInit = false;

	if(!FunctionsInit)
	{
		FunctionsInit = true;

		// Priority 1 : owned ships
		//   Sub Priority 1 : local cargo
		//   Sub Priority 2 : incoming cargo
		//   Sub Priority 3 : cargo in same moon
		//   Sub Priority 4 : cargo in world

		// Priority 2 : others ships
		// Sub Priority ...

		// Owned local ship
		int32 FunctionIndex = 0;
		Functions[FunctionIndex++] = [](AITradeIdleShips& iIdleShips, UFlareSimulatedSector* iSector, UFlareCompany* iSourceCompany, UFlareCompany* iNeedCompany, UFlareSimulatedSpacecraft* iNeedStation, FFlareResourceDescription* iNeedResource, int32 iNeededQuantity) ->AIIdleShip*
		{
			if(iNeedCompany->GetWarState(iSourceCompany) == EFlareHostility::Hostile)
			{
				// Cannot trade itself as enemy of the source
				return nullptr;
			}

			AITradeIdleShipsByLocation& IdleShipsBySector = iIdleShips.GetShipsPerSector(iSector);
			TArray<AIIdleShip*>& IdleShipsBySectorCompany = IdleShipsBySector.GetShipsPerCompany(iNeedCompany);

			AIIdleShip* BestShip = nullptr;
			FText Unused;
			for(AIIdleShip* IdleShip : IdleShipsBySectorCompany)
			{
				if(IdleShip->Traveling)
				{
					continue;
				}

				if ((iNeedStation && !iNeedStation->CanTradeWhiteListTo(IdleShip->Ship, Unused, iNeedResource)) || (IdleShip->Ship && !IdleShip->Ship->CanTradeWhiteListFrom(iNeedStation, Unused, iNeedResource)))
				{
					// White list restricted
					continue;
				}

				if(BestShip == nullptr)
				{
					BestShip = IdleShip;
				}
				else
				{
					if(IdleShip->Capacity < iNeededQuantity && BestShip->Capacity < IdleShip->Capacity)
					{
						// Closer to the needed quantiy but lower
						BestShip = IdleShip;
					}
					else if(IdleShip->Capacity > iNeededQuantity && BestShip->Capacity > IdleShip->Capacity)
					{
						// Closer to the needed quantiy but higher
						BestShip = IdleShip;
					}
				}
			}

			return BestShip;
		};

		// Owned incoming ship
		Functions[FunctionIndex++] = [](AITradeIdleShips& iIdleShips, UFlareSimulatedSector* iSector, UFlareCompany* iSourceCompany, UFlareCompany* iNeedCompany, UFlareSimulatedSpacecraft* iNeedStation, FFlareResourceDescription* iNeedResource, int32 iNeededQuantity) ->AIIdleShip*
		{
			if(iNeedCompany->GetWarState(iSourceCompany) == EFlareHostility::Hostile)
			{
				// Cannot trade itself as ennemy of the source
				return nullptr;
			}

			AITradeIdleShipsByLocation& IdleShipsBySector = iIdleShips.GetShipsPerSector(iSector);

			TArray<AIIdleShip*>& IdleShipsBySectorCompany = IdleShipsBySector.GetShipsPerCompany(iNeedCompany);


			AIIdleShip* BestShip = nullptr;
			FText Unused;
			for(AIIdleShip* IdleShip : IdleShipsBySectorCompany)
			{
				if(!IdleShip->Traveling)
				{
					continue;
				}

				if(IdleShip->Stranded)
				{
					continue;
				}

				if ((iNeedStation && !iNeedStation->CanTradeWhiteListTo(IdleShip->Ship, Unused, iNeedResource)) || (IdleShip->Ship && !IdleShip->Ship->CanTradeWhiteListFrom(iNeedStation, Unused, iNeedResource)))
				{
					// White list restricted
					continue;
				}

				if(BestShip == nullptr)
				{
					BestShip = IdleShip;
				}
				else
				{
					if(IdleShip->Capacity < iNeededQuantity && BestShip->Capacity < IdleShip->Capacity)
					{
						// Closer to the needed quantiy but lower
						BestShip = IdleShip;
					}
					else if(IdleShip->Capacity > iNeededQuantity && BestShip->Capacity > IdleShip->Capacity)
					{
						// Closer to the needed quantiy but higher
						BestShip = IdleShip;
					}
				}
			}
			return BestShip;
		};

		// Owned ship in same moon
		Functions[FunctionIndex++] = [](AITradeIdleShips& iIdleShips, UFlareSimulatedSector* iSector, UFlareCompany* iSourceCompany, UFlareCompany* iNeedCompany, UFlareSimulatedSpacecraft* iNeedStation, FFlareResourceDescription* iNeedResource, int32 iNeededQuantity) ->AIIdleShip*
		{
			if(iNeedCompany->GetWarState(iSourceCompany) == EFlareHostility::Hostile)
			{
				// Cannot trade itself as ennemy of the source
				return nullptr;
			}

			AITradeIdleShipsByLocation& IdleShipsByMoon = iIdleShips.GetShipsPerMoon(iSector->GetOrbitParameters()->CelestialBodyIdentifier);

			TArray<AIIdleShip*>& IdleShipsBySectorCompany = IdleShipsByMoon.GetShipsPerCompany(iNeedCompany);


			AIIdleShip* BestShip = nullptr;
			FText Unused;
			for(AIIdleShip* IdleShip : IdleShipsBySectorCompany)
			{
				if(IdleShip->Traveling)
				{
					continue;
				}

				if(IdleShip->Stranded)
				{
					continue;
				}

				if ((iNeedStation && !iNeedStation->CanTradeWhiteListTo(IdleShip->Ship, Unused, iNeedResource)) || (IdleShip->Ship && !IdleShip->Ship->CanTradeWhiteListFrom(iNeedStation, Unused, iNeedResource)))
				{
					// White list restricted
					continue;
				}

				if(BestShip == nullptr)
				{
					BestShip = IdleShip;
				}
				else
				{
					if(IdleShip->Capacity < iNeededQuantity && BestShip->Capacity < IdleShip->Capacity)
					{
						// Closer to the needed quantiy but lower
						BestShip = IdleShip;
					}
					else if(IdleShip->Capacity > iNeededQuantity && BestShip->Capacity > IdleShip->Capacity)
					{
						// Closer to the needed quantiy but higher
						BestShip = IdleShip;
					}
				}
			}

			return BestShip;
		};

		// Owned travelling ship in same moon
		Functions[FunctionIndex++] = [](AITradeIdleShips& iIdleShips, UFlareSimulatedSector* iSector, UFlareCompany* iSourceCompany, UFlareCompany* iNeedCompany, UFlareSimulatedSpacecraft* iNeedStation, FFlareResourceDescription* iNeedResource, int32 iNeededQuantity) ->AIIdleShip*
		{
			if(iNeedCompany->GetWarState(iSourceCompany) == EFlareHostility::Hostile)
			{
				// Cannot trade itself as ennemy of the source
				return nullptr;
			}

			AITradeIdleShipsByLocation& IdleShipsByMoon = iIdleShips.GetShipsPerSector(iSector);

			TArray<AIIdleShip*>& IdleShipsBySectorCompany = IdleShipsByMoon.GetShipsPerCompany(iNeedCompany);


			AIIdleShip* BestShip = nullptr;
			FText Unused;
			for(AIIdleShip* IdleShip : IdleShipsBySectorCompany)
			{
				if(!IdleShip->Traveling)
				{
					continue;
				}

				if(IdleShip->Stranded)
				{
					continue;
				}

				if ((iNeedStation && !iNeedStation->CanTradeWhiteListTo(IdleShip->Ship, Unused, iNeedResource)) || (IdleShip->Ship && !IdleShip->Ship->CanTradeWhiteListFrom(iNeedStation, Unused, iNeedResource)))
				{
					// White list restricted
					continue;
				}

				if(BestShip == nullptr)
				{
					BestShip = IdleShip;
				}
				else
				{
					if(IdleShip->Capacity < iNeededQuantity && BestShip->Capacity < IdleShip->Capacity)
					{
						// Closer to the needed quantiy but lower
						BestShip = IdleShip;
					}
					else if(IdleShip->Capacity > iNeededQuantity && BestShip->Capacity > IdleShip->Capacity)
					{
						// Closer to the needed quantiy but higher
						BestShip = IdleShip;
					}
				}
			}

			return BestShip;
		};

		// Owned ship in world
		Functions[FunctionIndex++] = [](AITradeIdleShips& iIdleShips, UFlareSimulatedSector* iSector, UFlareCompany* iSourceCompany, UFlareCompany* iNeedCompany, UFlareSimulatedSpacecraft* iNeedStation, FFlareResourceDescription* iNeedResource, int32 iNeededQuantity) ->AIIdleShip*
		{
			if(iNeedCompany->GetWarState(iSourceCompany) == EFlareHostility::Hostile)
			{
				// Cannot trade itself as ennemy of the source
				return nullptr;
			}


			TArray<AIIdleShip*>& IdleShipsByCompany = iIdleShips.GetShipsPerCompany(iNeedCompany);


			AIIdleShip* BestShip = nullptr;
			FText Unused;
			for(AIIdleShip* IdleShip : IdleShipsByCompany)
			{
				if(IdleShip->Traveling)
				{
					continue;
				}

				if(IdleShip->Stranded)
				{
					continue;
				}

				if ((iNeedStation && !iNeedStation->CanTradeWhiteListTo(IdleShip->Ship, Unused, iNeedResource)) || (IdleShip->Ship && !IdleShip->Ship->CanTradeWhiteListFrom(iNeedStation, Unused, iNeedResource)))
				{
					// White list restricted
					continue;
				}

				if(BestShip == nullptr)
				{
					BestShip = IdleShip;
				}
				else
				{
					if(IdleShip->Capacity < iNeededQuantity && BestShip->Capacity < IdleShip->Capacity)
					{
						// Closer to the needed quantiy but lower
						BestShip = IdleShip;
					}
					else if(IdleShip->Capacity > iNeededQuantity && BestShip->Capacity > IdleShip->Capacity)
					{
						// Closer to the needed quantiy but higher
						BestShip = IdleShip;
					}
				}
			}

			return BestShip;
		};

		// Owned travelling ship in world
		Functions[FunctionIndex++] = [](AITradeIdleShips& iIdleShips, UFlareSimulatedSector* iSector, UFlareCompany* iSourceCompany, UFlareCompany* iNeedCompany, UFlareSimulatedSpacecraft* iNeedStation, FFlareResourceDescription* iNeedResource, int32 iNeededQuantity) ->AIIdleShip*
		{
			if(iNeedCompany->GetWarState(iSourceCompany) == EFlareHostility::Hostile)
			{
				// Cannot trade itself as ennemy of the source
				return nullptr;
			}


			TArray<AIIdleShip*>& IdleShipsByCompany = iIdleShips.GetShipsPerCompany(iNeedCompany);


			AIIdleShip* BestShip = nullptr;
			FText Unused;
			for(AIIdleShip* IdleShip : IdleShipsByCompany)
			{
				if(!IdleShip->Traveling)
				{
					continue;
				}

				if(IdleShip->Stranded)
				{
					continue;
				}

				if ((iNeedStation && !iNeedStation->CanTradeWhiteListTo(IdleShip->Ship, Unused, iNeedResource)) || (IdleShip->Ship && !IdleShip->Ship->CanTradeWhiteListFrom(iNeedStation, Unused, iNeedResource)))
				{
					// White list restricted
					continue;
				}

				if(BestShip == nullptr)
				{
					BestShip = IdleShip;
				}
				else
				{
					if(IdleShip->Capacity < iNeededQuantity && BestShip->Capacity < IdleShip->Capacity)
					{
						// Closer to the needed quantiy but lower
						BestShip = IdleShip;
					}
					else if(IdleShip->Capacity > iNeededQuantity && BestShip->Capacity > IdleShip->Capacity)
					{
						// Closer to the needed quantiy but higher
						BestShip = IdleShip;
					}
				}
			}

			return BestShip;
		};

		// Others companies

		// Local ship
		Functions[FunctionIndex++] = [](AITradeIdleShips& iIdleShips, UFlareSimulatedSector* iSector, UFlareCompany* iSourceCompany, UFlareCompany* iNeedCompany, UFlareSimulatedSpacecraft* iNeedStation, FFlareResourceDescription* iNeedResource, int32 iNeededQuantity) ->AIIdleShip*
		{
			AITradeIdleShipsByLocation& IdleShipsBySector = iIdleShips.GetShipsPerSector(iSector);

			TArray<AIIdleShip*>& IdleShipsBySectorCompany = IdleShipsBySector.GetShips();


			AIIdleShip* BestShip = nullptr;

			FText Unused;
			for(AIIdleShip* IdleShip : IdleShipsBySectorCompany)
			{
				if(iSourceCompany->GetWarState(IdleShip->Company) == EFlareHostility::Hostile)
				{
					// Cannot trade itself as enemy of the source
					continue;
				}

				if(iNeedCompany->GetWarState(IdleShip->Company) == EFlareHostility::Hostile)
				{
					// Cannot trade itself as enemy of the need
					continue;
				}

				if(IdleShip->Traveling)
				{
					continue;
				}

				if ((iNeedStation && !iNeedStation->CanTradeWhiteListTo(IdleShip->Ship, Unused, iNeedResource)) || (IdleShip->Ship && !IdleShip->Ship->CanTradeWhiteListFrom(iNeedStation, Unused, iNeedResource)))
				{
					// White list restricted
					continue;
				}

				if(BestShip == nullptr)
				{
					BestShip = IdleShip;
				}
				else
				{
					if(IdleShip->Capacity < iNeededQuantity && BestShip->Capacity < IdleShip->Capacity)
					{
						// Closer to the needed quantiy but lower
						BestShip = IdleShip;
					}
					else if(IdleShip->Capacity > iNeededQuantity && BestShip->Capacity > IdleShip->Capacity)
					{
						// Closer to the needed quantiy but higher
						BestShip = IdleShip;
					}
				}
			}

			return BestShip;
		};

		// Incoming ship
		Functions[FunctionIndex++] = [](AITradeIdleShips& iIdleShips, UFlareSimulatedSector* iSector, UFlareCompany* iSourceCompany, UFlareCompany* iNeedCompany, UFlareSimulatedSpacecraft* iNeedStation, FFlareResourceDescription* iNeedResource, int32 iNeededQuantity) ->AIIdleShip*
		{
			AITradeIdleShipsByLocation& IdleShipsBySector = iIdleShips.GetShipsPerSector(iSector);

			TArray<AIIdleShip*>& IdleShipsBySectorCompany = IdleShipsBySector.GetShips();


			AIIdleShip* BestShip = nullptr;
			FText Unused;
			for(AIIdleShip* IdleShip : IdleShipsBySectorCompany)
			{
				if(iSourceCompany->GetWarState(IdleShip->Company) == EFlareHostility::Hostile)
				{
					// Cannot trade itself as ennemy of the source
					continue;
				}

				if(iNeedCompany->GetWarState(IdleShip->Company) == EFlareHostility::Hostile)
				{
					// Cannot trade itself as ennemy of the need
					continue;
				}

				if(IdleShip->Stranded)
				{
					continue;
				}

				if(!IdleShip->Traveling)
				{
					continue;
				}

				if ((iNeedStation && !iNeedStation->CanTradeWhiteListTo(IdleShip->Ship, Unused, iNeedResource)) || (IdleShip->Ship && !IdleShip->Ship->CanTradeWhiteListFrom(iNeedStation, Unused, iNeedResource)))
				{
					// White list restricted
					continue;
				}

				if(BestShip == nullptr)
				{
					BestShip = IdleShip;
				}
				else
				{
					if(IdleShip->Capacity < iNeededQuantity && BestShip->Capacity < IdleShip->Capacity)
					{
						// Closer to the needed quantiy but lower
						BestShip = IdleShip;
					}
					else if(IdleShip->Capacity > iNeededQuantity && BestShip->Capacity > IdleShip->Capacity)
					{
						// Closer to the needed quantiy but higher
						BestShip = IdleShip;
					}
				}
			}

			return BestShip;
		};

		// Ship in same moon
		Functions[FunctionIndex++] = [](AITradeIdleShips& iIdleShips, UFlareSimulatedSector* iSector, UFlareCompany* iSourceCompany, UFlareCompany* iNeedCompany, UFlareSimulatedSpacecraft* iNeedStation, FFlareResourceDescription* iNeedResource, int32 iNeededQuantity) ->AIIdleShip*
		{
			AITradeIdleShipsByLocation& IdleShipsByMoon = iIdleShips.GetShipsPerSector(iSector);

			TArray<AIIdleShip*>& IdleShipsBySectorCompany = IdleShipsByMoon.GetShips();


			AIIdleShip* BestShip = nullptr;

			FText Unused;
			for(AIIdleShip* IdleShip : IdleShipsBySectorCompany)
			{
				if(iSourceCompany->GetWarState(IdleShip->Company) == EFlareHostility::Hostile)
				{
					// Cannot trade itself as enemy of the source
					continue;
				}

				if(iNeedCompany->GetWarState(IdleShip->Company) == EFlareHostility::Hostile)
				{
					// Cannot trade itself as enemy of the need
					continue;
				}

				if(IdleShip->Stranded)
				{
					continue;
				}

				if(IdleShip->Traveling)
				{
					continue;
				}

				if ((iNeedStation && !iNeedStation->CanTradeWhiteListTo(IdleShip->Ship, Unused, iNeedResource)) || (IdleShip->Ship && !IdleShip->Ship->CanTradeWhiteListFrom(iNeedStation, Unused, iNeedResource)))
				{
					// White list restricted
					continue;
				}

				if(BestShip == nullptr)
				{
					BestShip = IdleShip;
				}
				else
				{
					if(IdleShip->Capacity < iNeededQuantity && BestShip->Capacity < IdleShip->Capacity)
					{
						// Closer to the needed quantiy but lower
						BestShip = IdleShip;
					}
					else if(IdleShip->Capacity > iNeededQuantity && BestShip->Capacity > IdleShip->Capacity)
					{
						// Closer to the needed quantiy but higher
						BestShip = IdleShip;
					}
				}
			}

			return BestShip;
		};

		// Travelling ship in same moon
		Functions[FunctionIndex++] = [](AITradeIdleShips& iIdleShips, UFlareSimulatedSector* iSector, UFlareCompany* iSourceCompany, UFlareCompany* iNeedCompany, UFlareSimulatedSpacecraft* iNeedStation, FFlareResourceDescription* iNeedResource, int32 iNeededQuantity) ->AIIdleShip*
		{
			AITradeIdleShipsByLocation& IdleShipsByMoon = iIdleShips.GetShipsPerSector(iSector);

			TArray<AIIdleShip*>& IdleShipsBySectorCompany = IdleShipsByMoon.GetShips();


			AIIdleShip* BestShip = nullptr;
			FText Unused;
			for(AIIdleShip* IdleShip : IdleShipsBySectorCompany)
			{
				if(iSourceCompany->GetWarState(IdleShip->Company) == EFlareHostility::Hostile)
				{
					// Cannot trade itself as ennemy of the source
					continue;
				}

				if(iNeedCompany->GetWarState(IdleShip->Company) == EFlareHostility::Hostile)
				{
					// Cannot trade itself as ennemy of the need
					continue;
				}

				if(IdleShip->Stranded)
				{
					continue;
				}

				if(!IdleShip->Traveling)
				{
					continue;
				}

				if ((iNeedStation && !iNeedStation->CanTradeWhiteListTo(IdleShip->Ship, Unused, iNeedResource)) || (IdleShip->Ship && !IdleShip->Ship->CanTradeWhiteListFrom(iNeedStation, Unused, iNeedResource)))
				{
					// White list restricted
					continue;
				}

				if(BestShip == nullptr)
				{
					BestShip = IdleShip;
				}
				else
				{
					if(IdleShip->Capacity < iNeededQuantity && BestShip->Capacity < IdleShip->Capacity)
					{
						// Closer to the needed quantiy but lower
						BestShip = IdleShip;
					}
					else if(IdleShip->Capacity > iNeededQuantity && BestShip->Capacity > IdleShip->Capacity)
					{
						// Closer to the needed quantiy but higher
						BestShip = IdleShip;
					}
				}
			}

			return BestShip;
		};

		// Ship in world
		Functions[FunctionIndex++] = [](AITradeIdleShips& iIdleShips, UFlareSimulatedSector* iSector, UFlareCompany* iSourceCompany, UFlareCompany* iNeedCompany, UFlareSimulatedSpacecraft* iNeedStation, FFlareResourceDescription* iNeedResource, int32 iNeededQuantity) ->AIIdleShip*
		{
			TArray<AIIdleShip*>& IdleShipsByCompany = iIdleShips.GetShips();


			AIIdleShip* BestShip = nullptr;
			FText Unused;
			for(AIIdleShip* IdleShip : IdleShipsByCompany)
			{
				if(iSourceCompany->GetWarState(IdleShip->Company) == EFlareHostility::Hostile)
				{
					// Cannot trade itself as ennemy of the source
					continue;
				}

				if(iNeedCompany->GetWarState(IdleShip->Company) == EFlareHostility::Hostile)
				{
					// Cannot trade itself as ennemy of the need
					continue;
				}

				if(IdleShip->Stranded)
				{
					continue;
				}

				if(IdleShip->Traveling)
				{
					continue;
				}

				if ((iNeedStation && !iNeedStation->CanTradeWhiteListTo(IdleShip->Ship, Unused, iNeedResource)) || (IdleShip->Ship && !IdleShip->Ship->CanTradeWhiteListFrom(iNeedStation, Unused, iNeedResource)))
				{
					// White list restricted
					continue;
				}

				if(BestShip == nullptr)
				{
					BestShip = IdleShip;
				}
				else
				{
					if(IdleShip->Capacity < iNeededQuantity && BestShip->Capacity < IdleShip->Capacity)
					{
						// Closer to the needed quantiy but lower
						BestShip = IdleShip;
					}
					else if(IdleShip->Capacity > iNeededQuantity && BestShip->Capacity > IdleShip->Capacity)
					{
						// Closer to the needed quantiy but higher
						BestShip = IdleShip;
					}
				}
			}

			return BestShip;
		};

		// Travelling ship in world
		Functions[FunctionIndex++] = [](AITradeIdleShips& iIdleShips, UFlareSimulatedSector* iSector, UFlareCompany* iSourceCompany, UFlareCompany* iNeedCompany, UFlareSimulatedSpacecraft* iNeedStation, FFlareResourceDescription* iNeedResource, int32 iNeededQuantity) ->AIIdleShip*
		{
			TArray<AIIdleShip*>& IdleShipsByCompany = iIdleShips.GetShips();


			AIIdleShip* BestShip = nullptr;
			FText Unused;
			for(AIIdleShip* IdleShip : IdleShipsByCompany)
			{
				if(iSourceCompany->GetWarState(IdleShip->Company) == EFlareHostility::Hostile)
				{
					// Cannot trade itself as enemy of the source
					continue;
				}

				if(iNeedCompany->GetWarState(IdleShip->Company) == EFlareHostility::Hostile)
				{
					// Cannot trade itself as enemy of the need
					continue;
				}

				if(!IdleShip->Traveling)
				{
					continue;
				}

				if ((iNeedStation && !iNeedStation->CanTradeWhiteListTo(IdleShip->Ship, Unused, iNeedResource)) || (IdleShip->Ship && !IdleShip->Ship->CanTradeWhiteListFrom(iNeedStation, Unused, iNeedResource)))
				{
					// White list restricted
					continue;
				}

				if(BestShip == nullptr)
				{
					BestShip = IdleShip;
				}
				else
				{
					if(IdleShip->Capacity < iNeededQuantity && BestShip->Capacity < IdleShip->Capacity)
					{
						// Closer to the needed quantiy but lower
						BestShip = IdleShip;
					}
					else if(IdleShip->Capacity > iNeededQuantity && BestShip->Capacity > IdleShip->Capacity)
					{
						// Closer to the needed quantiy but higher
						BestShip = IdleShip;
					}
				}
			}

			return BestShip;
		};

		FCHECK(IdleShipFunctionCount == FunctionIndex);
	}

	for(auto& Function : Functions)
	{
//		AIIdleShip* Ship = Function(IdleShips, Sector, SourceCompany, NeedCompany, NeedQuantity);
		AIIdleShip* Ship = Function(IdleShips, Source.Sector, Source.Company, Need.Company, Need.Station, Need.Resource, Need.Quantity);
		if(Ship != nullptr)
		{
			return Ship;
		}
	}

	return nullptr;
}

void AITradeNeeds::Print()
{
	FLOGV("AITradeNeeds : %d needs", List.Num())
	for(AITradeNeed& Need: List)
	{
		FLOGV(" - %s %s in %s: %d/%d %s (ratio: %f)",
			  *Need.Company->GetCompanyName().ToString(),
		      Need.Station != nullptr ? *Need.Station->GetImmatriculation().ToString() : *FString("sector maintenance"),
			  *Need.Sector->GetSectorName().ToString(),
			  Need.Quantity,
			  Need.TotalCapacity,
			  *Need.Resource->Name.ToString(),
			  Need.Ratio
			  );
	}
}

AITradeSources::AITradeSources(UFlareWorld* World)
{
	TArray<UFlareResourceCatalogEntry*>& Resources = World->GetGame()->GetResourceCatalog()->Resources;

	for (int32 ResourceIndex = 0; ResourceIndex < Resources.Num(); ResourceIndex++)
	{
		FFlareResourceDescription* Resource = &Resources[ResourceIndex]->Data;
		SourcesPerResource.Add(Resource, AITradeSourcesByResource(World));
	}

	SourceCount = 0;
}

#if DEBUG_NEW_AI_TRADING
void AITradeSources::Print()
{
	FLOGV("AITradeSources : %d sources", SourcesPtr.Num())

	for(AITradeSource* Source: SourcesPtr)
	{
		if(Source->Station)
		{
			FLOGV(" - %s station %s in %s: %d %s",
				  *Source->Company->GetCompanyName().ToString(),
				  *Source->Station->GetImmatriculation().ToString(),
				  *Source->Sector->GetSectorName().ToString(),
				  Source->Quantity,
				  *Source->Resource->Name.ToString()
				  );
		}

		if(Source->Ship)
		{
			FLOGV(" - %s ship %s in %s: %d %s (stranded: %d, travelling: %d)",
				  *Source->Company->GetCompanyName().ToString(),
				  *Source->Ship->GetImmatriculation().ToString(),
				  *Source->Sector->GetSectorName().ToString(),
				  Source->Quantity,
				  *Source->Resource->Name.ToString(),
				  Source->Stranded,
				  Source->Traveling
				  );
		}
	}
}
#endif

void AITradeSources::ConsumeSource(AITradeSource* Source)
{
	for(auto& SourcePerResource : SourcesPerResource)
	{
		SourcePerResource.Value.ConsumeSource(Source);
	}

#if DEBUG_NEW_AI_TRADING
	SourcesPtr.Remove(Source);
#endif

	SourceCount--;
	//FLOGV("AITradeSources::ConsumeSource %p nbSource=%d/%d", Source, SourceCount, Sources.Num());
}

void AITradeSources::ConsumeSource(AITradeSource* Source, int32 Quantity)
{
	Source->Quantity -= Quantity;
	if(Source->Quantity <= 0)
	{
		ConsumeSource(Source);
	}
}

void AITradeSources::Add(AITradeSource const& Source)
{
	Sources.Add(Source);
}

void AITradeSources::GenerateCache()
{
	for(AITradeSource& Source : Sources)
	{
		SourceCount++;
		SourcesPerResource[Source.Resource].Add(&Source);
#if DEBUG_NEW_AI_TRADING
		SourcesPtr.Add(&Source);
#endif
	}
}


AITradeSourcesByResource::AITradeSourcesByResource(UFlareWorld* World)
{
	SourcesPerSector.Reserve(World->GetSectors().Num());
	for(UFlareSimulatedSector* Sector : World->GetSectors())
	{
		SourcesPerSector.Add(Sector, AITradeSourcesByResourceLocation(World));

		FName Moon = Sector->GetOrbitParameters()->CelestialBodyIdentifier;

		if(!SourcesPerMoon.Contains(Moon))
		{
			SourcesPerMoon.Add(Moon, AITradeSourcesByResourceLocation(World));
		}
	}

	for(UFlareCompany* Company : World->GetCompanies())
	{
		SourcesPerCompany.Emplace(Company);
	}
}

AITradeSourcesByResource& AITradeSources::GetSourcesPerResource(FFlareResourceDescription* Resource)
{
	return SourcesPerResource[Resource];
}

void AITradeSourcesByResource::Add(AITradeSource* Source)
{
	SourcesPerSector[Source->Sector].Add(Source);

	FName Moon = Source->Sector->GetOrbitParameters()->CelestialBodyIdentifier;
	SourcesPerMoon[Moon].Add(Source);

	SourcesPerCompany[Source->Company].Add(Source);

	Sources.Add(Source);
}

AITradeSourcesByResourceLocation& AITradeSourcesByResource::GetSourcesPerSector(UFlareSimulatedSector* Sector)
{
	return SourcesPerSector[Sector];
}

AITradeSourcesByResourceLocation& AITradeSourcesByResource::GetSourcesPerMoon(FName Moon)
{
	return SourcesPerMoon[Moon];
}


TArray<AITradeSource*>& AITradeSourcesByResource::GetSourcePerCompany(UFlareCompany* Company)
{
	return SourcesPerCompany[Company];
}

TArray<AITradeSource*>& AITradeSourcesByResource::GetSources()
{
	return Sources;
}

void AITradeSourcesByResource::ConsumeSource(AITradeSource* Source)
{
	for(auto& SourcePerSector : SourcesPerSector)
	{
		SourcePerSector.Value.ConsumeSource(Source);
	}

	for(auto& SourcePerSector : SourcesPerMoon)
	{
		SourcePerSector.Value.ConsumeSource(Source);
	}

	for(auto& SourcePerCompany : SourcesPerCompany)
	{
		SourcePerCompany.Value.Remove(Source);
	}

	Sources.Remove(Source);
}

AITradeSourcesByResourceLocation::AITradeSourcesByResourceLocation(UFlareWorld* World)
{
	for(UFlareCompany* Company : World->GetCompanies())
	{
		SourcesPerCompany.Emplace(Company);
	}
}


void AITradeSourcesByResourceLocation::Add(AITradeSource* Source)
{
	SourcesPerCompany[Source->Company].Add(Source);

	Sources.Add(Source);
}



TArray<AITradeSource*>& AITradeSourcesByResourceLocation::GetSourcePerCompany(UFlareCompany* Company)
{
	return SourcesPerCompany[Company];
}

TArray<AITradeSource*>& AITradeSourcesByResourceLocation::GetSources()
{
	return Sources;
}


void AITradeSourcesByResourceLocation::ConsumeSource(AITradeSource* Source)
{
	for(auto& SourcePerCompany : SourcesPerCompany)
	{
		SourcePerCompany.Value.Remove(Source);
	}

	Sources.Remove(Source);
}

AITradeIdleShips::AITradeIdleShips(UFlareWorld* World)
{
	ShipsPerSector.Reserve(World->GetSectors().Num());
	for(UFlareSimulatedSector* Sector : World->GetSectors())
	{
		ShipsPerSector.Add(Sector, AITradeIdleShipsByLocation(World));

		FName Moon = Sector->GetOrbitParameters()->CelestialBodyIdentifier;

		if(!ShipsPerMoon.Contains(Moon))
		{
			ShipsPerMoon.Add(Moon, AITradeIdleShipsByLocation(World));
		}
	}

	for(UFlareCompany* Company : World->GetCompanies())
	{
		ShipsPerCompany.Emplace(Company);
	}
}

void AITradeIdleShips::Print()
{
	FLOGV("AITradeIdleShips : %d idle ships", ShipsPtr.Num())

	for(AIIdleShip* Ship: ShipsPtr)
	{
		FLOGV(" - %s %s in %s: %d free space (stranded: %d, travelling: %d)",
				*Ship->Company->GetCompanyName().ToString(),
				*Ship->Ship->GetImmatriculation().ToString(),
				*Ship->Sector->GetSectorName().ToString(),
				Ship->Capacity,
				Ship->Stranded,
				Ship->Traveling
				  );
	}
}

void AITradeIdleShips::ConsumeShip(AIIdleShip* Ship)
{
	for(auto& ShipPerSector : ShipsPerSector)
	{
		ShipPerSector.Value.ConsumeShip(Ship);
	}

	for(auto& ShipPerSector : ShipsPerMoon)
	{
		ShipPerSector.Value.ConsumeShip(Ship);
	}

	for(auto& ShipPerCompany : ShipsPerCompany)
	{
		ShipPerCompany.Value.Remove(Ship);
	}

	ShipsPtr.Remove(Ship);
}

void AITradeIdleShips::Add(AIIdleShip const& Ship)
{
	Ships.Add(Ship);
}

void AITradeIdleShips::GenerateCache()
{
	for(AIIdleShip& Ship : Ships)
	{
		ShipsPerSector[Ship.Sector].Add(&Ship);

		FName Moon = Ship.Sector->GetOrbitParameters()->CelestialBodyIdentifier;
		ShipsPerMoon[Moon].Add(&Ship);

		ShipsPerCompany[Ship.Company].Add(&Ship);
		ShipsPtr.Add(&Ship);
	}
}

TArray<AIIdleShip*>& AITradeIdleShips::GetShips()
{
	return ShipsPtr;
}

AITradeIdleShipsByLocation& AITradeIdleShips::GetShipsPerSector(UFlareSimulatedSector* Sector)
{
	return ShipsPerSector[Sector];
}

AITradeIdleShipsByLocation& AITradeIdleShips::GetShipsPerMoon(FName Moon)
{
	return ShipsPerMoon[Moon];
}

TArray<AIIdleShip*>& AITradeIdleShips::GetShipsPerCompany(UFlareCompany* Company)
{
	return ShipsPerCompany[Company];
}

TArray<AIIdleShip*>& AITradeIdleShipsByLocation::GetShipsPerCompany(UFlareCompany* Company)
{
	return ShipsPerCompany[Company];
}

TArray<AIIdleShip*>& AITradeIdleShipsByLocation::GetShips()
{
	return Ships;
}

AITradeIdleShipsByLocation::AITradeIdleShipsByLocation(UFlareWorld* World)
{
	for(UFlareCompany* Company : World->GetCompanies())
	{
		//FLOGV("   - AITradeIdleShipsByLocation emplace %s", *Company->GetIdentifier().ToString());
		ShipsPerCompany.Emplace(Company);
	}
}

void AITradeIdleShipsByLocation::Add(AIIdleShip* Ship)
{
	//FLOGV("   - AITradeIdleShipsByLocation add ship from %s", *Ship->Company->GetIdentifier().ToString());
	ShipsPerCompany[Ship->Company].Add(Ship);

	Ships.Add(Ship);
}

void AITradeIdleShipsByLocation::ConsumeShip(AIIdleShip* Ship)
{
	for(auto& ShipPerCompany : ShipsPerCompany)
	{
		ShipPerCompany.Value.Remove(Ship);
	}

	Ships.Remove(Ship);
}

void AITradeNeed::Consume(int UsedQuantity)
{
	Quantity -= UsedQuantity;
	Ratio = float(Quantity) / float(TotalCapacity);
}

void AICompaniesMoney::ConsumeMoney(UFlareCompany* Company, int64 Amount)
{
#if DEBUG_NEW_AI_TRADING
	int64 OldAmount = CompaniesMoney[Company];
#endif

	CompaniesMoney[Company] -= Amount;
#if DEBUG_NEW_AI_TRADING
	FLOGV("> %s consume %lld (%lld -> %lld)", *Company->GetCompanyName().ToString(), Amount, OldAmount, CompaniesMoney[Company]);
#endif

}

bool AICompaniesMoney::HasMoney(UFlareCompany* Company)
{
	return CompaniesMoney[Company] > 0;
}

#undef LOCTEXT_NAMESPACE
