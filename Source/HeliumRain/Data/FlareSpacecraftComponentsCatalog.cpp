
#include "FlareSpacecraftComponentsCatalog.h"
#include "../Flare.h"
#include "../Player/FlarePlayerController.h"
#include "AssetRegistryModule.h"


/*----------------------------------------------------
	Constructor
----------------------------------------------------*/

static bool SortByWeaponType(const UFlareSpacecraftComponentsCatalogEntry& A, const UFlareSpacecraftComponentsCatalogEntry& B)
{
	if (A.Data.WeaponCharacteristics.BombCharacteristics.IsBomb && !B.Data.WeaponCharacteristics.BombCharacteristics.IsBomb)
	{
		return false;
	}
	else
	{
		return (A.Data.CombatPoints > B.Data.CombatPoints);
	}
}

static bool SortByCost(const UFlareSpacecraftComponentsCatalogEntry& A, const UFlareSpacecraftComponentsCatalogEntry& B)
{
	return (A.Data.Cost > B.Data.Cost);
}


UFlareSpacecraftComponentsCatalog::UFlareSpacecraftComponentsCatalog(const class FObjectInitializer& PCIP)
	: Super(PCIP)
{
	TArray<FAssetData> AssetList;
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	Registry.SearchAllAssets(true);
	Registry.GetAssetsByClass(UFlareSpacecraftComponentsCatalogEntry::StaticClass()->GetFName(), AssetList);

	for (int32 Index = 0; Index < AssetList.Num(); Index++)
	{
		//FLOGV("UFlareSpacecraftComponentsCatalog::UFlareSpacecraftComponentsCatalog : Found '%s'", *AssetList[Index].GetFullName());
		UFlareSpacecraftComponentsCatalogEntry* SpacecraftComponent = Cast<UFlareSpacecraftComponentsCatalogEntry>(AssetList[Index].GetAsset());
		FCHECK(SpacecraftComponent);

		UFlareSpacecraftComponentsCatalogEntry* OldEntry = NULL;

		if (SpacecraftComponent->Data.Type == EFlarePartType::OrbitalEngine)
		{
			for (UFlareSpacecraftComponentsCatalogEntry* EntrySub : EngineCatalog)
			{
				if (EntrySub->Data.Identifier == SpacecraftComponent->Data.Identifier)
				{
					OldEntry = EntrySub;
					break;
				}
			}

			if (OldEntry)
			{
				EngineCatalog.Remove(OldEntry);
			}

			EngineCatalog.Add(SpacecraftComponent);
		}

		else if (SpacecraftComponent->Data.Type == EFlarePartType::RCS)
		{
			for (UFlareSpacecraftComponentsCatalogEntry* EntrySub : RCSCatalog)
			{
				if (EntrySub->Data.Identifier == SpacecraftComponent->Data.Identifier)
				{
					OldEntry = EntrySub;
					break;
				}
			}

			if (OldEntry)
			{
				RCSCatalog.Remove(OldEntry);
			}

			RCSCatalog.Add(SpacecraftComponent);
		}

		else if (SpacecraftComponent->Data.Type == EFlarePartType::Weapon)
		{
			for (UFlareSpacecraftComponentsCatalogEntry* EntrySub : WeaponCatalog)
			{
				if (EntrySub->Data.Identifier == SpacecraftComponent->Data.Identifier)
				{
					OldEntry = EntrySub;
					break;
				}
			}

			if (OldEntry)
			{
				WeaponCatalog.Remove(OldEntry);
			}

			WeaponCatalog.Add(SpacecraftComponent);
		}

		else if (SpacecraftComponent->Data.Type == EFlarePartType::InternalComponent)
		{
			for (UFlareSpacecraftComponentsCatalogEntry* EntrySub : InternalComponentsCatalog)
			{
				if (EntrySub->Data.Identifier == SpacecraftComponent->Data.Identifier)
				{
					OldEntry = EntrySub;
					break;
				}
			}

			if (OldEntry)
			{
				InternalComponentsCatalog.Remove(OldEntry);
			}

			InternalComponentsCatalog.Add(SpacecraftComponent);
		}

		else if (SpacecraftComponent->Data.Type == EFlarePartType::Meta)
		{
			for (UFlareSpacecraftComponentsCatalogEntry* EntrySub : MetaCatalog)
			{
				if (EntrySub->Data.Identifier == SpacecraftComponent->Data.Identifier)
				{
					OldEntry = EntrySub;
					break;
				}
			}

			if (OldEntry)
			{
				MetaCatalog.Remove(OldEntry);
			}

			MetaCatalog.Add(SpacecraftComponent);
		}
	}

	// Sort entries for the upgrade menu
	EngineCatalog.Sort(SortByCost);
	RCSCatalog.Sort(SortByCost);
	WeaponCatalog.Sort(SortByWeaponType);
}


/*----------------------------------------------------
	Data getters
----------------------------------------------------*/

