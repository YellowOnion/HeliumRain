
#include "FlareSimulatedSpacecraft.h"
#include "../Flare.h"

#include "../Game/FlareSimulatedSector.h"
#include "../Game/FlareGame.h"
#include "../Game/FlareFleet.h"
#include "../Game/FlareWorld.h"
#include "../Game/FlareSectorHelper.h"
#include "../Game/FlareGameTools.h"

#include "../Player/FlarePlayerController.h"
#include "../Economy/FlareCargoBay.h"
#include "../Economy/FlareFactory.h"

#include "../Data/FlareResourceCatalog.h"
#include "../Data/FlareFactoryCatalogEntry.h"
#include "../Data/FlareSpacecraftCatalog.h"
#include "../Data/FlareSpacecraftComponentsCatalog.h"

#include "Subsystems/FlareSpacecraftDockingSystem.h"
#include "Subsystems/FlareSpacecraftDamageSystem.h"
#include "Subsystems/FlareSimulatedSpacecraftDamageSystem.h"
#include "Subsystems/FlareSimulatedSpacecraftWeaponsSystem.h"


#define LOCTEXT_NAMESPACE "FlareSimulatedSpacecraft"

#define CAPTURE_RESET_SPEED 0.1f
#define CAPTURE_THRESOLD_MIN 0.05f
#define CAPTURE_THRESOLD_MIN_DAMAGE 0.25f

#define STATION_DAMAGE_THRESOLD 0.75f

/*----------------------------------------------------
	Constructor
----------------------------------------------------*/

UFlareSimulatedSpacecraft::UFlareSimulatedSpacecraft(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActiveSpacecraft = NULL;

}


void UFlareSimulatedSpacecraft::Load(const FFlareSpacecraftSave& Data)
{
	Company = Cast<UFlareCompany>(GetOuter());
	Game = Company->GetGame();
	SpacecraftData = Data;

	ComplexChildren.Empty();

	// Load spacecraft description
	SpacecraftDescription = Game->GetSpacecraftCatalog()->Get(Data.Identifier);

	// Initialize damage system
	if (!DamageSystem)
	{
		DamageSystem = NewObject<UFlareSimulatedSpacecraftDamageSystem>(this, UFlareSimulatedSpacecraftDamageSystem::StaticClass());
	
	}

	DamageSystem->Initialize(this, &SpacecraftData);

	// Initialize weapons system
	if (!WeaponsSystem)
	{
		WeaponsSystem = NewObject<UFlareSimulatedSpacecraftWeaponsSystem>(this, UFlareSimulatedSpacecraftWeaponsSystem::StaticClass());
	}
	WeaponsSystem->Initialize(this, &SpacecraftData);

	// Initialize complex
	if (IsComplex())
	{
		ComplexChildren.Empty();
		ComplexMaster = this;

		for (FFlareConnectionSave connexion : SpacecraftData.ConnectedStations)
		{
			UFlareSimulatedSpacecraft* childStation = Company->FindChildStation(connexion.StationIdentifier);
			if (childStation)
			{
				ComplexChildren.Add(childStation);
				childStation->RegisterComplexMaster(this);
			}
			else
			{
				FLOGV("WARNING: child station %s not found. Corrupted save", *connexion.StationIdentifier.ToString())
			}
		}
	}
	else if (!IsComplexElement())
	{
		ComplexChildren.Empty();
		ComplexMaster = nullptr;
	}


	// Initialize factories
	Game->GetGameWorld()->ClearFactories(this);
	Factories.Empty();

	if (IsComplex() && !Data.IsUnderConstruction)
	{
		if (IsUnderConstruction())
		{
			for (UFlareSimulatedSpacecraft* Child : ComplexChildren)
			{
				if (Child->IsUnderConstruction(true))
				{
					Factories.Append(Child->GetFactories());
				}
			}
		}
		else
		{
			for (UFlareSimulatedSpacecraft* Child : ComplexChildren)
			{
				Factories.Append(Child->GetFactories());
			}
		}
	}
	else
	{
		// Load factories
		if (!Data.IsUnderConstruction)
		{
			Factories.Reserve(SpacecraftDescription->Factories.Num());
			for (int FactoryIndex = 0; FactoryIndex < SpacecraftDescription->Factories.Num(); FactoryIndex++)
			{
				FFlareFactorySave FactoryData;
				FFlareFactoryDescription* FactoryDescription = &SpacecraftDescription->Factories[FactoryIndex]->Data;

				if (FactoryIndex < SpacecraftData.FactoryStates.Num())
				{
					FactoryData = SpacecraftData.FactoryStates[FactoryIndex];
				}
				else
				{
					FactoryData.Active = FactoryDescription->AutoStart;
					FactoryData.CostReserved = 0;
					FactoryData.ProductedDuration = 0;
					FactoryData.InfiniteCycle = true;
					FactoryData.CycleCount = 0;
					FactoryData.TargetShipClass = NAME_None;
					FactoryData.TargetShipCompany = NAME_None;
				}

				UFlareFactory* Factory = NewObject<UFlareFactory>(GetGame()->GetGameWorld(), UFlareFactory::StaticClass());
				Factory->Load(this, FactoryDescription, FactoryData);
				Factories.Add(Factory);
				if (!IsDestroyed())
				{
					Game->GetGameWorld()->AddFactory(Factory);
				}
			}
		}

		// Station is under construction
		else
		{
			UFlareFactory* Factory = NewObject<UFlareFactory>(GetGame()->GetGameWorld(), UFlareFactory::StaticClass());

			FFlareFactoryDescription* FactoryDescription = &Factory->ConstructionFactoryDescription;
			FactoryDescription->AutoStart = true;
			FactoryDescription->Name = LOCTEXT("Constructionfactory", "Station under constuction");
			FactoryDescription->Description = LOCTEXT("ConstructionfactoryDescription", "Bring construction resources");
			FactoryDescription->Identifier = FName(*(FString("construction-") + SpacecraftDescription->Identifier.ToString()));

			FFlareFactoryAction OutputAction;
			OutputAction.Action = EFlareFactoryAction::BuildStation;
			OutputAction.Identifier = "build-station";
			OutputAction.Quantity = 1;
			FactoryDescription->OutputActions.Add(OutputAction);
			FactoryDescription->CycleCost.ProductionCost = 0;
			FactoryDescription->CycleCost.ProductionTime = 0;
			FactoryDescription->CycleCost.InputResources.Append(SpacecraftDescription->CycleCost.InputResources);
			FactoryDescription->VisibleStates = true;

			// Create construction factory
			FFlareFactorySave FactoryData;
			FactoryData.Active = FactoryDescription->AutoStart;
			FactoryData.CostReserved = 0;
			FactoryData.ProductedDuration = 0;
			FactoryData.InfiniteCycle = true;
			FactoryData.CycleCount = 0;
			FactoryData.TargetShipClass = NAME_None;
			FactoryData.TargetShipCompany = NAME_None;

			Factory->Load(this, FactoryDescription, FactoryData);
			Factories.Add(Factory);
			if (!IsDestroyed())
			{
				Game->GetGameWorld()->AddFactory(Factory);
			}
		}
	}


	// Cargo bay init
	if (SpacecraftData.AttachComplexStationName == NAME_None)
	{
		int32 ProductionCargoBaySlotCapacity = 0;
		int32 ProductionCargoBayCount = 0;
		ComputeProductionCargoBaySize(ProductionCargoBaySlotCapacity, ProductionCargoBayCount);

		int32 ConstructionCargoBaySlotCapacity = 0;
		int32 ConstructionCargoBayCount = 0;
		ComputeConstructionCargoBaySize(ConstructionCargoBaySlotCapacity, ConstructionCargoBayCount);

		ProductionCargoBay = NewObject<UFlareCargoBay>(this, UFlareCargoBay::StaticClass());
		ProductionCargoBay->Load(this, SpacecraftData.ProductionCargoBay, ProductionCargoBayCount, ProductionCargoBaySlotCapacity);

		ConstructionCargoBay = NewObject<UFlareCargoBay>(this, UFlareCargoBay::StaticClass());
		ConstructionCargoBay->Load(this, SpacecraftData.ConstructionCargoBay, ConstructionCargoBayCount, ConstructionCargoBaySlotCapacity);

		// Lock resources
		LockResources();
	}
	else
	{
		ProductionCargoBay = nullptr;
		ConstructionCargoBay = nullptr;
	}

	if (!IsComplexElement() && IsUnderConstruction())
	{
		AutoFillConstructionCargoBay();
	}


	// Setup station connectors
	ConnectorSlots.Empty();
	ConnectorSlots.Reserve(SpacecraftDescription->StationConnectorCount);
	for (int32 DockIndex = 0; DockIndex < SpacecraftDescription->StationConnectorCount; DockIndex++)
	{
		// Look for the slot name. Unnamed slots are fine if there is only one (most stations are in that case).
		FName ConnectorName = NAME_None;
		if (DockIndex < SpacecraftDescription->StationConnectorNames.Num())
		{
			ConnectorName = SpacecraftDescription->StationConnectorNames[DockIndex];
		}
		else
		{
			FCHECK(SpacecraftDescription->StationConnectorCount == 1);
		}

		// Setup the data
		FFlareDockingInfo StationConnection;
		StationConnection.Name = ConnectorName;
		StationConnection.Occupied = false;
		StationConnection.Granted = false;
		StationConnection.ConnectedStationName = NAME_None;
		StationConnection.DockId = DockIndex;
		StationConnection.Ship = NULL;
		StationConnection.Station = NULL;
		StationConnection.DockSize = EFlarePartSize::L;
		ConnectorSlots.Add(StationConnection);
	}

	// Load connected stations for use in simulated contexts
	for (const FFlareConnectionSave& ConnectedStation : Data.ConnectedStations)
	{
		auto FindByName = [=](const FFlareDockingInfo& Slot)
		{
			return Slot.Name == ConnectedStation.ConnectorName;
		};

		FFlareDockingInfo* StationConnection = ConnectorSlots.FindByPredicate(FindByName);
		StationConnection->ConnectedStationName = ConnectedStation.StationIdentifier;
		StationConnection->Occupied = false;
		StationConnection->Granted = true;
	}

	if (IsStation() && IsShipyard())
	{
		if(SpacecraftData.ShipyardOrderExternalConfig.Num() <= 0)
			//if num below 0 it's probably from an old savegame
		{
			SpacecraftData.ShipyardOrderExternalConfig.Reserve(Game->GetSpacecraftCatalog()->ShipCatalog.Num());
			for (auto& CatalogEntry : Game->GetSpacecraftCatalog()->ShipCatalog)
			{
				const FFlareSpacecraftDescription* Description = &CatalogEntry->Data;
				if (!Description->IsDroneShip)
				{
					if (Description->BuildableShip.Num())
					{
						if (Description->BuildableShip.Contains(this->GetDescription()->Identifier))
						{
							SpacecraftData.ShipyardOrderExternalConfig.Add(Description->Identifier);
						}
					}
					else
					{
						SpacecraftData.ShipyardOrderExternalConfig.Add(Description->Identifier);
					}
				}
			}
		}
	}
	else if (SpacecraftDescription->IsDroneCarrier)
	{
		if (SpacecraftData.ShipyardOrderExternalConfig.Num() <= 0)
			//if num below 0 it's probably from an old savegame
		{
			SpacecraftData.ShipyardOrderExternalConfig.Reserve(Game->GetSpacecraftCatalog()->ShipCatalog.Num());
			for (auto& CatalogEntry : Game->GetSpacecraftCatalog()->ShipCatalog)
			{
				const FFlareSpacecraftDescription* Description = &CatalogEntry->Data;
				if (Description->IsDroneShip)
				{
					SpacecraftData.ShipyardOrderExternalConfig.Add(Description->Identifier);
				}
			}
		}
	}

	if (SpacecraftData.OwnedShipNames.Num() > 0)
	{
		for (FName OwnedShips : SpacecraftData.OwnedShipNames)
		{
			UFlareSimulatedSpacecraft* ChildShip = Company->FindSpacecraft(OwnedShips);
			if (ChildShip && !ChildShip->IsDestroyed())
			{
				AddShipChildren(ChildShip);
			}
			else
			{
				SpacecraftData.OwnedShipNames.Remove(OwnedShips);
			}
		}
	}

	//adding to "master" shipchildren too, depends on load order. subordinates probably almost always load after the master ship

	if (SpacecraftData.OwnerShipName != NAME_None)
	{
		UFlareSimulatedSpacecraft* OwnerShip = Company->FindSpacecraft(SpacecraftData.OwnerShipName);
		if (OwnerShip)
		{
			OwnerShip->AddShipChildren(this);
		}
	}

	// Load active spacecraft if it exists
	if (ActiveSpacecraft)
	{
		ActiveSpacecraft->Load(this);
		ActiveSpacecraft->Redock();
	}
}

void UFlareSimulatedSpacecraft::Reload()
{
	Save();
	Load(SpacecraftData);
}

FFlareSpacecraftSave* UFlareSimulatedSpacecraft::Save()
{
	SpacecraftData.FactoryStates.Empty();
	SpacecraftData.FactoryStates.Reserve(Factories.Num());
	for (int FactoryIndex = 0; FactoryIndex < Factories.Num(); FactoryIndex++)
	{
		SpacecraftData.FactoryStates.Add(*Factories[FactoryIndex]->Save());
	}

	if(ConstructionCargoBay && ProductionCargoBay)
	{
		SpacecraftData.ConstructionCargoBay = *ConstructionCargoBay->Save();
		SpacecraftData.ProductionCargoBay = *ProductionCargoBay->Save();
	}

	if (IsActive())
	{
		GetActive()->Save();
	}

	// Save connected stations
	GetData().ConnectedStations.Empty();
	GetData().ConnectedStations.Reserve(ConnectorSlots.Num());
	for (FFlareDockingInfo& StationConnection : ConnectorSlots)
	{
		if (StationConnection.Granted)
		{
			FFlareConnectionSave Data;
			Data.ConnectorName = StationConnection.Name;
			Data.StationIdentifier = StationConnection.ConnectedStationName;
			GetData().ConnectedStations.Add(Data);
		}
	}

	return &SpacecraftData;
}

