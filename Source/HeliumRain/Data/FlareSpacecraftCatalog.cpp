
#include "FlareSpacecraftCatalog.h"
#include "../Flare.h"
#include "AssetRegistryModule.h"


/*----------------------------------------------------
	Constructor
----------------------------------------------------*/

struct FSortByEntrySize
{
	FORCEINLINE bool operator()(const UFlareSpacecraftCatalogEntry& EntryA, const UFlareSpacecraftCatalogEntry& EntryB) const
	{
		const FFlareSpacecraftDescription* A = &EntryA.Data;
		const FFlareSpacecraftDescription* B = &EntryB.Data;

		if (A->IsStation() && B->IsStation())
		{
			if (A->IsSubstation && !B->IsSubstation)
			{
				return true;
			}
			else if (!A->IsSubstation && B->IsSubstation)
			{
				return false;
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
		else if (A->IsMilitary() && B->IsMilitary())
		{
			return A->CombatPoints > B->CombatPoints;
		}
		else if (A->IsMilitary() && !B->IsMilitary())
		{
			return true;
		}
		else if (!A->IsMilitary() && B->IsMilitary())
		{
			return false;
		}
		else if (A->Mass > B->Mass)
		{
			return true;
		}
		else if (A->Mass < B->Mass)
		{
			return false;
		}

		return false;
	}
};

UFlareSpacecraftCatalog::UFlareSpacecraftCatalog(const class FObjectInitializer& PCIP)
	: Super(PCIP)
{
	TArray<FAssetData> AssetList;
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	Registry.SearchAllAssets(true);
	Registry.GetAssetsByClass(UFlareSpacecraftCatalogEntry::StaticClass()->GetFName(), AssetList);

	TArray<UFlareSpacecraftCatalogEntry> TemporaryChecking;
	for (int32 Index = 0; Index < AssetList.Num(); Index++)
	{
		//FLOGV("UFlareSpacecraftCatalog::UFlareSpacecraftCatalog : Found '%s'", *AssetList[Index].GetFullName());
		UFlareSpacecraftCatalogEntry* Spacecraft = Cast<UFlareSpacecraftCatalogEntry>(AssetList[Index].GetAsset());
		FCHECK(Spacecraft);

		UFlareSpacecraftCatalogEntry* OldEntry = NULL;
		if (Spacecraft->Data.IsStation())
		{
			for (UFlareSpacecraftCatalogEntry* ShipSub : StationCatalog)
			{
				if (ShipSub->Data.Identifier == Spacecraft->Data.Identifier && ShipSub->Data.ModLoadPriority <= Spacecraft->Data.ModLoadPriority)
				{
					OldEntry = ShipSub;
					break;
				}
			}

			if (OldEntry)
			{
				StationCatalog.Remove(OldEntry);
			}

			StationCatalog.Add(Spacecraft);
		}
		else
		{
			for (UFlareSpacecraftCatalogEntry* ShipSub : ShipCatalog)
			{
				if (ShipSub->Data.Identifier == Spacecraft->Data.Identifier)// && ShipSub->Data.ModLoadPriority <= Spacecraft->Data.ModLoadPriority)
				{
					OldEntry = ShipSub;
					break;
				}
			}	

			if (OldEntry)
			{
				if (Spacecraft->Data.IsDisabledOverrideStats)
				{
					FFlareSpacecraftDescription* OldEntryDesc = Get(OldEntry->Data.Identifier);
					if (OldEntryDesc)
					{
						OldEntryDesc->Name = Spacecraft->Data.Name;
						OldEntryDesc->ShortName = Spacecraft->Data.ShortName;
						OldEntryDesc->Description = Spacecraft->Data.Description;

						OldEntryDesc->Mass = Spacecraft->Data.Mass;
						OldEntryDesc->AngularMaxVelocity = Spacecraft->Data.AngularMaxVelocity;
						OldEntryDesc->LinearMaxVelocity = Spacecraft->Data.LinearMaxVelocity;
						OldEntryDesc->HeatCapacity = Spacecraft->Data.HeatCapacity;

						OldEntryDesc->CombatPoints = Spacecraft->Data.CombatPoints;
						OldEntryDesc->MaxLevel = Spacecraft->Data.MaxLevel;
						OldEntryDesc->CapturePointThreshold = Spacecraft->Data.CapturePointThreshold;

						OldEntryDesc->CycleCost.ProductionTime = Spacecraft->Data.CycleCost.ProductionTime;
						OldEntryDesc->CycleCost.ProductionCost = Spacecraft->Data.CycleCost.ProductionCost;

						OldEntryDesc->CycleCost.InputResources.Empty();
						for (FFlareFactoryResource FactoryResource : Spacecraft->Data.CycleCost.InputResources)
						{
							OldEntryDesc->CycleCost.InputResources.Add(FactoryResource);
						}

						OldEntryDesc->CycleCost.OutputResources.Empty();
						for (FFlareFactoryResource FactoryResource : Spacecraft->Data.CycleCost.OutputResources)
						{
							OldEntryDesc->CycleCost.OutputResources.Add(FactoryResource);
						}

						OldEntryDesc->CargoBayCapacity = Spacecraft->Data.CargoBayCapacity;
						OldEntryDesc->CargoBayCount = Spacecraft->Data.CargoBayCount;

						OldEntryDesc->DroneLaunchDelay = Spacecraft->Data.DroneLaunchDelay;
						OldEntryDesc->IsUncapturable = Spacecraft->Data.IsUncapturable;
						OldEntryDesc->DroneMaximum = Spacecraft->Data.DroneMaximum;
						OldEntryDesc->IsDroneCarrier = Spacecraft->Data.IsDroneCarrier;
						OldEntryDesc->IsDroneShip = Spacecraft->Data.IsDroneShip;
					}
				}
				else
				{
					ShipCatalog.Remove(OldEntry);
				}
			}

			if (!Spacecraft->Data.IsDisabled)
			{
				ShipCatalog.Add(Spacecraft);
			}
		}
	}
	StationCatalog.Sort(FSortByEntrySize());
	ShipCatalog.Sort(FSortByEntrySize());
}


/*----------------------------------------------------
	Data getters
----------------------------------------------------*/

const void UFlareSpacecraftCatalog::GetSpacecraftList(TArray<FFlareSpacecraftDescription*>& OutData)
{
	for (UFlareSpacecraftCatalogEntry* ShipSub : ShipCatalog)
	{
		FFlareSpacecraftDescription Candidate = ShipSub->Data;
		OutData.Add(&Candidate);
	}
}

FFlareSpacecraftDescription* UFlareSpacecraftCatalog::Get(FName Identifier) const
{
	auto FindByName = [=](const UFlareSpacecraftCatalogEntry* Candidate)
	{
		return Candidate->Data.Identifier == Identifier;
	};

	UFlareSpacecraftCatalogEntry* const* Entry = ShipCatalog.FindByPredicate(FindByName);
	if (Entry && *Entry)
	{
		return &((*Entry)->Data);
	}

	Entry = StationCatalog.FindByPredicate(FindByName);
	if (Entry && *Entry)
	{
		return &((*Entry)->Data);
	}

	return NULL;
}