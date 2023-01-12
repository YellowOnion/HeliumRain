#pragma once

#include "FlareTechnologyCatalogEntry.h"
#include "FlareTechnologyCatalog.generated.h"


UCLASS()
class HELIUMRAIN_API UFlareTechnologyCatalog : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	/*----------------------------------------------------
		Public data
	----------------------------------------------------*/
	
	/** Tehcnologies */
	UPROPERTY(EditAnywhere, Category = Content)
	TArray<UFlareTechnologyCatalogEntry*> TechnologyCatalog;
	int MaximumTechnologyLevel;

public:

	/*----------------------------------------------------
		Public methods
	----------------------------------------------------*/
	
	/** Get a technology from identifier */
	FFlareTechnologyDescription* Get(FName Identifier) const;
	int GetMaxTechLevel() const;
};