void UFlareSimulatedSpacecraft::SelectWhiteListDefault(FName IdentifierSearch)
{
	UFlareCompanyWhiteList* FoundWhiteList = Company->GetWhiteList(IdentifierSearch);
	SelectWhiteListDefault(FoundWhiteList);
}

void UFlareSimulatedSpacecraft::SelectWhiteListDefault(UFlareCompanyWhiteList* NewWhiteList)
{

	if (NewWhiteList)
	{
		SpacecraftData.DefaultWhiteListIdentifier = NewWhiteList->GetWhiteListIdentifier();

		if (ShipSelectedWhiteList && ShipSelectedWhiteList != NewWhiteList)
		{
			ShipSelectedWhiteList->RemoveShipFromTracker(this);
		}

		ShipSelectedWhiteList = NewWhiteList;
		ShipSelectedWhiteList->AddShipToTracker(this);
	}
	else
	{
		if (ShipSelectedWhiteList)
		{
			ShipSelectedWhiteList->RemoveShipFromTracker(this);
		}

		SpacecraftData.DefaultWhiteListIdentifier = FName();
		ShipSelectedWhiteList = nullptr;
	}
}

UFlareCompanyWhiteList* UFlareSimulatedSpacecraft::GetActiveWhitelist()
{
	if (ShipSelectedWhiteList)
	{
		return ShipSelectedWhiteList;
	}
	if (GetCurrentFleet() && GetCurrentFleet()->GetSelectedWhiteList())
	{
		return GetCurrentFleet()->GetSelectedWhiteList();
	}
	if (GetCompany() && GetCompany()->GetCompanySelectedWhiteList())
	{
		return GetCompany()->GetCompanySelectedWhiteList();
	}

	return nullptr;
}

UFlareCompany* UFlareSimulatedSpacecraft::GetCompany() const
{
	return Company;
}

EFlarePartSize::Type UFlareSimulatedSpacecraft::GetSize() const
{
	return SpacecraftDescription->Size;
}

bool UFlareSimulatedSpacecraft::IsMilitary() const
{
	return SpacecraftDescription->IsMilitary();
}

bool UFlareSimulatedSpacecraft::IsCapableCarrier() const
{
	return SpacecraftDescription->IsDroneCarrier && this->GetShipChildren().Num() > 0;
}

bool UFlareSimulatedSpacecraft::IsStation() const
{
	return SpacecraftDescription->IsStation();
}

bool UFlareSimulatedSpacecraft::IsUncapturable() const
{
	return SpacecraftDescription->IsAnUncapturable();
}

bool UFlareSimulatedSpacecraft::IsImmobileStation() const
{
	return SpacecraftDescription->IsImmobileStation();
}

bool UFlareSimulatedSpacecraft::CanFight() const
{
	return GetDamageSystem()->IsAlive() && IsMilitary() && !GetDamageSystem()->IsDisarmed();
}

bool UFlareSimulatedSpacecraft::CanTravel() const
{
	return !IsTrading() && !IsIntercepted() && GetDamageSystem()->IsAlive() && !GetDamageSystem()->IsStranded();
}

FName UFlareSimulatedSpacecraft::GetImmatriculation() const
{
	return SpacecraftData.Immatriculation;
}

UFlareSimulatedSpacecraftDamageSystem* UFlareSimulatedSpacecraft::GetDamageSystem() const
{
	return DamageSystem;
}

UFlareSimulatedSpacecraftWeaponsSystem* UFlareSimulatedSpacecraft::GetWeaponsSystem() const
{
	return WeaponsSystem;
}

void UFlareSimulatedSpacecraft::SetSpawnMode(EFlareSpawnMode::Type SpawnMode)
{
	SpacecraftData.SpawnMode = SpawnMode;
}

bool UFlareSimulatedSpacecraft::CanBeFlown(FText& OutInfo) const
{
	UFlareFleet* PlayerFleet = GetGame()->GetPC()->GetPlayerFleet();

	if (IsStation())
	{
		return false;
	}
	else if (this == GetGame()->GetPC()->GetPlayerShip())
	{
		OutInfo = LOCTEXT("CantFlySame", "You are already flying this ship");
		return false;
	}
	else if (GetDamageSystem()->IsUncontrollable())
	{
		OutInfo = LOCTEXT("CantFlyDestroyedInfo", "This ship is uncontrollable");
		return false;
	}
	else if (PlayerFleet && GetCurrentFleet() != PlayerFleet)
	{
		OutInfo = LOCTEXT("CantFlyOtherInfo", "You can only switch ships from within the same fleet");
		return false;
	}
	else if (!IsActive())
	{
		OutInfo = LOCTEXT("CantFlyDistantInfo", "Can't fly a ship from another sector");
		return false;
	}
	else
	{
		return true;
	}
}


/*----------------------------------------------------
	Gameplay
----------------------------------------------------*/


void UFlareSimulatedSpacecraft::SetCurrentSector(UFlareSimulatedSector* Sector)
{
	CurrentSector = Sector;

	// Mark the sector as visited
	if (!Sector->IsTravelSector())
	{
		GetCompany()->VisitSector(Sector);
	}

	if(IsComplex())
	{
		for(UFlareSimulatedSpacecraft* Child : GetComplexChildren())
		{
			Child->SetCurrentSector(Sector);
		}
	}
}

void UFlareSimulatedSpacecraft::TryMigrateDrones()
{
	if (CurrentSector)
	{
		for (UFlareSimulatedSpacecraft* SectorShip : CurrentSector->GetSectorShips())
		{
			if (SectorShip == this || !SectorShip->GetDescription()->IsDroneCarrier || SectorShip->GetCompany() != this->GetCompany() || !SectorShip->GetDamageSystem()->IsAlive() || this->ShipMaster == SectorShip)
			{
				continue;
			}

			int32 ActiveShipyards = 0;
			for (UFlareFactory* Shipyard : SectorShip->GetShipyardFactories())
			{
				if (Shipyard->IsProducing())
				{
					ActiveShipyards++;
				}
			}

			if ((ActiveShipyards + SectorShip->ShipChildren.Num()) < SectorShip->GetDescription()->DroneMaximum)
			{
				SetOwnerShip(SectorShip);
				SectorShip->GetCurrentFleet()->AddShip(this);
				return;
			}
		}

		if (GetActive())
		{
			GetActive()->GetDamageSystem()->SetDead();
			return;
		}
	}

	GetDamageSystem()->SetDead();
}

/*----------------------------------------------------
	Resources

----------------------------------------------------*/

bool UFlareSimulatedSpacecraft::CanTradeWhiteListTo(UFlareFleet* OtherFleet, FFlareResourceDescription* Resource)
{
	if (!OtherFleet)
	{
		return true;
	}
	UFlareCompanyWhiteList* ActiveWhiteList = GetActiveWhitelist();
	if (ActiveWhiteList)
	{
		FFlareWhiteListCompanyDataSave* CompanyData = ActiveWhiteList->GetCompanyDataFor(OtherFleet->GetFleetCompany());
		if (CompanyData)
		{
			FText Reason;
			if (!ActiveWhiteList->CanCompanyDataTradeTo(CompanyData, Resource, Reason))
			{
				return false;
			}
		}
	}
	return true;
}

bool UFlareSimulatedSpacecraft::CanTradeWhiteListTo(UFlareSimulatedSpacecraft* OtherSpacecraft, FText& Reason, FFlareResourceDescription* Resource)
{
	if (!OtherSpacecraft)
	{
		return true;
	}
	UFlareCompanyWhiteList* ActiveWhiteList = GetActiveWhitelist();
	if (ActiveWhiteList)
	{
		FFlareWhiteListCompanyDataSave* CompanyData = ActiveWhiteList->GetCompanyDataFor(OtherSpacecraft->GetCompany());
		if (CompanyData)
		{
			if (!ActiveWhiteList->CanCompanyDataTradeTo(CompanyData, Resource, Reason))
			{
				return false;
			}
		}
	}
	return true;
}

bool UFlareSimulatedSpacecraft::CanTradeWhiteListFrom(UFlareSimulatedSpacecraft* OtherSpacecraft, FText& Reason, FFlareResourceDescription* Resource)
{
	if (!OtherSpacecraft)
	{
		return true;
	}
	UFlareCompanyWhiteList* ActiveWhiteList = GetActiveWhitelist();
	if (ActiveWhiteList)
	{
		FFlareWhiteListCompanyDataSave* CompanyData = ActiveWhiteList->GetCompanyDataFor(OtherSpacecraft->GetCompany());
		if (CompanyData)
		{
			if (!ActiveWhiteList->CanCompanyDataTradeFrom(CompanyData, Resource, Reason))
			{
				return false;
			}
		}
	}
	return true;
}


bool UFlareSimulatedSpacecraft::CanTradeWith(UFlareSimulatedSpacecraft* OtherSpacecraft, FText& Reason, FFlareResourceDescription* Resource)
{
	// Check if both spacecraft are in the same sector
	if (GetCurrentSector() != OtherSpacecraft->GetCurrentSector())
	{
		Reason = LOCTEXT("CantTradeSector", "Can't trade between different sectors");
		return false;
	}

	// Damaged
	if (GetDamageSystem()->IsUncontrollable() || OtherSpacecraft->GetDamageSystem()->IsUncontrollable())
	{
		Reason = LOCTEXT("CantTradeUncontrollable", "Can't trade with uncontrollable ships");
		return false;
	}

	// Check if spacecraft are both stations
	if (IsStation() && OtherSpacecraft->IsStation())
	{
		Reason = LOCTEXT("CantTradeStations", "Can't trade between two stations");
		return false;
	}

	// Check if spacecraft are not both ships
	if (!IsStation() && !OtherSpacecraft->IsStation())
	{
		Reason = LOCTEXT("CantTradeShips", "Can't trade between two ships");
		return false;
	}

	// Check if spacecraft are are not already trading

	//	if (IsTrading() || OtherSpacecraft->IsTrading())
	if (Game->GetActiveSector())
	{
		if (Game->GetActiveSector()->GetSimulatedSector() != GetCurrentSector() && (IsTrading() || OtherSpacecraft->IsTrading()))
		{
			Reason = LOCTEXT("CantTradeAlreadyTrading", "Can't trade with spacecraft that are already in a trading transaction");
			return false;
		}
	}

	if (IsTrading() || OtherSpacecraft->IsTrading())
	{
		Reason = LOCTEXT("CantTradeAlreadyTrading", "Can't trade with spacecraft that are already in a trading transaction");
		return false;
	}

	// Check if both spacecraft are not at war
	if (GetCompany()->GetWarState(OtherSpacecraft->GetCompany()) == EFlareHostility::Hostile)
	{
		Reason = LOCTEXT("CantTradeWar", "Can't trade between enemies");
		return false;
	}

	if (!CanTradeWhiteListTo(OtherSpacecraft, Reason, Resource))
	{
		return false;
	}

	if (!OtherSpacecraft->CanTradeWhiteListFrom(this, Reason, Resource))
	{
		return false;
	}

	return true;
}

FFlareResourceUsage UFlareSimulatedSpacecraft::GetResourceUseType(FFlareResourceDescription* Resource)
{
	FFlareResourceUsage Usage;

	// Check we're and station
	if (!IsStation())
	{
		Usage.AddUsage(EFlareResourcePriceContext::Default);
		return Usage;
	}

	// Parse factories
	for (UFlareFactory* Factory : Factories)
	{

		// Is input resource of a station ?
		for (int32 ResourceIndex = 0; ResourceIndex < Factory->GetCycleData().InputResources.Num(); ResourceIndex++)
		{
			const FFlareFactoryResource* FactoryResource = &Factory->GetCycleData().InputResources[ResourceIndex];
			if (&FactoryResource->Resource->Data == Resource)
			{
				Usage.AddUsage(EFlareResourcePriceContext::FactoryInput);
				break;
			}
		}

		// Is output resource of a station ?
		for (int32 ResourceIndex = 0; ResourceIndex < Factory->GetCycleData().OutputResources.Num(); ResourceIndex++)
		{
			const FFlareFactoryResource* FactoryResource = &Factory->GetCycleData().OutputResources[ResourceIndex];
			if (&FactoryResource->Resource->Data == Resource)
			{
				Usage.AddUsage(EFlareResourcePriceContext::FactoryOutput);
				break;
			}
		}
	}

	// Customer resource ?
	if (HasCapability(EFlareSpacecraftCapability::Consumer) && Resource->IsConsumerResource)
	{
		Usage.AddUsage(EFlareResourcePriceContext::ConsumerConsumption);
	}

	// Maintenance resource ?
	if (HasCapability(EFlareSpacecraftCapability::Maintenance) && Resource->IsMaintenanceResource)
	{
		Usage.AddUsage(EFlareResourcePriceContext::MaintenanceConsumption);
	}

	// Maintenance resource ?
	if (HasCapability(EFlareSpacecraftCapability::Storage))
	{
		bool Input = false;
		bool Output = false;

		for(FFlareCargo& Slot : GetActiveCargoBay()->GetSlots())
		{
			if(Slot.Lock == EFlareResourceLock::Input || Slot.Lock == EFlareResourceLock::Trade)
			{
				Input = true;
				if(Output)
				{
					break;
				}
			}

			if(Slot.Lock == EFlareResourceLock::Output || Slot.Lock == EFlareResourceLock::Trade)
			{
				Output = true;
				if(Input)
				{
					break;
				}
			}
		}

		if(Input)
		{
			Usage.AddUsage(EFlareResourcePriceContext::HubInput);
		}

		if(Output)
		{
			Usage.AddUsage(EFlareResourcePriceContext::HubOutput);
		}
	}


	return Usage;
}

