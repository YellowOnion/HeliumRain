#pragma once

#include "../Flare.h"
#include "../Game/FlareCompany.h"
#include "Engine/DataAsset.h"
#include "FlareStartingScenarioCatalogEntry.h"
#include "FlareStartingScenarioCatalog.generated.h"


UCLASS()
class HELIUMRAIN_API UFlareStartingScenarioCatalog : public UDataAsset
{
	GENERATED_UCLASS_BODY()

public:

	/*----------------------------------------------------
		Public data
	-----------------------------5-----------------------*/

	UPROPERTY(EditAnywhere, Category = Content)
	TArray<UFlareStartingScenarioCatalogEntry*> StartingScenarioCatalog;

	TArray<UFlareStartingScenarioCatalogEntry*> GetStartingScenarios();
};