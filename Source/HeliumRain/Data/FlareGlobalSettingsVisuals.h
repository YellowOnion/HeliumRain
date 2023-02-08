#pragma once

#include "../Flare.h"
#include "../Game/FlareGameTypes.h"
#include "Engine/DataAsset.h"
#include "FlareGlobalSettingsVisuals.generated.h"

UCLASS()
class HELIUMRAIN_API UFlareGlobalSettingsVisuals : public UDataAsset
{
	GENERATED_UCLASS_BODY()

public:

	/*----------------------------------------------------
		Public data
	----------------------------------------------------*/

	UPROPERTY(EditAnywhere, Category = Content)
	UParticleSystem*                        DefaultImpactEffectTemplateS;

	UPROPERTY(EditAnywhere, Category = Content)
	UParticleSystem*                        DefaultImpactEffectTemplateL;
};