void UFlareSimulatedSpacecraft::LockResources()
{
	GetActiveCargoBay()->UnlockAll();

	TMap<FFlareResourceDescription*, EFlareResourceLock::Type> LockMap;

	auto AddLockInMap = [&LockMap](FFlareResourceDescription* Resource, EFlareResourceLock::Type LockType)
	{
		if(LockMap.Contains(Resource))
		{
			if(LockType == EFlareResourceLock::Hidden)
			{
				return;
			}

			EFlareResourceLock::Type OldLockType = LockMap[Resource];
			if(OldLockType != LockType)
			{
				LockMap[Resource] = EFlareResourceLock::Trade;
			}
		}
		else
		{
			LockMap.Add(Resource, LockType);
		}
	};

	if (Factories.Num() > 0)
	{
		for (UFlareFactory* Factory : Factories)
		{
			for (int32 ResourceIndex = 0; ResourceIndex < Factory->GetCycleData().InputResources.Num(); ResourceIndex++)
			{
				const FFlareFactoryResource* Resource = &Factory->GetCycleData().InputResources[ResourceIndex];

				AddLockInMap(&Resource->Resource->Data, EFlareResourceLock::Input);

				/*if (!GetActiveCargoBay()->LockSlot(&Resource->Resource->Data, EFlareResourceLock::Input, false))
				{
					FLOGV("Fail to lock a slot of %s in %s", *(&Resource->Resource->Data)->Name.ToString(), *GetImmatriculation().ToString());
				}*/
			}

			for (int32 ResourceIndex = 0; ResourceIndex < Factory->GetCycleData().OutputResources.Num(); ResourceIndex++)
			{
				const FFlareFactoryResource* Resource = &Factory->GetCycleData().OutputResources[ResourceIndex];

				AddLockInMap(&Resource->Resource->Data, EFlareResourceLock::Output);


				/*if (!GetActiveCargoBay()->LockSlot(&Resource->Resource->Data, EFlareResourceLock::Output, false))
				{
					FLOGV("Fail to lock a slot of %s in %s", *(&Resource->Resource->Data)->Name.ToString(), *GetImmatriculation().ToString());
				}*/
			}
		}
	}

	if (!IsUnderConstruction())
	{

		if (HasCapability(EFlareSpacecraftCapability::Consumer))
		{
			for (int32 ResourceIndex = 0; ResourceIndex < GetGame()->GetResourceCatalog()->ConsumerResources.Num(); ResourceIndex++)
			{
				FFlareResourceDescription* Resource = &GetGame()->GetResourceCatalog()->ConsumerResources[ResourceIndex]->Data;
				AddLockInMap(Resource, EFlareResourceLock::Input);


				/*if (!GetActiveCargoBay()->LockSlot(Resource, EFlareResourceLock::Input, false))
				{
					FLOGV("Fail to lock a slot of %s in %s", *Resource->Name.ToString(), *GetImmatriculation().ToString());
				}*/
			}
		}

		if (HasCapability(EFlareSpacecraftCapability::Maintenance))
		{
			for (int32 ResourceIndex = 0; ResourceIndex < GetGame()->GetResourceCatalog()->MaintenanceResources.Num(); ResourceIndex++)
			{
				FFlareResourceDescription* Resource = &GetGame()->GetResourceCatalog()->MaintenanceResources[ResourceIndex]->Data;

				AddLockInMap(Resource, EFlareResourceLock::Trade);
			}
		}

		if (HasCapability(EFlareSpacecraftCapability::Storage))
		{
			// Lock resource with usage as Trade
			for (int32 ResourceIndex = 0; ResourceIndex < GetGame()->GetResourceCatalog()->Resources.Num(); ResourceIndex++)
			{
				FFlareResourceDescription* Resource = &GetGame()->GetResourceCatalog()->Resources[ResourceIndex]->Data;

				if(GetCurrentSector())
				{
					for(UFlareSimulatedSpacecraft* Station : GetCurrentSector()->GetSectorStations())
					{
						 FFlareResourceUsage Usage = Station->GetResourceUseType(Resource);


						 if(Usage.HasAnyUsage())
						 {
							 AddLockInMap(Resource, EFlareResourceLock::Trade);
							 break;
						 }
					}
				}

				// Lock resource with stock in cargo bay as Output
				int32 ResourceQuantity = GetActiveCargoBay()->GetResourceQuantity(Resource, GetCompany());

				if(ResourceQuantity > 0)
				{
					AddLockInMap(Resource, EFlareResourceLock::Output);
				}
			}
		}

		if(IsComplex())
		{
			// Lock resource with usage as Trade
			for (int32 ResourceIndex = 0; ResourceIndex < GetGame()->GetResourceCatalog()->Resources.Num(); ResourceIndex++)
			{
				FFlareResourceDescription* Resource = &GetGame()->GetResourceCatalog()->Resources[ResourceIndex]->Data;
				AddLockInMap(Resource, EFlareResourceLock::Hidden);

			}
		}
	}

	for(auto Entry : LockMap)
	{
		if (!GetActiveCargoBay()->LockSlot(Entry.Key, Entry.Value, false))
		{
			FLOGV("Fail to lock a slot of %s in %s", *Entry.Key->Name.ToString(), *GetImmatriculation().ToString());
		}
	}

	if(IsStation())
	{
		GetActiveCargoBay()->HideUnlockedSlots();
	}
}

void UFlareSimulatedSpacecraft::ComputeConstructionCargoBaySize(int32& CargoBaySlotCapacity, int32& CargoBayCount)
{
	if(IsUnderConstruction())
	{
		UFlareFactory* Factory = GetFactories()[0];
		CargoBaySlotCapacity = 0;
		for(const FFlareFactoryResource& Resource: Factory->GetCycleData().InputResources)
		{
			if(Resource.Quantity > CargoBaySlotCapacity)
			{
				CargoBaySlotCapacity = Resource.Quantity;
			}
		}

		CargoBayCount = Factory->GetCycleData().InputResources.Num();
	}
	else
	{
		CargoBayCount = 0;
		CargoBaySlotCapacity = 0;
	}
}

void UFlareSimulatedSpacecraft::ComputeProductionCargoBaySize(int32& CargoBaySlotCapacity, int32& CargoBayCount)
{
	if(!IsUnderConstruction())
	{
		if(IsComplex())
		{
			UFlareResourceCatalog* Catalog = GetGame()->GetResourceCatalog();
			CargoBayCount = Catalog->GetResourceList().Num();
			CargoBaySlotCapacity = 0;

			for(UFlareSimulatedSpacecraft* Child : GetComplexChildren())
			{
				int32 LocalCapacity = Child->GetDescription()->CargoBayCapacity * Child->GetLevel();
				if(LocalCapacity > CargoBaySlotCapacity)
				{
					CargoBaySlotCapacity = LocalCapacity;
				}
			}
		}
		else if(HasCapability(EFlareSpacecraftCapability::Storage))
		{
			UFlareResourceCatalog* Catalog = GetGame()->GetResourceCatalog();
			CargoBayCount = Catalog->GetResourceList().Num();
			CargoBaySlotCapacity = GetDescription()->CargoBayCapacity * GetLevel();
		}
		else
		{
			CargoBayCount = GetDescription()->CargoBayCount;
			CargoBaySlotCapacity = GetDescription()->CargoBayCapacity * GetLevel();
		}
	}
	else
	{
		CargoBayCount = 0;
		CargoBaySlotCapacity = 0;
	}
}

void UFlareSimulatedSpacecraft::SetAsteroidData(FFlareAsteroidSave* Data)
{
	SpacecraftData.AsteroidData.Identifier = Data->Identifier;
	SpacecraftData.AsteroidData.AsteroidMeshID = Data->AsteroidMeshID;
	SpacecraftData.AsteroidData.Scale = Data->Scale;
	SpacecraftData.Location = Data->Location;
	SpacecraftData.Rotation = Data->Rotation;
}

void UFlareSimulatedSpacecraft::SetComplexStationAttachment(FName StationName, FName ConnectorName)
{
	FLOGV("UFlareSimulatedSpacecraft::SetComplexStationAttachment : %s will attach to %s at %s",
		*GetImmatriculation().ToString(), *StationName.ToString(), *ConnectorName.ToString());

	SpacecraftData.AttachComplexStationName = StationName;
	SpacecraftData.AttachComplexConnectorName = ConnectorName;
}

void UFlareSimulatedSpacecraft::SetActorAttachment(FName ActorName)
{
	FLOGV("UFlareSimulatedSpacecraft::SetActorAttachment : %s will attach to %s",
		*GetImmatriculation().ToString(), *ActorName.ToString());

	SpacecraftData.AttachActorName = ActorName;
}

void UFlareSimulatedSpacecraft::SetDynamicComponentState(FName Identifier, float Progress)
{
	SpacecraftData.DynamicComponentStateIdentifier = Identifier;
	SpacecraftData.DynamicComponentStateProgress = Progress;
}

void UFlareSimulatedSpacecraft::Upgrade()
{
	FLOGV("UFlareSimulatedSpacecraft::Upgrade %s to level %d", *GetImmatriculation().ToString(), SpacecraftData.Level+1);

	SpacecraftData.Level++;
	SpacecraftData.IsUnderConstruction = true;

	Reload();

	if(GetCompany() == Game->GetPC()->GetCompany())
	{
		Game->GetQuestManager()->OnEvent(FFlareBundle().PutTag("start-station-construction").PutInt32("upgrade", 1));
	}

	if(IsComplexElement())
	{
		GetComplexMaster()->Reload();
	}

	FLOGV("UFlareSimulatedSpacecraft::Upgrade %s to level %d done", *GetImmatriculation().ToString(), SpacecraftData.Level);
}


void UFlareSimulatedSpacecraft::CancelUpgrade()
{
	if (IsComplex())
	{
		for (UFlareSimulatedSpacecraft* Substation : GetComplexChildren())
		{
			if (Substation->IsUnderConstruction(true) && Substation->GetLevel() > 1)
			{
				Substation->CancelUpgrade();
				break;
			}
		}
	}
	else
	{
		if(!SpacecraftData.IsUnderConstruction)
		{
			return;
		}

		SpacecraftData.Level--;


		int64 ProductionCost = GetStationUpgradeFee();

		// Refund station cost
		Company->GiveMoney(ProductionCost, FFlareTransactionLogEntry::LogCancelUpgradeStationFees(this));
		GetCurrentSector()->GetPeople()->TakeMoney(ProductionCost);

		FinishConstruction();
	}
}


void UFlareSimulatedSpacecraft::ForceUndock()
{
	SpacecraftData.DockedTo = NAME_None;
	SpacecraftData.DockedAt = -1;
}

void UFlareSimulatedSpacecraft::SetTrading(bool Trading, int32 TradeReason)
{
	if (IsStation())
	{
		FLOGV("Fail to set trading state to %s : station are never locked in trading state", *GetImmatriculation().ToString());
		return;
	}

	// Notify end of state if not on trade route
	if (!Trading
		&& SpacecraftData.IsTrading
		&& GetCompany() == GetGame()->GetPC()->GetCompany()
		&& (GetCurrentTradeRoute() == NULL || GetCurrentTradeRoute()->IsPaused())
		&& !GetCurrentFleet()->IsAutoTrading()
		&& SpacecraftData.TradingReason != 1)
	{
		FFlareMenuParameterData Data;
		Data.Spacecraft = this;
		Game->GetPC()->Notify(LOCTEXT("TradingStateEnd", "Trading complete"),
			FText::Format(LOCTEXT("TravelEndedFormat", "{0} finished trading in {1}"),
				UFlareGameTools::DisplaySpacecraftName(this),
				GetCurrentSector()->GetSectorName()),
			FName("trading-state-end"),
			EFlareNotification::NT_Economy,
			false,
			EFlareMenu::MENU_Ship,
			Data);
	}

	SpacecraftData.IsTrading = Trading;
	SpacecraftData.TradingReason = TradeReason;
}

void UFlareSimulatedSpacecraft::SetIntercepted(bool Intercepted)
{
	SpacecraftData.IsIntercepted = Intercepted;
}

void UFlareSimulatedSpacecraft::SetReserve(bool InReserve)
{
	SpacecraftData.IsReserve = InReserve;
}

