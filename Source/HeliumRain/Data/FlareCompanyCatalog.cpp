
#include "FlareCompanyCatalog.h"
#include "../Flare.h"


/*----------------------------------------------------
	Constructor
----------------------------------------------------*/

UFlareCompanyCatalog::UFlareCompanyCatalog(const class FObjectInitializer& PCIP)
	: Super(PCIP)
{
}

const void UFlareCompanyCatalog::GetCompanyList(TArray<FFlareCompanyDescription*>& OutData)
{
	for (int32 i = 0; i < Companies.Num(); i++)
	{
		FFlareCompanyDescription& Candidate = Companies[i];
		OutData.Add(&Candidate);
	}
}