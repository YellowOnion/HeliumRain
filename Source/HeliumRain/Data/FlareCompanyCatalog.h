#pragma once

#include "../Flare.h"
#include "../Game/FlareCompany.h"
#include "Engine/DataAsset.h"
#include "FlareCompanyCatalog.generated.h"


UCLASS()
class HELIUMRAIN_API UFlareCompanyCatalog : public UDataAsset
{
	GENERATED_UCLASS_BODY()

public:

	/*----------------------------------------------------
		Public data
	----------------------------------------------------*/
	
	const void GetCompanyList(TArray<FFlareCompanyDescription*>& OutData);

	/** Company data */
	UPROPERTY(EditAnywhere, Category = Content)
	TArray<FFlareCompanyDescription> Companies;

};