void UFlareSimulatedSpacecraft::Repair()
{
	if(GetRepairStock() <= 0 || (GetCurrentSector() && GetCurrentSector()->IsInDangerousBattle(GetCompany())))
	{
		// No repair possible
		return;
	}

	UFlareSpacecraftComponentsCatalog* Catalog = GetGame()->GetShipPartsCatalog();

	float SpacecraftPreciseCurrentNeededFleetSupply = 0;

	// List components
	for (int32 ComponentIndex = 0; ComponentIndex < GetData().Components.Num(); ComponentIndex++)
	{
		FFlareSpacecraftComponentSave* ComponentData = &GetData().Components[ComponentIndex];
		FFlareSpacecraftComponentDescription* ComponentDescription = Catalog->Get(ComponentData->ComponentIdentifier);

		float DamageRatio = GetDamageSystem()->GetDamageRatio(ComponentDescription, ComponentData);
		float TechnologyBonus = GetCompany()->IsTechnologyUnlocked("quick-repair") ? 1.5f: 1.f;
		float TechnologyBonusSecondary = GetCompany()->GetTechnologyBonus("repair-bonus");
		TechnologyBonus += TechnologyBonusSecondary;

		float ComponentMaxRepairRatio = SectorHelper::GetComponentMaxRepairRatio(ComponentDescription) * (GetSize() == EFlarePartSize::L ? 0.2f : 1.f) * TechnologyBonus;
		float CurrentRepairRatio = FMath::Min(ComponentMaxRepairRatio, (1.f - DamageRatio));


		SpacecraftPreciseCurrentNeededFleetSupply += CurrentRepairRatio * UFlareSimulatedSpacecraftDamageSystem::GetRepairCost(ComponentDescription);
	}

	if(SpacecraftPreciseCurrentNeededFleetSupply != 0)
	{

		float MaxRepairRatio = FMath::Min(1.f, GetRepairStock() / SpacecraftPreciseCurrentNeededFleetSupply);

		for (int32 ComponentIndex = 0; ComponentIndex < GetData().Components.Num(); ComponentIndex++)
		{
			FFlareSpacecraftComponentSave* ComponentData = &GetData().Components[ComponentIndex];
			FFlareSpacecraftComponentDescription* ComponentDescription = Catalog->Get(ComponentData->ComponentIdentifier);

			float TechnologyBonus = GetCompany()->IsTechnologyUnlocked("quick-repair") ? 1.5f: 1.f;
			float TechnologyBonusSecondary = GetCompany()->GetTechnologyBonus("repair-bonus");
			TechnologyBonus += TechnologyBonusSecondary;

			float ComponentMaxRepairRatio = SectorHelper::GetComponentMaxRepairRatio(ComponentDescription) * (GetSize() == EFlarePartSize::L ? 0.2f : 1.f) * TechnologyBonus;
			float ConsumedFS = GetDamageSystem()->Repair(ComponentDescription,ComponentData, MaxRepairRatio * ComponentMaxRepairRatio, SpacecraftData.RepairStock);

			SpacecraftData.RepairStock -= ConsumedFS;
			if(GetRepairStock() <= 0)
			{
				break;
			}
		}
	}


	if (GetDamageSystem()->GetGlobalDamageRatio() >= 1.f)
	{
		SpacecraftData.RepairStock = 0;

		if(SpacecraftPreciseCurrentNeededFleetSupply > 0)
		{
			if(GetCompany() == GetGame()->GetPC()->GetCompany())
			{
				GetGame()->GetQuestManager()->OnEvent(FFlareBundle().PutTag("repair-end"));
			}
		}
	}
}

void UFlareSimulatedSpacecraft::RecoveryRepair()
{
	SpacecraftData.RepairStock = 0;


	UFlareSpacecraftComponentsCatalog* Catalog = GetGame()->GetShipPartsCatalog();


	for (int32 ComponentIndex = 0; ComponentIndex < GetData().Components.Num(); ComponentIndex++)
	{
		FFlareSpacecraftComponentSave* ComponentData = &GetData().Components[ComponentIndex];
		FFlareSpacecraftComponentDescription* ComponentDescription = Catalog->Get(ComponentData->ComponentIdentifier);

		if (ComponentDescription->Type == EFlarePartType::RCS
				|| ComponentDescription->Type == EFlarePartType::OrbitalEngine
				|| ComponentDescription->Type == EFlarePartType::InternalComponent)
		{
			float MaxDamage = (1-(BROKEN_RATIO + 0.1)) * ComponentDescription->HitPoints;
			if(ComponentDescription->Type == EFlarePartType::OrbitalEngine)
			{
				MaxDamage = BROKEN_RATIO * 0.6 * ComponentDescription->HitPoints;
			}

			ComponentData->Damage = FMath::Min(ComponentData->Damage, MaxDamage);

			GetDamageSystem()->SetDamageDirty(ComponentDescription);

			if(!GetDamageSystem()->IsStranded())
			{
				return;
			}
		}
	}
}

void UFlareSimulatedSpacecraft::Stabilize()
{
	if(!GetDamageSystem()->IsUncontrollable())
	{
		SpacecraftData.LinearVelocity = FVector::ZeroVector;
		SpacecraftData.AngularVelocity = FVector::ZeroVector;

//		float Limits = UFlareSector::GetSectorLimits();
		float Limits = GetCurrentSector()->GetSectorLimits();
		float Distance = SpacecraftData.Location.Size();

		if (GetCurrentSector())
		{
			Limits = GetCurrentSector()->GetSectorLimits();
		}
		else if (GetCurrentFleet()->GetCurrentTravel() && GetCurrentFleet()->GetCurrentTravel()->GetDestinationSector())
		{
			Limits = GetCurrentFleet()->GetCurrentTravel()->GetDestinationSector()->GetSectorLimits();
		}

		if(Distance > Limits)
		{
			float Correction = 0.8* Limits / Distance;
			SpacecraftData.Location *= Correction;
		}
	}
}

void UFlareSimulatedSpacecraft::Refill()
{
	if(GetRefillStock() == 0 || (GetCurrentSector() && GetCurrentSector()->IsInDangerousBattle(GetCompany())))
	{
		// No refill possible
		return;
	}

	UFlareSpacecraftComponentsCatalog* Catalog = GetGame()->GetShipPartsCatalog();
	float SpacecraftPreciseCurrentNeededFleetSupply = 0;

	// List components
	for (int32 ComponentIndex = 0; ComponentIndex < GetData().Components.Num(); ComponentIndex++)
	{
		FFlareSpacecraftComponentSave* ComponentData = &GetData().Components[ComponentIndex];
		FFlareSpacecraftComponentDescription* ComponentDescription = Catalog->Get(ComponentData->ComponentIdentifier);
		if(ComponentDescription->Type == EFlarePartType::Weapon)
		{
			int32 MaxAmmo = ComponentDescription->WeaponCharacteristics.AmmoCapacity;
			int32 CurrentAmmo = MaxAmmo - ComponentData->Weapon.FiredAmmo;
			float FillRatio = (float) CurrentAmmo / (float) MaxAmmo;
			float CurrentRefillRatio = FMath::Min(MAX_REFILL_RATIO_BY_DAY * (GetSize() == EFlarePartSize::L ? 0.2f : 1.f), (1.f - FillRatio));

			SpacecraftPreciseCurrentNeededFleetSupply += CurrentRefillRatio * UFlareSimulatedSpacecraftDamageSystem::GetRefillCost(ComponentDescription);
		}

	}

	if(SpacecraftPreciseCurrentNeededFleetSupply != 0)
	{
		float MaxRefillRatio = MAX_REFILL_RATIO_BY_DAY * (GetSize() == EFlarePartSize::L ? 0.2f : 1.f) * FMath::Min(1.f,GetRefillStock() / SpacecraftPreciseCurrentNeededFleetSupply);

		for (int32 ComponentIndex = 0; ComponentIndex < GetData().Components.Num(); ComponentIndex++)
		{
			FFlareSpacecraftComponentSave* ComponentData = &GetData().Components[ComponentIndex];
			FFlareSpacecraftComponentDescription* ComponentDescription = Catalog->Get(ComponentData->ComponentIdentifier);

			if(ComponentDescription->Type == EFlarePartType::Weapon)
			{
				float ConsumedFS = GetDamageSystem()->Refill(ComponentDescription,ComponentData, MaxRefillRatio, SpacecraftData.RefillStock);

				SpacecraftData.RefillStock -= ConsumedFS;

				if(GetRefillStock() <= 0)
				{
					break;
				}
			}
		}
	}

	if (!NeedRefill())
	{
		SpacecraftData.RefillStock = 0;


		if(SpacecraftPreciseCurrentNeededFleetSupply > 0)
		{
			if(GetCompany() == GetGame()->GetPC()->GetCompany())
			{
				GetGame()->GetQuestManager()->OnEvent(FFlareBundle().PutTag("refill-end"));
			}
		}
	}

}

void UFlareSimulatedSpacecraft::SetHarpooned(UFlareCompany* OwnerCompany)
{
	if (OwnerCompany) {
		if (SpacecraftData.HarpoonCompany != OwnerCompany->GetIdentifier())
		{
			CombatLog::SpacecraftHarpooned(this, OwnerCompany);
			SpacecraftData.HarpoonCompany  = OwnerCompany->GetIdentifier();
		}
	}
	else
	{
		SpacecraftData.HarpoonCompany = NAME_None;
	}
}


void UFlareSimulatedSpacecraft::SetInternalDockedTo(UFlareSimulatedSpacecraft* OwnerShip)
{
	if (OwnerShip)
	{
		this->SetSpawnMode(EFlareSpawnMode::InternalDocked);
		if (SpacecraftData.DockedAtInternally != OwnerShip->GetImmatriculation())
		{
			SpacecraftData.DockedAtInternally = OwnerShip->GetImmatriculation();
		}
	}
	else
	{
		SpacecraftData.DockedAtInternally = NAME_None;
	}
}

bool UFlareSimulatedSpacecraft::IsInternalDockedTo(UFlareSimulatedSpacecraft* OwnerShip)
{
	if (OwnerShip)
	{
		if (SpacecraftData.DockedAtInternally == OwnerShip->GetImmatriculation())
		{
			return true;
		}
	}
	return false;
}


void UFlareSimulatedSpacecraft::SetOwnerShip(UFlareSimulatedSpacecraft* OwnerShip)
{
	if (OwnerShip)
	{
		if (SpacecraftData.OwnerShipName != OwnerShip->GetImmatriculation())
		{
			if (this->ShipMaster != NULL && OwnerShip!=this->ShipMaster)
			{
				UFlareSimulatedSpacecraft* OldOwnerShip = this->ShipMaster;
				FFlareSpacecraftSave& OldOwnerData = OldOwnerShip->GetData();
				OldOwnerShip->AddShipChildren(this,true);
				OldOwnerData.OwnedShipNames.Remove(this->GetImmatriculation());
			}

			OwnerShip->AddShipChildren(this);

			SpacecraftData.OwnerShipName = OwnerShip->GetImmatriculation();
			FFlareSpacecraftSave& NewOwnerData = OwnerShip->GetData();
			NewOwnerData.OwnedShipNames.AddUnique(this->GetImmatriculation());
		}
	}
	else
	{
		if (this->ShipMaster != NULL)
		{
			UFlareSimulatedSpacecraft* OldOwnerShip = this->ShipMaster;
			FFlareSpacecraftSave& OldOwnerData = OldOwnerShip->GetData();
			OldOwnerShip->AddShipChildren(this, true);
			OldOwnerData.OwnedShipNames.Remove(this->GetImmatriculation());
		}
		SpacecraftData.OwnerShipName = NAME_None;
	}
}

UFlareCompany* UFlareSimulatedSpacecraft::GetHarpoonCompany()
{
	return Game->GetGameWorld()->FindCompany(SpacecraftData.HarpoonCompany);
}

void UFlareSimulatedSpacecraft::RemoveCapturePoint(FName CompanyIdentifier, int32 CapturePoint)
{
	if(SpacecraftData.CapturePoints.Contains(CompanyIdentifier))
	{
		int32 CurrentCapturePoint = SpacecraftData.CapturePoints[CompanyIdentifier];
		if(CapturePoint >= CurrentCapturePoint)
		{
			SpacecraftData.CapturePoints.Remove(CompanyIdentifier);
		}
		else
		{
			SpacecraftData.CapturePoints[CompanyIdentifier] = CurrentCapturePoint - CapturePoint;
		}
	}
}

void UFlareSimulatedSpacecraft::ResetCapture(UFlareCompany* OtherCompany)
{
	int32 ResetSpeedPoint = FMath::CeilToInt(GetCapturePointThreshold() * CAPTURE_RESET_SPEED);

	if (OtherCompany)
	{
		FName CompanyIdentifier = OtherCompany->GetIdentifier();
		RemoveCapturePoint(CompanyIdentifier, ResetSpeedPoint);

		OtherCompany->StopCapture(this);
	}
	else
	{
		TArray<FName> CapturingCompany;
		SpacecraftData.CapturePoints.GetKeys(CapturingCompany);

		for(int CompanyIndex = 0; CompanyIndex < CapturingCompany.Num(); CompanyIndex++)
		{
			RemoveCapturePoint(CapturingCompany[CompanyIndex], ResetSpeedPoint);
		}

		for(UFlareCompany* CompanyCandidate: Game->GetGameWorld()->GetCompanies())
		{
			CompanyCandidate->StopCapture(this);
		}
	}
}

bool UFlareSimulatedSpacecraft::TryCapture(UFlareCompany* OtherCompany, int32 CapturePoint)
{
	int32 CurrentCapturePoint = 0;
	FName CompanyIdentifier = OtherCompany->GetIdentifier();
	if (SpacecraftData.CapturePoints.Contains(CompanyIdentifier))
	{
		CurrentCapturePoint = SpacecraftData.CapturePoints[CompanyIdentifier];
	}

	CurrentCapturePoint += CapturePoint;

	if(SpacecraftData.CapturePoints.Contains(CompanyIdentifier)){
		SpacecraftData.CapturePoints[CompanyIdentifier] = CurrentCapturePoint;
	}
	else
	{
		SpacecraftData.CapturePoints.Add(CompanyIdentifier, CurrentCapturePoint);
	}

	if (CurrentCapturePoint > GetCapturePointThreshold())
	{
		// Can be captured
		return true;
	}

	return false;
}

