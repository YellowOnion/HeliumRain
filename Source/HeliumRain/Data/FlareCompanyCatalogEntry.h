#pragma once

#include "../Flare.h"
#include "../Game/FlareGameTypes.h"
#include "Engine/DataAsset.h"
#include "FlareCompanyCatalogEntry.generated.h"

UCLASS()
class HELIUMRAIN_API UFlareCompanyCatalogEntry : public UDataAsset
{
	GENERATED_UCLASS_BODY()

public:

	/*----------------------------------------------------
		Public data
	----------------------------------------------------*/

	/** Company data */
	UPROPERTY(EditAnywhere, Category = Content)
	FFlareCompanyDescription Data;
};