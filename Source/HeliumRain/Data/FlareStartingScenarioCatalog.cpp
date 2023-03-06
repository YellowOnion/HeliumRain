
#include "FlareStartingScenarioCatalog.h"
#include "../Flare.h"
#include "AssetRegistryModule.h"

/*----------------------------------------------------
	Constructor
----------------------------------------------------*/

UFlareStartingScenarioCatalog::UFlareStartingScenarioCatalog(const class FObjectInitializer& PCIP)
	: Super(PCIP)
{
	TArray<FAssetData> AssetList;
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	Registry.SearchAllAssets(true);
	Registry.GetAssetsByClass(UFlareStartingScenarioCatalogEntry::StaticClass()->GetFName(), AssetList);
	for (int32 Index = 0; Index < AssetList.Num(); Index++)
	{
		FLOGV("UFlareCompanyCatalog::UFlareCompanyCatalog : Found '%s'", *AssetList[Index].GetFullName());

		UFlareStartingScenarioCatalogEntry* StartingScenarioEntry = Cast<UFlareStartingScenarioCatalogEntry>(AssetList[Index].GetAsset());
		FCHECK(StartingScenarioEntry);
		StartingScenarioCatalog.Add(StartingScenarioEntry);
	}
}

TArray<UFlareStartingScenarioCatalogEntry*> UFlareStartingScenarioCatalog::GetStartingScenarios()
{
	return StartingScenarioCatalog;
}