bool UFlareSimulatedSpacecraft::UpgradePart(FFlareSpacecraftComponentDescription* NewPartDesc, int32 WeaponGroupIndex)
{

	UFlareSpacecraftComponentsCatalog* Catalog = Game->GetPC()->GetGame()->GetShipPartsCatalog();
	int32 TransactionCost = 0;

	// Update all components
	for (int32 i = 0; i < SpacecraftData.Components.Num(); i++)
	{
		bool UpdatePart = false;
		FFlareSpacecraftComponentDescription* ComponentDescription = Catalog->Get(SpacecraftData.Components[i].ComponentIdentifier);

		if (ComponentDescription->Type == NewPartDesc->Type)
		{

			// For a weapon, check if this slot belongs to the current group
			if (ComponentDescription->Type == EFlarePartType::Weapon)
			{
				FName SlotName = SpacecraftData.Components[i].ShipSlotIdentifier;
				int32 TargetGroupIndex = UFlareSimulatedSpacecraftWeaponsSystem::GetGroupIndexFromSlotIdentifier(GetDescription(), SlotName);
				UpdatePart = (TargetGroupIndex == WeaponGroupIndex);
			}
			else
			{
				UpdatePart = true;
			}
		}

		// Set the new description and reload the weapon if it was marked for change
		if (UpdatePart)
		{
			if(TransactionCost == 0)
			{
				TransactionCost = GetUpgradeCost(NewPartDesc, ComponentDescription);
				if(TransactionCost > GetCompany()->GetMoney())
				{
					// Cannot afford upgrade
					return false;
				}
			}
			SpacecraftData.Components[i].ComponentIdentifier = NewPartDesc->Identifier;
			SpacecraftData.Components[i].Weapon.FiredAmmo = 0;
			GetDamageSystem()->SetDamageDirty(ComponentDescription);
		}
	}

	// Update the world ship, take money from player, etc
	UFlareSimulatedSector* Sector = GetCurrentSector();
	if (TransactionCost > 0)
	{
		GetCompany()->TakeMoney(TransactionCost, false, FFlareTransactionLogEntry::LogUpgradeShipPart(this));
		if (Sector)
		{
			Sector->GetPeople()->Pay(TransactionCost);
		}
	}
	else
	{
		GetCompany()->GiveMoney(FMath::Abs(TransactionCost), FFlareTransactionLogEntry::LogUpgradeShipPart(this));
		if (Sector)
		{
			Sector->GetPeople()->TakeMoney(FMath::Abs(TransactionCost));
		}
	}

	Load(SpacecraftData);

	return true;
}

FFlareSpacecraftComponentDescription* UFlareSimulatedSpacecraft::GetCurrentPart(EFlarePartType::Type Type, int32 WeaponGroupIndex)
{
	UFlareSpacecraftComponentsCatalog* Catalog = Game->GetPC()->GetGame()->GetShipPartsCatalog();

	// Update all components
	for (int32 i = 0; i < SpacecraftData.Components.Num(); i++)
	{
		bool UpdatePart = false;
		FFlareSpacecraftComponentDescription* ComponentDescription = Catalog->Get(SpacecraftData.Components[i].ComponentIdentifier);

		if (ComponentDescription->Type == Type)
		{

			// For a weapon, check if this slot belongs to the current group
			if (ComponentDescription->Type == EFlarePartType::Weapon)
			{
				FName SlotName = SpacecraftData.Components[i].ShipSlotIdentifier;
				int32 TargetGroupIndex = UFlareSimulatedSpacecraftWeaponsSystem::GetGroupIndexFromSlotIdentifier(GetDescription(), SlotName);
				if(TargetGroupIndex == WeaponGroupIndex)
				{
					return ComponentDescription;
				}
			}
			else
			{
				return ComponentDescription;
			}
		}
	}
	return NULL;
}

void UFlareSimulatedSpacecraft::FinishConstruction()
{
	if(!IsUnderConstruction())
	{
		return;
	}

	Company->GetAI()->FinishedConstruction(this);
	SpacecraftData.IsUnderConstruction = false;

	Reload();
	if (IsShipyard())
	{
		for (UFlareFactory* Factory : Factories)
		{
			Factory->Stop();
		}
	}

	if(IsComplexElement())
	{
		GetComplexMaster()->Reload();
	}
}



void UFlareSimulatedSpacecraft::OrderRepairStock(float FS)
{
	SpacecraftData.RepairStock += FS;
}

void UFlareSimulatedSpacecraft::OrderRefillStock(float FS)
{
	SpacecraftData.RefillStock += FS;
}

bool UFlareSimulatedSpacecraft::NeedRefill()
{
	UFlareSpacecraftComponentsCatalog* Catalog = GetGame()->GetShipPartsCatalog();

	// List components
	for (int32 ComponentIndex = 0; ComponentIndex < GetData().Components.Num(); ComponentIndex++)
	{
		FFlareSpacecraftComponentSave* ComponentData = &GetData().Components[ComponentIndex];
		FFlareSpacecraftComponentDescription* ComponentDescription = Catalog->Get(ComponentData->ComponentIdentifier);

		if(ComponentDescription->Type == EFlarePartType::Weapon)
		{

			int32 MaxAmmo = ComponentDescription->WeaponCharacteristics.AmmoCapacity;
			int32 CurrentAmmo = MaxAmmo - ComponentData->Weapon.FiredAmmo;

			float FillRatio = (float) CurrentAmmo / (float) MaxAmmo;

			if(FillRatio < 1.f)
			{
				return true;
			}
		}
	}

	return false;
}

bool UFlareSimulatedSpacecraft::IsShipyard()
{
	for (UFlareFactory* Factory : GetFactories())
	{
		if (Factory->IsShipyard())
		{
			return true;
		}
	}
	return false;
}

void UFlareSimulatedSpacecraft::AutoFillConstructionCargoBay()
{
	UFlareCargoBay* Construction = GetConstructionCargoBay();
	UFlareCargoBay* Production = GetProductionCargoBay();


	for(FFlareCargo& Slot : Construction->GetSlots())
	{
		if(Slot.Lock == EFlareResourceLock::Input)
		{
			int32 MissingQuantity = Construction->GetFreeSpaceForResource(Slot.Resource, GetCompany(), true);
			int32 TakenQuantity = Production->TakeResources(Slot.Resource, MissingQuantity, GetCompany());
			Construction->GiveResources(Slot.Resource, TakenQuantity, GetCompany());
		}
	}
}

bool UFlareSimulatedSpacecraft::CanScrapStation() const
{
	if(IsComplex() && GetComplexChildren().Num() > 0)
	{
		return false;
	}

	return true;
}

TMap<FFlareResourceDescription*, int32> UFlareSimulatedSpacecraft::ComputeScrapResources() const
{
	TMap<FFlareResourceDescription*, int32> ScrapResources;

	auto AddToMap = [&ScrapResources](FFlareResourceDescription* Resource, int32 Quantity)
	{
		if(ScrapResources.Contains(Resource))
		{
			ScrapResources[Resource] += Quantity;
		}
		else
		{
			ScrapResources.Add(Resource, Quantity);
		}
	};

	auto AddBayContent = [&AddToMap](UFlareCargoBay* Bay)
	{
		for(FFlareCargo& Slot : Bay->GetSlots())
		{
			if(Slot.Quantity > 0)
			{
				AddToMap(Slot.Resource, Slot.Quantity);
			}
		}
	};

	if(!IsComplexElement())
	{
		AddBayContent(GetProductionCargoBay());
	}

	if(IsUnderConstruction(true))
	{
		AddBayContent(GetConstructionCargoBay());
	}


	int32 ApplicableLevel = IsUnderConstruction(true) ? GetLevel() - 1 : GetLevel();

	if (ApplicableLevel > 0)
	{
		for(FFlareFactoryResource& Resource : GetDescription()->CycleCost.InputResources)
		{
			AddToMap(&Resource.Resource->Data, Resource.Quantity * ApplicableLevel);
		}
	}

	return ScrapResources;
}


/*----------------------------------------------------
	Complexes
----------------------------------------------------*/

