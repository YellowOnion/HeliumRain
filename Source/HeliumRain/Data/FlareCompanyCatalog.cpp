
#include "FlareCompanyCatalog.h"
#include "../Flare.h"
#include "AssetRegistryModule.h"

/*----------------------------------------------------
	Constructor
----------------------------------------------------*/

UFlareCompanyCatalog::UFlareCompanyCatalog(const class FObjectInitializer& PCIP)
	: Super(PCIP)
{
	TArray<FAssetData> AssetList;
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	Registry.SearchAllAssets(true);
	Registry.GetAssetsByClass(UFlareCompanyCatalogEntry::StaticClass()->GetFName(), AssetList);

	for (int32 Index = 0; Index < AssetList.Num(); Index++)
	{
		FLOGV("UFlareCompanyCatalog::UFlareCompanyCatalog : Found '%s'", *AssetList[Index].GetFullName());

		UFlareCompanyCatalogEntry* CompanyEntry = Cast<UFlareCompanyCatalogEntry>(AssetList[Index].GetAsset());
		FCHECK(CompanyEntry);

		UFlareCompanyCatalogEntry* OldEntry = NULL;
		for (UFlareCompanyCatalogEntry* CompanySub : CompanyCatalog)
		{
			if (CompanySub->Data.ShortName == CompanyEntry->Data.ShortName)
			{
				OldEntry = CompanySub;
				break;
			}
		}
		if (OldEntry)
		{
			CompanyCatalog.Remove(OldEntry);
		}
		CompanyCatalog.Add(CompanyEntry);
	}

}

const void UFlareCompanyCatalog::GetCompanyList(TArray<FFlareCompanyDescription*>& OutData)
{
	if (CompanyCatalog.Num())
	{
		for (UFlareCompanyCatalogEntry* CompanySub : CompanyCatalog)
		{
			FFlareCompanyDescription& Candidate = CompanySub->Data;
			OutData.Add(&Candidate);
		}
	}
	else
	{
		for (int32 i = 0; i < Companies.Num(); i++)
		{
			FFlareCompanyDescription& Candidate = Companies[i];
			OutData.Add(&Candidate);
		}
	}
}