#pragma once

#include "../Flare.h"
#include "../Game/FlareCompany.h"
#include "Engine/DataAsset.h"
#include "FlareGlobalSettingsVisuals.h"
#include "FlareGlobalSettings.generated.h"


UCLASS()
class HELIUMRAIN_API UFlareGlobalSettings : public UDataAsset
{
	GENERATED_UCLASS_BODY()

public:

	/*----------------------------------------------------
		Public data
	-----------------------------5-----------------------*/

	UPROPERTY(EditAnywhere, Category = Content)
	UFlareGlobalSettingsVisuals* GlobalVisualSettings;

	UPROPERTY(EditAnywhere, Category = Content)
	UParticleSystem*                        VanillaImpactEffectTemplateS;

	UPROPERTY(EditAnywhere, Category = Content)
	UParticleSystem*                        VanillaImpactEffectTemplateL;

	void GetDefaultImpactTemplates(UParticleSystem*& ImpactEffectTemplateS, UParticleSystem*& ImpactEffectTemplateL);
};