bool UFlareSimulatedSpacecraft::IsComplex() const
{
	if (GetDescription()->Identifier == "station-complex")
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool UFlareSimulatedSpacecraft::IsComplexElement() const
{
	return (GetDataConst().AttachComplexStationName != NAME_None);
}

void UFlareSimulatedSpacecraft::RegisterComplexMaster(UFlareSimulatedSpacecraft* master)
{
	FLOGV("RegisterComplexMaster %p to %p", master, this)
	ComplexMaster = master;
}

bool UFlareSimulatedSpacecraft::IsSpecialComplexSlot(FName ConnectorName)
{
	return ConnectorName.ToString().Contains(TEXT("special"));
}

TArray<FFlareDockingInfo>& UFlareSimulatedSpacecraft::GetStationConnectors()
{
	return ConnectorSlots;
}

FFlareDockingInfo* UFlareSimulatedSpacecraft::GetStationConnector(FName Name)
{
	auto FindByName = [=](const FFlareDockingInfo& Slot)
	{
		return Slot.Name == Name;
	};

	return ConnectorSlots.FindByPredicate(FindByName);
}

void UFlareSimulatedSpacecraft::RegisterComplexElement(FFlareConnectionSave ConnectionData)
{
	for (FFlareDockingInfo& StationConnection : ConnectorSlots)
	{
		if (StationConnection.Name == ConnectionData.ConnectorName)
		{
			StationConnection.ConnectedStationName = ConnectionData.StationIdentifier;
			StationConnection.Occupied = false;
			StationConnection.Granted = true;
		}
	}
}

void UFlareSimulatedSpacecraft::UnregisterComplexElement(UFlareSimulatedSpacecraft* Element)
{
	for (FFlareDockingInfo& StationConnection : ConnectorSlots)
	{
		if (StationConnection.ConnectedStationName == Element->GetImmatriculation())
		{
			StationConnection.ConnectedStationName = NAME_None;
			StationConnection.Occupied = false;
			StationConnection.Granted = false;
		}
	}
}

/*----------------------------------------------------
	Shipyards
----------------------------------------------------*/

bool UFlareSimulatedSpacecraft::ShipyardOrderShip(UFlareCompany* OrderCompany, FName ShipIdentifier, bool LimitedQueues)
{
	FFlareSpacecraftDescription* ShipDescription = GetGame()->GetSpacecraftCatalog()->Get(ShipIdentifier);

	if (!ShipDescription || !CanOrder(ShipDescription, OrderCompany, LimitedQueues))
	{
		return false;
	}

	uint32 ShipPrice = 0;

	if(GetCompany() != OrderCompany)
	{
		ShipPrice = UFlareGameTools::ComputeSpacecraftPrice(ShipIdentifier, GetCurrentSector(), true, false, true, OrderCompany,GetCompany());
		if(!OrderCompany->TakeMoney(ShipPrice, false, FFlareTransactionLogEntry::LogOrderShip(this, ShipIdentifier)))
		{
			// Not enough money
			return false;
		}
	}

	FFlareShipyardOrderSave newOrder;
	newOrder.ShipClass = ShipIdentifier;
	newOrder.Company = OrderCompany->GetIdentifier();
	newOrder.AdvancePayment = ShipPrice;

	SpacecraftData.ShipyardOrderQueue.Add(newOrder);
	UpdateShipyardProduction();

	if (OrderCompany == Game->GetPC()->GetCompany())
	{
		FFlareSpacecraftDescription* Desc = Game->GetSpacecraftCatalog()->Get(ShipIdentifier);

		int32 Size = 0;
		if (Desc)
		{
			Size = Desc->Size;
		}

		int32 Military = 0;
		if(Desc && (Desc->WeaponGroups.Num()  > 0 || Desc->TurretSlots.Num()  > 0 ))
		{
			Military = 1;
		}

		Game->GetQuestManager()->OnEvent(FFlareBundle().PutTag("order-ship").PutInt32("size", Size).PutInt32("military", Military));

		int32 GameDifficulty = -1;
		GameDifficulty = Game->GetPC()->GetPlayerData()->DifficultyId;
		int32 RepDivisor = 100000;
		switch (GameDifficulty)
		{
		case -1: // Easy
			RepDivisor = 100000;
			break;
		case 0: // Normal
			RepDivisor = 100000;
			break;
		case 1: // Hard
			RepDivisor = 125000;
			break;
		case 2: // Very Hard
			RepDivisor = 150000;
			break;
		case 3: // Expert
			RepDivisor = 200000;
			break;
		case 4: // Unfair
			RepDivisor = 250000;
			break;
		}

		GetCompany()->GivePlayerReputation(ShipPrice / RepDivisor);
	}
	return true;
}
void UFlareSimulatedSpacecraft::CancelShipyardOrder(int32 OrderIndex)
{
	if(OrderIndex < 0 || OrderIndex > SpacecraftData.ShipyardOrderQueue.Num())
	{
		return;
	}

	FFlareShipyardOrderSave Order = SpacecraftData.ShipyardOrderQueue[OrderIndex];

	UFlareCompany* OtherCompany = GetGame()->GetGameWorld()->FindCompany(Order.Company);
	OtherCompany->GiveMoney(Order.AdvancePayment, FFlareTransactionLogEntry::LogCancelOrderShip(this, Order.ShipClass));


	if(Order.Company == Game->GetPC()->GetCompany()->GetIdentifier())
	{
		int32 GameDifficulty = -1;
		GameDifficulty = Game->GetPC()->GetPlayerData()->DifficultyId;
		int32 RepDivisor = 100000;
		switch (GameDifficulty)
		{
		case -1: // Easy
			RepDivisor = 110000;
			break;
		case 0: // Normal
			RepDivisor = 100000;
			break;
		case 1: // Hard
			RepDivisor = 125000;
			break;
		case 2: // Very Hard
			RepDivisor = 150000;
			break;
		case 3: // Expert
			RepDivisor = 200000;
			break;
		case 4: // Unfair
			RepDivisor = 250000;
			break;
		}
		GetCompany()->GivePlayerReputation(-Order.AdvancePayment / RepDivisor);
	}

	SpacecraftData.ShipyardOrderQueue.RemoveAt(OrderIndex);

	UpdateShipyardProduction();
}

TArray<FFlareShipyardOrderSave>& UFlareSimulatedSpacecraft::GetShipyardOrderQueue()
{
	return SpacecraftData.ShipyardOrderQueue;
}

TArray<FFlareShipyardOrderSave> UFlareSimulatedSpacecraft::GetOngoingProductionList()
{
	TArray<FFlareShipyardOrderSave> productionList;

	for (UFlareFactory* Factory : Factories)
	{
		if (Factory->IsShipyard() && Factory->GetTargetShipClass() != NAME_None)
		{
			FFlareShipyardOrderSave production;
			production.ShipClass = Factory->GetTargetShipClass();
			production.Company = Factory->GetTargetShipCompany();
			production.RemainingProductionDuration = Factory->GetRemainingProductionDuration();
			productionList.Add(production);
		}
	}
	return productionList;
}

FText UFlareSimulatedSpacecraft::GetNextShipyardOrderStatus()
{
	if(SpacecraftData.ShipyardOrderQueue.Num() == 0)
	{
		return FText(LOCTEXT("NextShipyardOrderStatusNone", "No ship order"));
	}

	FFlareShipyardOrderSave& NextOrder = SpacecraftData.ShipyardOrderQueue[0];

	UFlareFactory* Factory = GetCompatibleIdleShipyardFactory(NextOrder.ShipClass);

	if(!Factory)
	{
		return FText(LOCTEXT("NextShipyardOrderStatusWaitFactory", "Waiting for an available production line"));
	}

	const FFlareProductionData& ProductionData = GetCycleDataForShipClass(NextOrder.ShipClass);

	FText MissingResources;
	int32 MissingResourcesCount = 0;

	for(const FFlareFactoryResource& InputResource : ProductionData.InputResources)
	{
		int32 AvailableQuantity = GetActiveCargoBay()->GetResourceQuantity(&InputResource.Resource->Data, GetCompany());
		if(InputResource.Quantity > AvailableQuantity)
		{
			// Not enought resource
			MissingResources = FText::Format(LOCTEXT("NextShipyardOrderStatusMissingResource", "{0}{1}{2} {3}"),
			MissingResources,
			MissingResources.IsEmpty() ? FText() : LOCTEXT("NextShipyardOrderStatusMissingResourceSeparator", ", "),
			FText::AsNumber(InputResource.Quantity - AvailableQuantity),
			InputResource.Resource->Data.Name);
			++MissingResourcesCount;
		}
	}

	if(MissingResourcesCount > 0)
	{
		return FText::Format(LOCTEXT("NextShipyardOrderStatusWaitResources", "Waiting for resources ({0})"), MissingResources);
	}
	else
	{
		return LOCTEXT("NextShipyardOrderStatusUnknownCause", "Construction starting soon");
	}
}

UFlareFactory* UFlareSimulatedSpacecraft::GetCompatibleIdleShipyardFactory(FName ShipIdentifier)
{
	FFlareSpacecraftDescription* Desc = GetGame()->GetSpacecraftCatalog()->Get(ShipIdentifier);

	for (UFlareFactory* Factory : Factories)
	{
		if((Factory->IsSmallShipyard() && Desc->Size == EFlarePartSize::S) ||
			(Factory->IsLargeShipyard() && Desc->Size == EFlarePartSize::L))
		{
			if(!Factory->IsActive())
			{
				return Factory;
			}
		}
	}
	return nullptr;
}

void UFlareSimulatedSpacecraft::UpdateShipyardProduction()
{
	if(IsComplexElement())
	{
		GetComplexMaster()->UpdateShipyardProduction();
		return;
	}

	TArray<int32> IndexToRemove;

	int32 Index = 0;
	for (FFlareShipyardOrderSave& Order : SpacecraftData.ShipyardOrderQueue)
	{
#if 0
		FLOGV("Order %s for %s", *Order.ShipClass.ToString(), *Order.Company.ToString())
#endif
		bool MissingResource = false;
		const FFlareProductionData& ProductionData = GetCycleDataForShipClass(Order.ShipClass);

		float ShipyardfabricationBonus = ShipyardfabricationBonus = GetCompany()->GetTechnologyBonus("shipyard-fabrication-bonus");
		float TotalResourceMargin = 1;
		if (ShipyardfabricationBonus)
		{
			TotalResourceMargin = FMath::Min(0.10f, TotalResourceMargin - ShipyardfabricationBonus);
		}

		for (const FFlareFactoryResource& InputResource : ProductionData.InputResources)
		{
			int32 AdjustedQuantity = InputResource.Quantity * TotalResourceMargin;
			if(AdjustedQuantity > GetActiveCargoBay()->GetResourceQuantity(&InputResource.Resource->Data, GetCompany()))
			{
				// Missing resources, stop all
				MissingResource = true;
				break;
			}
		}

		if(MissingResource)
		{
#if 0
			FLOG("- Missing resource");
#endif
			break;
		}

		UFlareFactory* Factory = GetCompatibleIdleShipyardFactory(Order.ShipClass);

		if(Factory)
		{
			FLOG("- Start");
			Factory->StartShipBuilding(Order);
			IndexToRemove.Add(Index);
		}
//		else
//		{
//			FLOG("- No factory");
//		}
		Index++;
	}

	for (int i = IndexToRemove.Num()-1 ; i >= 0; --i)
	{
		SpacecraftData.ShipyardOrderQueue.RemoveAt(IndexToRemove[i]);
	}
}

bool UFlareSimulatedSpacecraft::CanOrder(const FFlareSpacecraftDescription* ShipDescription, UFlareCompany* OrderCompany, bool LimitedQueues)
{
	if(!IsShipyard())
	{
		return false;
	}

	if(GetCompany()->GetWarState(OrderCompany) == EFlareHostility::Hostile)
	{
		// Not possible to buy ship to hostile company
		return false;
	}

	UFlareCompany* PlayerCompany = Game->GetPC()->GetCompany();

	if (OrderCompany != GetCompany())
	{
		if (!IsAllowExternalOrder())
		{
			return false;
		}

		//TODO: make it apply to the AI if they have logic for disabling/enabling their own ship configuration settings
		FFlareSpacecraftSave SpaceCraftData = GetData();
		if (GetCompany() == PlayerCompany && SpaceCraftData.ShipyardOrderExternalConfig.Find(ShipDescription->Identifier) == INDEX_NONE)
		{
			return false;
		}
	}

	if (SpacecraftDescription->IsDroneCarrier != ShipDescription->IsDroneShip)
	{
		return false;
	}

	if (ShipDescription->BuildableShip.Num())
	{
		if (!ShipDescription->BuildableShip.Contains(this->GetDescription()->Identifier))
		{
			return false;
		}
	}

	if (ShipDescription->BuyableCompany.Num() > 0)
	{
		//buyable company has something, check if we're allowed to buy this in the first place
		if ((OrderCompany == PlayerCompany && !ShipDescription->BuyableCompany.Contains(FName("PLAYER"))) || !ShipDescription->BuyableCompany.Contains(OrderCompany->GetDescription()->ShortName))
		{
			return false;
		}
	}

	if (ShipDescription->BuildableCompany.Num() > 0)
	{
		//buildable company has something, check if shipyard owning faction is allowed to build this
		if ((Company == PlayerCompany && !ShipDescription->BuildableCompany.Contains(FName("PLAYER"))) || !ShipDescription->BuildableCompany.Contains(Company->GetDescription()->ShortName))
		{
			return false;
		}
	}

	if (!GetCompany()->IsTechnologyUnlockedShip(ShipDescription))
	{
		return false;
	}

	if (LimitedQueues)
	{
		if (OrderCompany == PlayerCompany)
		{
			for (FFlareShipyardOrderSave& Order : SpacecraftData.ShipyardOrderQueue)
			{
				if (Order.Company == PlayerCompany->GetIdentifier())
				{
					FFlareSpacecraftDescription* OrderShip = GetGame()->GetSpacecraftCatalog()->Get(Order.ShipClass);

					if (OrderShip->Size == ShipDescription->Size)
					{
						return false;
					}
				}
			}
		}
		else if (SpacecraftData.ShipyardOrderQueue.Num() > 0)
		{
/*
			TArray<FFlareShipyardOrderSave> CurrentOrders = this->GetOngoingProductionList();
			if (CurrentOrders.Num() >= this->GetShipyardFactoriesCount())
			{
				return false;
			}
*/
			for (int i = SpacecraftData.ShipyardOrderQueue.Num() - 1; i >= 0; i--)
				{
					FFlareShipyardOrderSave& Order = SpacecraftData.ShipyardOrderQueue[i];
					FFlareSpacecraftDescription* OrderShip = GetGame()->GetSpacecraftCatalog()->Get(Order.ShipClass);
					if (OrderShip->Size == ShipDescription->Size)
					{
						if (Order.Company == PlayerCompany->GetIdentifier())
						{
							return true;
						}
						else
						{
							return false;
						}
					}
				}
		}
	}

	else if (SpacecraftData.ShipyardOrderQueue.Num() > 0)
	{
		int32 Index = 0;
		UFlareSpacecraftCatalog* SpacecraftCatalog = this->GetGame()->GetSpacecraftCatalog();

		for (FFlareShipyardOrderSave& Order : GetShipyardOrderQueue())
		{
			if (Index >= GetShipyardOrderQueue().Num())
			{
				return false;
			}

			FFlareShipyardOrderSave& Order = this->GetShipyardOrderQueue()[Index];
			FFlareSpacecraftDescription* OrderDescription = SpacecraftCatalog->Get(Order.ShipClass);

			if (!OrderDescription)
			{
				return false;
			}

			Index++;
		}
		return false;
	}

	return true;
}

bool UFlareSimulatedSpacecraft::IsShipyardMissingResources()
{
	bool MissingResource = false;
	if(SpacecraftData.ShipyardOrderQueue.Num() > 0)
	{
		const FFlareProductionData& ProductionData = GetCycleDataForShipClass(SpacecraftData.ShipyardOrderQueue[0].ShipClass);

		for (const FFlareFactoryResource& InputResource : ProductionData.InputResources)
		{
			if(InputResource.Quantity > GetActiveCargoBay()->GetResourceQuantity(&InputResource.Resource->Data, GetCompany()))
			{
				// Missing resources, stop all
				MissingResource = true;
				break;
			}
		}
	}

	return MissingResource;
}

const FFlareProductionData& UFlareSimulatedSpacecraft::GetCycleDataForShipClass(FName ShipIdentifier)
{
	return GetGame()->GetSpacecraftCatalog()->Get(ShipIdentifier)->CycleCost;
}

int32 UFlareSimulatedSpacecraft::GetShipProductionTime(FName ShipIdentifier)
{
	return GetCycleDataForShipClass(ShipIdentifier).ProductionTime;
}

int32 UFlareSimulatedSpacecraft::GetEstimatedQueueAndProductionDuration(FName ShipIdentifier, int32 OrderIndex)
{
	if(OrderIndex >= SpacecraftData.ShipyardOrderQueue.Num())
	{
		FLOGV("WARNING: invalid OrderIndex %d : ShipyardOrderQueue have %d elements", OrderIndex, SpacecraftData.ShipyardOrderQueue.Num());
		return 0;
	}

	FFlareSpacecraftDescription* Desc = GetGame()->GetSpacecraftCatalog()->Get(ShipIdentifier);

	int32 Duration = 0;

	TArray<int32> ShipyardSimulators;

	for (UFlareFactory* Factory : Factories)
	{
		if((Factory->IsSmallShipyard() && Desc->Size == EFlarePartSize::S) ||
			(Factory->IsLargeShipyard() && Desc->Size == EFlarePartSize::L))
		{
			if(Factory->IsActive())
			{
				ShipyardSimulators.Add(Factory->GetRemainingProductionDuration());
			}
			else
			{
				ShipyardSimulators.Add(0);
			}
		}
	}

	if(ShipyardSimulators.Num() == 0)
	{
		FLOG("WARNING: no shipyard simulator");
		return 0;
	}

	if(OrderIndex < 0)
	{
		OrderIndex = SpacecraftData.ShipyardOrderQueue.Num();
	}

	int32 NextOrderIndex = 0;

	while(true)
	{
		int32 MinFactoryDuration = INT32_MAX;
		// Find the minimum duration
		for (int32 ProductionDuration : ShipyardSimulators)
		{
			if(ProductionDuration < MinFactoryDuration)
			{
				MinFactoryDuration = ProductionDuration;
			}
		}

		Duration += MinFactoryDuration;

		// Remove this duration to shipyard
		for (int32& ProductionDuration : ShipyardSimulators)
		{
			ProductionDuration -= MinFactoryDuration;

			if (ProductionDuration <= 0)
			{
				// Try to queue
				if(NextOrderIndex == OrderIndex)
				{
					// Enqueue the target, return duration
					return  Duration;
				}

				FFlareShipyardOrderSave& Order = SpacecraftData.ShipyardOrderQueue[NextOrderIndex];
				int32 ShipProductionDuration = GetShipProductionTime(Order.ShipClass);
				ProductionDuration = ShipProductionDuration;
				NextOrderIndex++;
			}
		}
	}
}

FText UFlareSimulatedSpacecraft::GetShipCost(FName ShipIdentifier)
{
	FText ProductionCostText;
	FText CommaTextReference = UFlareGameTools::AddLeadingSpace(LOCTEXT("Comma", "+"));

	const FFlareProductionData& ProductionData = GetCycleDataForShipClass(ShipIdentifier);


	// Cycle cost in credits
	uint32 CycleProductionCost = ProductionData.ProductionCost;
	if (CycleProductionCost > 0)
	{
		ProductionCostText = FText::Format(LOCTEXT("ShipProductionCostFormat", "{0} credits"), FText::AsNumber(UFlareGameTools::DisplayMoney(CycleProductionCost)));
	}

	// Cycle cost in resources
	float ShipyardfabricationBonus = GetCompany()->GetTechnologyBonus("shipyard-fabrication-bonus");

	for (int ResourceIndex = 0; ResourceIndex < ProductionData.InputResources.Num(); ResourceIndex++)
	{
		FText CommaText = ProductionCostText.IsEmpty() ? FText() : CommaTextReference;
		const FFlareFactoryResource* FactoryResource = &ProductionData.InputResources[ResourceIndex];
		FCHECK(FactoryResource);
		int32 Quantity = FactoryResource->Quantity;
		if (ShipyardfabricationBonus)
		{
			Quantity *= (1 - ShipyardfabricationBonus);
		}

		ProductionCostText = FText::Format(LOCTEXT("ShipProductionResourcesFormat", "{0}{1} {2} {3}"),
		ProductionCostText, CommaText, FText::AsNumber(Quantity), FactoryResource->Resource->Data.Acronym);
	}
	return ProductionCostText;
}

FText UFlareSimulatedSpacecraft::GetShipResourceCost(FFlareSpacecraftDescription* Description)
{
	FText ProductionCostText;
	FText CommaTextReference = UFlareGameTools::AddLeadingSpace(LOCTEXT("Comma", "+"));

	const FFlareProductionData& ProductionData = Description->CycleCost;
	// Cycle cost in resources
	float ShipyardfabricationBonus = GetCompany()->GetTechnologyBonus("shipyard-fabrication-bonus");

	for (int ResourceIndex = 0; ResourceIndex < ProductionData.InputResources.Num(); ResourceIndex++)
	{
		const FFlareFactoryResource* FactoryResource = &ProductionData.InputResources[ResourceIndex];
		FCHECK(FactoryResource);
		int32 Quantity = FactoryResource->Quantity;
		if (ShipyardfabricationBonus)
		{
			Quantity *= (1 - ShipyardfabricationBonus);
		}

		int32 StorageQuanty = GetActiveCargoBay()->GetResourceQuantitySimple(&FactoryResource->Resource->Data);

		if (StorageQuanty < Quantity)
		{
			ProductionCostText = FText::Format(LOCTEXT("ShipProductionResourcesFormat", "{0} \u2715{1}/{2} {3}"),
			ProductionCostText, FText::AsNumber(StorageQuanty), FText::AsNumber(Quantity), FactoryResource->Resource->Data.Acronym);
		}
		else
		{
			ProductionCostText = FText::Format(LOCTEXT("ShipProductionResourcesFormat", "{0} \u2022{1} {2}"),
			ProductionCostText, FText::AsNumber(Quantity), FactoryResource->Resource->Data.Acronym);
		}
	}

	return ProductionCostText;
}

bool UFlareSimulatedSpacecraft::IsAllowExternalOrder()
{
	return SpacecraftData.AllowExternalOrder;
}

bool UFlareSimulatedSpacecraft::IsAllowAutoConstruction()
{
	return SpacecraftData.AllowAutoConstruction;
}

bool UFlareSimulatedSpacecraft::IsInExternalOrdersArray(const FName ShipDescription)
{
	if (SpacecraftData.ShipyardOrderExternalConfig.Find(ShipDescription) != INDEX_NONE)
	{
		return true;
	}
	return false;
}

bool UFlareSimulatedSpacecraft::IsInExternalOrdersArray(const FFlareSpacecraftDescription* ShipDescription)
{
	if (SpacecraftData.ShipyardOrderExternalConfig.Find(ShipDescription->Identifier) != INDEX_NONE)
	{
		return true;
	}
	return false;
}


void UFlareSimulatedSpacecraft::SetAllowAutoConstruction(bool Allow)
{
	SpacecraftData.AllowAutoConstruction = Allow;
}

void UFlareSimulatedSpacecraft::SetAllowExternalOrder(bool Allow)
{
	SpacecraftData.AllowExternalOrder = Allow;

	if(!Allow)
	{
		// Cancel others companies order
		bool ok = false;
		while(!ok)
		{
			ok = true;
			int Index = 0;
			for (FFlareShipyardOrderSave& Order : GetShipyardOrderQueue())
			{
				if(Order.Company != GetCompany()->GetIdentifier())
				{
					CancelShipyardOrder(Index);
					ok = false;
					break;
				}
				Index++;
			}
		}
	}
}

void UFlareSimulatedSpacecraft::AddRemoveExternalOrderArray(const FName ShipDescription)
{

	if (SpacecraftData.ShipyardOrderExternalConfig.Find(ShipDescription) != INDEX_NONE)
	{
		SpacecraftData.ShipyardOrderExternalConfig.RemoveSwap(ShipDescription);
		// Cancel others companies order
		bool ok = false;
		while (!ok)
		{
			ok = true;
			int Index = 0;
			for (FFlareShipyardOrderSave& Order : GetShipyardOrderQueue())
			{
				if (Order.Company != GetCompany()->GetIdentifier())
				{
					if (ShipDescription == Order.ShipClass)
					{
						CancelShipyardOrder(Index);
						ok = false;
						break;
					}
				}
				Index++;
			}
		}
	}
	else
	{
		SpacecraftData.ShipyardOrderExternalConfig.Add(ShipDescription);

	
/*
		Game->GetPC()->Notify(LOCTEXT("TradingStateEnd", "Trading complete"),
		FText::Format(LOCTEXT("TravelEndedFormat", "Add space {0} vs {1}"),
		OldLen,
		SpacecraftData.ShipyardOrderExternalConfig.Num()),
		FName("trading-state-end"));
*/
	}
}

const FFlareProductionData* UFlareSimulatedSpacecraft::GetNextOrderShipProductionData(EFlarePartSize::Type Size)
{
	for (FFlareShipyardOrderSave& Order : SpacecraftData.ShipyardOrderQueue)
	{
		FFlareSpacecraftDescription* Desc = GetGame()->GetSpacecraftCatalog()->Get(Order.ShipClass);

		if (Desc->Size == Size)
		{
			return &GetCycleDataForShipClass(Order.ShipClass);
		}
	}
	return NULL;
}

int64 UFlareSimulatedSpacecraft::GetUpgradeCost(FFlareSpacecraftComponentDescription* NewPart, FFlareSpacecraftComponentDescription* OldPart)
{
	return NewPart->Cost - OldPart->Cost;
}

bool UFlareSimulatedSpacecraft::CanUpgrade(EFlarePartType::Type Type)
{
	if(!GetCurrentSector()->CanUpgrade(GetCompany()))
	{
		return false;
	}

	bool CanBeChanged = false;

	switch(Type)
	{
		case EFlarePartType::RCS:
			CanBeChanged = (GetDamageSystem()->GetSubsystemHealth(EFlareSubsystem::SYS_RCS) >= 0.99f);
			break;
		case EFlarePartType::OrbitalEngine:
			CanBeChanged = (GetDamageSystem()->GetSubsystemHealth(EFlareSubsystem::SYS_Propulsion) >= 0.99f);
			break;
		case EFlarePartType::Weapon:
			CanBeChanged = (GetDamageSystem()->GetSubsystemHealth(EFlareSubsystem::SYS_WeaponAndAmmo) >= 0.99f);
			break;
	}
	return CanBeChanged;
}

bool UFlareSimulatedSpacecraft::HasCapability(EFlareSpacecraftCapability::Type Capability) const
{
	if(IsComplex())
	{
		for(UFlareSimulatedSpacecraft* Child : GetComplexChildren())
		{
			if(Child->HasCapability(Capability))
			{
				return true;
			}
		}
		return false;
	}
	else
	{
		return GetDescription()->Capabilities.Contains(Capability);
	}
}

EFlareHostility::Type UFlareSimulatedSpacecraft::GetPlayerWarState() const
{
	return GetCompany()->GetPlayerWarState();
}

TArray<UFlareFactory*>& UFlareSimulatedSpacecraft::GetFactories()
{
	return Factories;
}

TArray<UFlareFactory*> UFlareSimulatedSpacecraft::GetShipyardFactories()
{
	TArray<UFlareFactory*> Shipyards;

	for (UFlareFactory* Factory : GetFactories())
	{
		if (Factory->IsShipyard())
		{
			Shipyards.Add(Factory);
		}
	}
	return Shipyards;
}

int32 UFlareSimulatedSpacecraft::GetShipyardFactoriesCount()
{
	int32 Count = 0;
	for (UFlareFactory* Factory : GetFactories())
	{
		if (Factory->IsShipyard())
		{
			Count++;
		}
	}
	return Count;
}


UFlareCargoBay* UFlareSimulatedSpacecraft::GetActiveCargoBay() const
{
	if(IsComplexElement())
	{
		return GetComplexMaster()->GetActiveCargoBay();
	}
	else
	{
		if(IsUnderConstruction())
		{
			return GetConstructionCargoBay();
		}
		else
		{
			return GetProductionCargoBay();
		}
	}
}

UFlareSimulatedSpacecraft* UFlareSimulatedSpacecraft::GetComplexMaster() const
{
	return ComplexMaster;
}

TArray<UFlareSimulatedSpacecraft*> const& UFlareSimulatedSpacecraft::GetComplexChildren() const
{
	return ComplexChildren;
}

UFlareSimulatedSpacecraft* UFlareSimulatedSpacecraft::GetShipMaster() const
{
	return ShipMaster;
}

TArray<UFlareSimulatedSpacecraft*> UFlareSimulatedSpacecraft::GetShipChildren() const
{
	return ShipChildren;
}

void UFlareSimulatedSpacecraft::AddShipChildren(UFlareSimulatedSpacecraft* ShipToAdd, bool Removing)
{
	if (ShipToAdd)
	{
		if (Removing)
		{
			ShipChildren.Remove(ShipToAdd);
		}
		else
		{
			ShipChildren.AddUnique(ShipToAdd);
			ShipToAdd->SetShipMaster(this);
		}
	}
}

void UFlareSimulatedSpacecraft::SetShipMaster(UFlareSimulatedSpacecraft* NewMasterShip)
{
	if (NewMasterShip)
	{
		ShipMaster = NewMasterShip;
	}
}

int32 UFlareSimulatedSpacecraft::GetCapturePoint(UFlareCompany* OtherCompany) const
{
	if(SpacecraftData.CapturePoints.Contains(OtherCompany->GetIdentifier()))
	{
		return SpacecraftData.CapturePoints[OtherCompany->GetIdentifier()];
	}
	return 0;
}

bool UFlareSimulatedSpacecraft::IsBeingCaptured() const
{
	return SpacecraftData.CapturePoints.Num() > 0;
}

int32 UFlareSimulatedSpacecraft::GetCapturePointThreshold() const
{
	int32 BaseCapturePoint = SpacecraftData.Level * SpacecraftDescription->CapturePointThreshold;

	if(IsComplex())
	{
		for(UFlareSimulatedSpacecraft* Child: GetComplexChildren())
		{
			BaseCapturePoint += Child->GetCapturePointThreshold();
		}
	}

	float DamageRatio = GetDamageSystem()->GetGlobalDamageRatio();
	float CaptureRatio = CAPTURE_THRESOLD_MIN;

	if (DamageRatio > CAPTURE_THRESOLD_MIN_DAMAGE)
	{
		// if Damage ratio == 1 , Capture ratio == 1
		// if Damage ratio == CAPTURE_THRESOLD_MIN_DAMAGE , Capture ratio == CAPTURE_THRESOLD_MIN
		float Coef = (CAPTURE_THRESOLD_MIN - CAPTURE_THRESOLD_MIN_DAMAGE) / (1.f - CAPTURE_THRESOLD_MIN_DAMAGE);
		CaptureRatio = (1.f - Coef) * DamageRatio + Coef;
	}

	return FMath::CeilToInt(BaseCapturePoint * CaptureRatio);
}

float UFlareSimulatedSpacecraft::GetDamageRatio()
{
	if (IsComplexElement())
	{
		return GetComplexMaster()->GetDamageRatio();
	}
	return GetDamageSystem()->GetGlobalHealth();
}

float UFlareSimulatedSpacecraft::GetStationEfficiency()
{
	if(IsComplexElement())
	{
		return GetComplexMaster()->GetStationEfficiency();
	}

	// if Damage ratio == 1 ,  efficiency == 1
	// if Damage ratio == STATION_DAMAGE_THRESOLD , efficiency = 0
	float DamageRatio = GetDamageSystem()->GetGlobalHealth();
	float Efficiency = 0.f;

	if (DamageRatio > STATION_DAMAGE_THRESOLD)
	{
		float Coef = -STATION_DAMAGE_THRESOLD / (1.f - STATION_DAMAGE_THRESOLD);
		Efficiency = (1.f - Coef) * DamageRatio + Coef;
	}

	if (IsStation())
	{
		if (!OwnerHasStationLicense)
		{
			//note: -1.f efficiency = 10X slower production
			Efficiency = Efficiency - 0.25f;
		}

		if (IsBeingCaptured())
		{
			Efficiency = Efficiency - 0.06f;
		}
		if (!Company->IsTechnologyUnlockedStation(GetDescription()))
		{
			Efficiency = Efficiency - 0.12f;
		}
	}

	if (CurrentFleet)
	{
		if (CurrentFleet->IsTraveling())
		{
		//carriers which are traveling build a little slower, no help from locals perhaps?
		Efficiency = Efficiency - 0.10f;
		}
	}
	return Efficiency;
}

void UFlareSimulatedSpacecraft::SetOwnerHasStationLicense(bool Setting)
{
	OwnerHasStationLicense = Setting;
}


int32 UFlareSimulatedSpacecraft::GetEquippedSalvagerCount()
{
	int32 Salvagers = 0;
	int32 WeaponGroupCount = GetDescription()->WeaponGroups.Num();
	for (int32 WeaponGroupIndex = 0; WeaponGroupIndex < WeaponGroupCount; WeaponGroupIndex++)
	{
		FFlareSpacecraftComponentDescription* CurrentWeapon = GetCurrentPart(EFlarePartType::Weapon, WeaponGroupIndex);
		if (CurrentWeapon->WeaponCharacteristics.DamageType == EFlareShellDamageType::LightSalvage || CurrentWeapon->WeaponCharacteristics.DamageType == EFlareShellDamageType::HeavySalvage)
		{
			Salvagers++;
		}
	}
	return Salvagers;
}


int32 UFlareSimulatedSpacecraft::GetCombatPoints(bool ReduceByDamage)
{
	if (!IsMilitary() || (ReduceByDamage && GetDamageSystem()->IsDisarmed()))
	{
		return 0;
	}

	int32 SpacecraftCombatPoints = GetDescription()->CombatPoints;

	int32 WeaponGroupCount = GetDescription()->WeaponGroups.Num();

	for(int32 WeaponGroupIndex = 0; WeaponGroupIndex < WeaponGroupCount; WeaponGroupIndex++)
	{
		FFlareSpacecraftComponentDescription* CurrentWeapon = GetCurrentPart(EFlarePartType::Weapon, WeaponGroupIndex);
		SpacecraftCombatPoints += CurrentWeapon->CombatPoints;
	}


	FFlareSpacecraftComponentDescription* CurrentPart = GetCurrentPart(EFlarePartType::RCS, 0);
	SpacecraftCombatPoints += CurrentPart->CombatPoints;

	CurrentPart = GetCurrentPart(EFlarePartType::OrbitalEngine, 0);
	SpacecraftCombatPoints += CurrentPart->CombatPoints;

	if(ReduceByDamage)
	{
		SpacecraftCombatPoints *= GetDamageSystem()->GetGlobalHealth();
	}

	return SpacecraftCombatPoints;
}

bool UFlareSimulatedSpacecraft::IsUnderConstruction(bool local)  const
{
	if(!local)
	{
		if(IsComplex())
		{
			for(UFlareSimulatedSpacecraft* Child : GetComplexChildren())
			{
				if(Child->IsUnderConstruction(true))
				{
					return true;
				}
			}
		}
		else if(IsComplexElement())
		{
			return GetComplexMaster()->IsUnderConstruction();
		}
	}

	return SpacecraftData.IsUnderConstruction;
}

bool UFlareSimulatedSpacecraft::IsPlayerShip()
{
	return (this == GetGame()->GetPC()->GetPlayerShip());
}

bool UFlareSimulatedSpacecraft::IsRepairing()
{
	return GetRepairStock() > 0 && GetDamageSystem()->GetGlobalDamageRatio() < 1.f;
}

bool UFlareSimulatedSpacecraft::IsRefilling()
{
	return GetRefillStock() > 0 && NeedRefill();
}

int32 UFlareSimulatedSpacecraft::GetRepairDuration()
{
	if(GetRepairStock() <= 0 || (GetCurrentSector() && GetCurrentSector()->IsInDangerousBattle(GetCompany())))
	{
		// No repair possible
		return 0;
	}

	UFlareSpacecraftComponentsCatalog* Catalog = GetGame()->GetShipPartsCatalog();

	float SpacecraftPreciseCurrentNeededFleetSupply = 0;

	// List components
	for (int32 ComponentIndex = 0; ComponentIndex < GetData().Components.Num(); ComponentIndex++)
	{
		FFlareSpacecraftComponentSave* ComponentData = &GetData().Components[ComponentIndex];
		FFlareSpacecraftComponentDescription* ComponentDescription = Catalog->Get(ComponentData->ComponentIdentifier);

		float DamageRatio = GetDamageSystem()->GetDamageRatio(ComponentDescription, ComponentData);
		float TechnologyBonus = GetCompany()->IsTechnologyUnlocked("quick-repair") ? 1.5f: 1.f;
		float TechnologyBonusSecondary = GetCompany()->GetTechnologyBonus("repair-bonus");
		TechnologyBonus += TechnologyBonusSecondary;

		float ComponentMaxRepairRatio = SectorHelper::GetComponentMaxRepairRatio(ComponentDescription) * (GetSize() == EFlarePartSize::L ? 0.2f : 1.f) * TechnologyBonus;
		float CurrentRepairRatio = FMath::Min(ComponentMaxRepairRatio, (1.f - DamageRatio));


		SpacecraftPreciseCurrentNeededFleetSupply += CurrentRepairRatio * UFlareSimulatedSpacecraftDamageSystem::GetRepairCost(ComponentDescription);
	}

	int32 ShipRepairDuration = 0;

	if(SpacecraftPreciseCurrentNeededFleetSupply != 0)
	{
		float MaxRepairRatio = FMath::Min(1.f, GetRepairStock() / SpacecraftPreciseCurrentNeededFleetSupply);

		for (int32 ComponentIndex = 0; ComponentIndex < GetData().Components.Num(); ComponentIndex++)
		{
			FFlareSpacecraftComponentSave* ComponentData = &GetData().Components[ComponentIndex];
			FFlareSpacecraftComponentDescription* ComponentDescription = Catalog->Get(ComponentData->ComponentIdentifier);

			float TechnologyBonus = GetCompany()->IsTechnologyUnlocked("quick-repair") ? 1.5f: 1.f;
			float TechnologyBonusSecondary = GetCompany()->GetTechnologyBonus("repair-bonus");
			TechnologyBonus += TechnologyBonusSecondary;

			float ComponentMaxRepairRatio = SectorHelper::GetComponentMaxRepairRatio(ComponentDescription) * (GetSize() == EFlarePartSize::L ? 0.2f : 1.f) * TechnologyBonus;
			float DamageRatio = GetDamageSystem()->GetDamageRatio(ComponentDescription, ComponentData);


			float RepairRatio = MaxRepairRatio * (1.f - DamageRatio);

			float PartRepairDurationFloat = RepairRatio/ComponentMaxRepairRatio;
			int32 PartRepairDuration = FMath::CeilToInt( FMath::RoundToFloat(PartRepairDurationFloat * 10) / 10);

			if(PartRepairDuration > ShipRepairDuration)
			{
				ShipRepairDuration = PartRepairDuration;
			}
		}
	}

	return ShipRepairDuration;
}

int32 UFlareSimulatedSpacecraft::GetRefillDuration()
{
	if(GetRefillStock() == 0 || (GetCurrentSector() && GetCurrentSector()->IsInDangerousBattle(GetCompany())))
	{
		// No refill possible
		return 0;
	}

	UFlareSpacecraftComponentsCatalog* Catalog = GetGame()->GetShipPartsCatalog();
	float SpacecraftPreciseCurrentNeededFleetSupply = 0;

	// List components
	for (int32 ComponentIndex = 0; ComponentIndex < GetData().Components.Num(); ComponentIndex++)
	{
		FFlareSpacecraftComponentSave* ComponentData = &GetData().Components[ComponentIndex];
		FFlareSpacecraftComponentDescription* ComponentDescription = Catalog->Get(ComponentData->ComponentIdentifier);
		if(ComponentDescription->Type == EFlarePartType::Weapon)
		{
			int32 MaxAmmo = ComponentDescription->WeaponCharacteristics.AmmoCapacity;
			int32 CurrentAmmo = MaxAmmo - ComponentData->Weapon.FiredAmmo;
			float FillRatio = (float) CurrentAmmo / (float) MaxAmmo;
			float CurrentRefillRatio = FMath::Min(MAX_REFILL_RATIO_BY_DAY * (GetSize() == EFlarePartSize::L ? 0.2f : 1.f), (1.f - FillRatio));

			SpacecraftPreciseCurrentNeededFleetSupply += CurrentRefillRatio * UFlareSimulatedSpacecraftDamageSystem::GetRefillCost(ComponentDescription);
		}

	}

	int32 ShipRefillDuration = 0;

	if(SpacecraftPreciseCurrentNeededFleetSupply != 0)
	{
		float MaxRefillRatio = FMath::Min(1.f,GetRefillStock() / SpacecraftPreciseCurrentNeededFleetSupply);

		for (int32 ComponentIndex = 0; ComponentIndex < GetData().Components.Num(); ComponentIndex++)
		{
			FFlareSpacecraftComponentSave* ComponentData = &GetData().Components[ComponentIndex];
			FFlareSpacecraftComponentDescription* ComponentDescription = Catalog->Get(ComponentData->ComponentIdentifier);

			if(ComponentDescription->Type == EFlarePartType::Weapon)
			{
				int32 MaxAmmo = ComponentDescription->WeaponCharacteristics.AmmoCapacity;
				int32 CurrentAmmo = MaxAmmo - ComponentData->Weapon.FiredAmmo;
				float FillRatio = (float) CurrentAmmo / (float) MaxAmmo;
				float RefillRatio = MaxRefillRatio * (1.f - FillRatio);


				int32 PartRefillDuration = FMath::CeilToInt(RefillRatio/(MAX_REFILL_RATIO_BY_DAY * (GetSize() == EFlarePartSize::L ? 0.2f : 1.f)));

				if(PartRefillDuration > ShipRefillDuration)
				{
					ShipRefillDuration = PartRefillDuration;
				}
			}
		}
	}

	return ShipRefillDuration;
}



bool UFlareSimulatedSpacecraft::IsLastPlayerShip()
{
	if(GetCurrentFleet() == GetGame()->GetPC()->GetPlayerFleet())
	{
		for(UFlareSimulatedSpacecraft* OtherSpacecraft : GetCurrentFleet()->GetShips())
		{
			if(OtherSpacecraft == this)
			{
				continue;
			}

			if(!OtherSpacecraft->GetDamageSystem()->IsUncontrollable())
			{
				return false;
			}
		}

		return true;
	}
	else
	{
		return false;
	}
}


bool UFlareSimulatedSpacecraft::IsResponsible(EFlareDamage::Type DamageType)
{
	UFlareSimulatedSpacecraft* PlayerShip = GetGame()->GetPC()->GetPlayerShip();

	if (this != PlayerShip)
	{
		return false;
	}

	if (PlayerShip->GetSize() == EFlarePartSize::S)
	{
		return true;
	}
	else
	{
		if (PlayerShip->IsActive())
		{
			if (PlayerShip->GetActive()->GetWeaponsSystem()->GetActiveWeaponType() == EFlareWeaponGroupType::WG_TURRET)
			{
				return true;
			}
			else
			{
				return (DamageType == EFlareDamage::DAM_Collision);
			}
		}
		else
		{
			return false;
		}
	}
}



const FSlateBrush* FFlareSpacecraftDescription::GetIcon(FFlareSpacecraftDescription* Characteristic)
{
	if (Characteristic)
	{
		if (Characteristic->IsStation())
		{
			return FFlareStyleSet::GetIcon("SS");
		}
		else if (Characteristic->IsMilitary())
		{
			if (Characteristic->Size == EFlarePartSize::S)
			{
				return FFlareStyleSet::GetIcon("MS");
			}
			else if (Characteristic->Size == EFlarePartSize::L)
			{
				return FFlareStyleSet::GetIcon("ML");
			}
		}
		else
		{
			if (Characteristic->Size == EFlarePartSize::S)
			{
				return FFlareStyleSet::GetIcon("CS");
			}
			else if (Characteristic->Size == EFlarePartSize::L)
			{
				return FFlareStyleSet::GetIcon("CL");
			}
		}
	}
	return NULL;
}

bool UFlareSimulatedSpacecraft::IsPlayerHostile() const
{
	UFlareCompany* PlayerCompany = Game->GetPC()->GetCompany();
	return IsHostile(PlayerCompany);
}

bool UFlareSimulatedSpacecraft::IsHostile(UFlareCompany* OtherCompany, bool UseCache) const
{
	if(GetCompany() == OtherCompany)
	{
		return false;
	}


	if(GetCompany()->IsPlayerCompany())
	{
		// Is player ship hostile ?

		if (IsMilitary() && Game->GetQuestManager()
				&& Game->GetQuestManager()->IsUnderMilitaryContract(GetCurrentSector(), OtherCompany, UseCache)
				&& !GetDamageSystem()->IsDisarmed() && !GetDamageSystem()->IsUncontrollable())
		{
			return true;
		}
	}
	else if (OtherCompany->IsPlayerCompany())
	{
		// Is non player ship hostile to player ?
		if(IsMilitary() && Game->GetQuestManager() &&
			Game->GetQuestManager()->IsUnderMilitaryContract(GetCurrentSector(), GetCompany(), UseCache))
		{
			return true;
		}

		if(Game->GetQuestManager() && Game->GetQuestManager()->IsMilitaryTarget(this, UseCache))
		{
			return true;
		}
	}

	return GetCompany()->GetWarState(OtherCompany) == EFlareHostility::Hostile;
}

#undef LOCTEXT_NAMESPACE
