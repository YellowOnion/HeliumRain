#include "FlareGlobalSettings.h"
#include "../Flare.h"
#include "AssetRegistryModule.h"

UFlareGlobalSettings::UFlareGlobalSettings(const class FObjectInitializer& PCIP)
	: Super(PCIP)
{
	TArray<FAssetData> AssetList;
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	Registry.SearchAllAssets(true);
	Registry.GetAssetsByClass(UFlareGlobalSettingsVisuals::StaticClass()->GetFName(), AssetList);


	for (int32 Index = 0; Index < AssetList.Num(); Index++)
	{
		UFlareGlobalSettingsVisuals* VisualSettings = Cast<UFlareGlobalSettingsVisuals>(AssetList[Index].GetAsset());
		if (VisualSettings)
		{
			GlobalVisualSettings = VisualSettings;
		}
	}
	static ConstructorHelpers::FObjectFinder<UParticleSystem> ImpactEffectTemplateSObj(TEXT("/Game/Master/Particles/DamageEffects/PS_Fire.PS_Fire"));
	static ConstructorHelpers::FObjectFinder<UParticleSystem> ImpactEffectTemplateLObj(TEXT("/Game/Master/Particles/DamageEffects/PS_Fire_L.PS_Fire_L"));
	VanillaImpactEffectTemplateS = ImpactEffectTemplateSObj.Object;
	VanillaImpactEffectTemplateL = ImpactEffectTemplateLObj.Object;
}

void UFlareGlobalSettings::GetDefaultImpactTemplates(UParticleSystem*& ImpactEffectTemplateS, UParticleSystem*& ImpactEffectTemplateL)
{
	if (GlobalVisualSettings && GlobalVisualSettings->DefaultImpactEffectTemplateS && GlobalVisualSettings->DefaultImpactEffectTemplateL)
	{
		ImpactEffectTemplateS = GlobalVisualSettings->DefaultImpactEffectTemplateS;
		ImpactEffectTemplateL = GlobalVisualSettings->DefaultImpactEffectTemplateL;
	}
	else
	{
		ImpactEffectTemplateS = VanillaImpactEffectTemplateS;
		ImpactEffectTemplateL = VanillaImpactEffectTemplateL;
	}
}