FFlareSpacecraftComponentDescription* UFlareSpacecraftComponentsCatalog::Get(FName Identifier) const
{
	FFlareSpacecraftComponentDescription* Part = NULL;

	auto FindByName = [=](const UFlareSpacecraftComponentsCatalogEntry* Candidate)
	{
		return Candidate && Candidate->Data.Identifier == Identifier;
	};

	UFlareSpacecraftComponentsCatalogEntry* const* Temp = EngineCatalog.FindByPredicate(FindByName);
	if (Temp)
	{
		Part = &(*Temp)->Data;
	}

	if (!Part)
	{
		Temp = RCSCatalog.FindByPredicate(FindByName);
		if (Temp)
		{
			Part = &(*Temp)->Data;
		}
	}

	if (!Part)
	{
		Temp = WeaponCatalog.FindByPredicate(FindByName);
		if (Temp)
		{
			Part = &(*Temp)->Data;
		}
	}

	if (!Part)
	{
		Temp = InternalComponentsCatalog.FindByPredicate(FindByName);
		if (Temp)
		{
			Part = &(*Temp)->Data;
		}
	}
	
	if (!Part)
	{
		Temp = MetaCatalog.FindByPredicate(FindByName);
		if (Temp)
		{
			Part = &(*Temp)->Data;
		}
	}

	return Part;
}

const void UFlareSpacecraftComponentsCatalog::GetEngineList(TArray<FFlareSpacecraftComponentDescription*>& OutData, TEnumAsByte<EFlarePartSize::Type> Size, UFlareCompany* FilterCompany, UFlareSimulatedSpacecraft* FilterShip)
{
	for (int32 i = 0; i < EngineCatalog.Num(); i++)
	{
		FFlareSpacecraftComponentDescription& Candidate = EngineCatalog[i]->Data;
		if (Candidate.Size == Size && (FilterCompany == NULL || FilterCompany->IsTechnologyUnlockedPart(&Candidate) && (FilterShip == NULL || !FilterCompany->IsPartRestricted(&Candidate, FilterShip))))
		{
			OutData.Add(&Candidate);
		}
	}
}

const void UFlareSpacecraftComponentsCatalog::GetEngineList(TArray<FFlareSpacecraftComponentDescription*>& OutData, TEnumAsByte<EFlarePartSize::Type> Size, UFlareCompany* FilterCompany, FName FilterShip)
{
	for (int32 i = 0; i < EngineCatalog.Num(); i++)
	{
		FFlareSpacecraftComponentDescription& Candidate = EngineCatalog[i]->Data;
		if (Candidate.Size == Size && (FilterCompany == NULL || FilterCompany->IsTechnologyUnlockedPart(&Candidate) && (FilterShip == NAME_None || !FilterCompany->IsPartRestricted(&Candidate, FilterShip))))
		{
			OutData.Add(&Candidate);
		}
	}
}

const void UFlareSpacecraftComponentsCatalog::GetRCSList(TArray<FFlareSpacecraftComponentDescription*>& OutData, TEnumAsByte<EFlarePartSize::Type> Size, UFlareCompany* FilterCompany, UFlareSimulatedSpacecraft* FilterShip)
{
	for (int32 i = 0; i < RCSCatalog.Num(); i++)
	{
		FFlareSpacecraftComponentDescription& Candidate = RCSCatalog[i]->Data;

		if (Candidate.Size == Size && (FilterCompany == NULL || FilterCompany->IsTechnologyUnlockedPart(&Candidate) && (FilterShip == NULL || !FilterCompany->IsPartRestricted(&Candidate, FilterShip))))
		{
			OutData.Add(&Candidate);
		}
	}
}

const void UFlareSpacecraftComponentsCatalog::GetRCSList(TArray<FFlareSpacecraftComponentDescription*>& OutData, TEnumAsByte<EFlarePartSize::Type> Size, UFlareCompany* FilterCompany, FName FilterShip)
{
	for (int32 i = 0; i < RCSCatalog.Num(); i++)
	{
		FFlareSpacecraftComponentDescription& Candidate = RCSCatalog[i]->Data;

		if (Candidate.Size == Size && (FilterCompany == NULL || FilterCompany->IsTechnologyUnlockedPart(&Candidate) && (FilterShip == NAME_None || !FilterCompany->IsPartRestricted(&Candidate, FilterShip))))
		{
			OutData.Add(&Candidate);
		}
	}
}

const void UFlareSpacecraftComponentsCatalog::GetWeaponList(TArray<FFlareSpacecraftComponentDescription*>& OutData, TEnumAsByte<EFlarePartSize::Type> Size, UFlareCompany* FilterCompany, UFlareSimulatedSpacecraft* FilterShip)
{
	for (int32 i = 0; i < WeaponCatalog.Num(); i++)
	{
		FFlareSpacecraftComponentDescription& Candidate = WeaponCatalog[i]->Data;

		if (Candidate.Size == Size && (FilterCompany == NULL || FilterCompany->IsTechnologyUnlockedPart(&Candidate) && (FilterShip == NULL || !FilterCompany->IsPartRestricted(&Candidate,FilterShip))))
		{
			OutData.Add(&Candidate);
		}
	}
}

const void UFlareSpacecraftComponentsCatalog::GetWeaponList(TArray<FFlareSpacecraftComponentDescription*>& OutData, TEnumAsByte<EFlarePartSize::Type> Size, UFlareCompany* FilterCompany, FName FilterShip)
{
	for (int32 i = 0; i < WeaponCatalog.Num(); i++)
	{
		FFlareSpacecraftComponentDescription& Candidate = WeaponCatalog[i]->Data;

		if (Candidate.Size == Size && (FilterCompany == NULL || FilterCompany->IsTechnologyUnlockedPart(&Candidate) && (FilterShip == NAME_None || !FilterCompany->IsPartRestricted(&Candidate, FilterShip))))
		{
			OutData.Add(&Candidate);
		}
	}
}