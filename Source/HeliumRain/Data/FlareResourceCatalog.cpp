
#include "FlareResourceCatalog.h"
#include "../Flare.h"
#include "AssetRegistryModule.h"


/*----------------------------------------------------
	Constructor
----------------------------------------------------*/

UFlareResourceCatalog::UFlareResourceCatalog(const class FObjectInitializer& PCIP)
	: Super(PCIP)
{
	TArray<FAssetData> AssetList;
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	Registry.SearchAllAssets(true);
	Registry.GetAssetsByClass(UFlareResourceCatalogEntry::StaticClass()->GetFName(), AssetList);

	for (int32 Index = 0; Index < AssetList.Num(); Index++)
	{
		//FLOGV("UFlareResourceCatalog::UFlareResourceCatalog : Found '%s'", *AssetList[Index].GetFullName());
		UFlareResourceCatalogEntry* Resource = Cast<UFlareResourceCatalogEntry>(AssetList[Index].GetAsset());
		FCHECK(Resource);

		UFlareResourceCatalogEntry* OldEntry = NULL;
		for (UFlareResourceCatalogEntry* ResourceSub : Resources)
		{
			if (ResourceSub->Data.Identifier == Resource->Data.Identifier)
			{
				OldEntry = ResourceSub;
				break;
			}
		}

		if (OldEntry)
		{
			Resources.Remove(OldEntry);
		}

		Resources.Add(Resource);

		if (Resource->Data.IsConsumerResource)
		{
			if (OldEntry)
			{
				ConsumerResources.Remove(OldEntry);
			}
			ConsumerResources.Add(Resource);
		}

		if (Resource->Data.IsMaintenanceResource)
		{
			if (OldEntry)
			{
				MaintenanceResources.Remove(OldEntry);
			}
			MaintenanceResources.Add(Resource);
		}
	}

	// Sort resources
	Resources.Sort(SortByResourceType);
	ConsumerResources.Sort(SortByResourceType);
	MaintenanceResources.Sort(SortByResourceType);
}


/*----------------------------------------------------
	Data getters
----------------------------------------------------*/

FFlareResourceDescription* UFlareResourceCatalog::Get(FName Identifier) const
{
	auto FindByName = [=](const UFlareResourceCatalogEntry* Candidate)
	{
		return Candidate->Data.Identifier == Identifier;
	};

	UFlareResourceCatalogEntry* const* Entry = Resources.FindByPredicate(FindByName);
	if (Entry && *Entry)
	{
		return &((*Entry)->Data);
	}

	return NULL;
}

UFlareResourceCatalogEntry* UFlareResourceCatalog::GetEntry(FFlareResourceDescription* Resource) const
{
	for (int32 ResourceIndex = 0; ResourceIndex < Resources.Num(); ResourceIndex++)
	{
		if(Resource == &Resources[ResourceIndex]->Data)
		{
			return Resources[ResourceIndex];
		}
	}
	return NULL;
}
